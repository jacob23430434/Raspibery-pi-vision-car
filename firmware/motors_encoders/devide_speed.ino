#include <QTRSensors.h> 
#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
QTRSensors qtr;

// ===== LCD（按你之前项目）=====
const int rs = 12, en = 11, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// ===== Rotary + Button（按你之前项目）=====
#define BUTTON_PIN 9
#define PUSH1 3
#define PUSH2 10

// ================= 传感器 =================
const uint8_t SensorCount = 8;
uint16_t sensorValues[SensorCount];

// ===== error 权重 =====
const float W[SensorCount] = { -10, -7, -4, -1, 1, 4, 7, 10 };

// ===== 丢线阈值（可调）=====
uint16_t LINE_TH = 600;

// ===== PID（可调）=====
float Kp = 6.5f, Ki = 0.0f, Kd = 2.2f;

// ===== 速度（可调）=====
int baseSpeed = 70;
int maxSpeed  = 100;

// ===== 转向参数（可调）=====
int speed = 20;   // car_turn 里用
unsigned int i;

// ===== 状态 =====
float holdError = 0.0f;
float lastError = 0.0f;
float Iterm     = 0.0f;
bool wasLost = false;

// ===== 方案1：恢复期状态（关键新增：平滑加速）=====
bool inRecover = false;
unsigned long recoverStartMs = 0;   // 恢复期开始时间(ms)

// 恢复期“缓慢加速”参数（可调）
const float RECOVER_START_FRAC = 0.25f;     // 刚找回线时 base 上限=25%*baseSpeed
const unsigned long RECOVER_RAMP_MS = 700;  // 从 25% -> 100% 需要的时间(ms)

// ===== 新增：|D|过大 或 |error|过大 -> 强制慢速 =====
bool slowMode = false;

// ✅ 这些全部可调（菜单 + EEPROM）
int   SLOW_BASE  = 30;        // 慢速时 base 上限
float D_SLOW_ON  = 1.5f;      // |D| 触发阈值
float D_SLOW_OFF = 1.1f;      // |D| 退出阈值（迟滞）
float E_SLOW_ON  = 1.2f;      // |error| 触发阈值
float E_SLOW_OFF = 0.6f;      // |error| 退出阈值（迟滞）

// ===== base 平滑切换（新增：柔和渐变）=====
float baseOut = 0;                 
unsigned long baseLastMs = 0;
// 你可以调：下降可快一点，上升慢一点更稳
const float BASE_RAMP_UP_PER_S   = 120.0f; // 加速：每秒 +120
const float BASE_RAMP_DOWN_PER_S = 200.0f; // 减速：每秒 -200

// ===== turn 平滑切换（新增：柔和转向）=====
float turnOut = 0;                 
unsigned long turnLastMs = 0;
// 你可以调：越小越“柔”，越大越“跟手”
const float TURN_RAMP_PER_S = 800.0f; // 每秒 turn 最大变化量（单位=你的turn单位）

// ================== UI 状态 ==================
enum MenuPage {
  PAGE_KP = 0,
  PAGE_KI,
  PAGE_KD,
  PAGE_BASE,
  PAGE_MAX,
  PAGE_TURNSPEED,
  PAGE_LINETH,

  // ✅ 新增：慢速阈值/慢速速度
  PAGE_E_ON,
  PAGE_E_OFF,
  PAGE_D_ON,
  PAGE_D_OFF,
  PAGE_SLOWBASE,

  PAGE_COUNT
};

MenuPage page = PAGE_KP;

// 开机默认停
bool isRunning = false;

// encoder 解码
static uint8_t lastState = 0;
static int stepAcc = 0;

// LCD 刷新节流
unsigned long lcdLastUpdate = 0;

// ================= EEPROM 存储结构 =================
static const uint32_t EEPROM_MAGIC = 0x31525451UL; // 'QTR1'
static const uint16_t EEPROM_VER   = 2;            
static const int EEPROM_ADDR       = 0;

struct EepromData {
  uint32_t magic;
  uint16_t ver;

  // 可调参数
  float Kp, Ki, Kd;
  uint16_t LINE_TH;
  int16_t baseSpeed;
  int16_t maxSpeed;
  int16_t turnSpeed;

  // 慢速参数
  int16_t slowBase;
  float   dOn, dOff;
  float   eOn, eOff;

  // 校准数据（calibrationOn）
  uint16_t calMin[SensorCount];
  uint16_t calMax[SensorCount];

  // checksum
  uint16_t checksum;
};

static inline uint16_t checksum16(const uint8_t* p, size_t n) {
  uint16_t s = 0;
  for (size_t k = 0; k < n; k++) s = (uint16_t)(s + p[k]);
  return s;
}

// ================= 工具函数 =================
static inline int clampInt(int x, int lo, int hi)
{
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static inline float clampFloat(float x, float lo, float hi)
{
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

static inline float absf(float x) { return x < 0 ? -x : x; }

// base 平滑爬坡
int rampToTarget(int current, int target, float upPerS, float downPerS, float dtS)
{
  if (dtS <= 0) return target;

  int diff = target - current;
  if (diff == 0) return current;

  float maxStep = (diff > 0 ? upPerS : downPerS) * dtS;
  if (maxStep < 1.0f) maxStep = 1.0f;

  if (diff > 0) {
    int step = (int)maxStep;
    if (step > diff) step = diff;
    return current + step;
  } else {
    int step = (int)maxStep;
    if (step > -diff) step = -diff;
    return current - step;
  }
}

// turn 平滑爬坡（对称）
float rampToTargetF(float current, float target, float perS, float dtS)
{
  if (dtS <= 0) return target;
  float diff = target - current;
  float maxStep = perS * dtS;
  if (diff >  maxStep) diff =  maxStep;
  if (diff < -maxStep) diff = -maxStep;
  return current + diff;
}

// ================= 电机直行 =================
void setMotor(int a, int b)
{
  Wire.beginTransmission(42);
  Wire.write("baffff");

  Wire.write((int)b); Wire.write(0); // 右前
  Wire.write((int)b); Wire.write(0); // 右后
  Wire.write((int)a); Wire.write(0); // 左前
  Wire.write((int)a); Wire.write(0); // 左后

  Wire.endTransmission();
  delay(1);
}

// ================= 出线转向（保持不乱改） =================
void car_turn(char dir)   // dir = 'r' or 'l'
{
  Wire.beginTransmission(42);

  if (dir == 'r') Wire.write("tr");   
  else           Wire.write("tl");   

  for (i = 0; i <= 3; i++)
  {
    Wire.write(speed);
    Wire.write(0);
  }

  for (i = 0; i <= 3; i++)
  {
    Wire.write(256);
    Wire.write(100);
    Wire.write(0);
    Wire.write(0);
  }

  Wire.endTransmission();
  delay(1);
}

// ================== EEPROM：保存/读取 =================
bool loadFromEEPROM(bool applyCalibration)
{
  EepromData d;
  EEPROM.get(EEPROM_ADDR, d);

  if (d.magic != EEPROM_MAGIC) return false;
  if (!(d.ver == 1 || d.ver == EEPROM_VER)) return false;

  uint16_t cs = d.checksum;
  d.checksum = 0;
  uint16_t calc = checksum16((const uint8_t*)&d, sizeof(EepromData));
  if (calc != cs) return false;

  Kp = d.Kp; Ki = d.Ki; Kd = d.Kd;
  LINE_TH = d.LINE_TH;
  baseSpeed = d.baseSpeed;
  maxSpeed  = d.maxSpeed;
  speed     = d.turnSpeed;

  if (d.ver >= 2) {
    SLOW_BASE   = d.slowBase;
    D_SLOW_ON   = d.dOn;
    D_SLOW_OFF  = d.dOff;
    E_SLOW_ON   = d.eOn;
    E_SLOW_OFF  = d.eOff;
  }

  if (applyCalibration) {
    qtr.calibrate();
    for (uint8_t k = 0; k < SensorCount; k++) {
      qtr.calibrationOn.minimum[k] = d.calMin[k];
      qtr.calibrationOn.maximum[k] = d.calMax[k];
    }
  }

  return true;
}

void saveToEEPROM()
{
  EepromData d;
  d.magic = EEPROM_MAGIC;
  d.ver = EEPROM_VER;

  d.Kp = Kp; d.Ki = Ki; d.Kd = Kd;
  d.LINE_TH = LINE_TH;
  d.baseSpeed = (int16_t)baseSpeed;
  d.maxSpeed  = (int16_t)maxSpeed;
  d.turnSpeed = (int16_t)speed;

  d.slowBase = (int16_t)SLOW_BASE;
  d.dOn  = D_SLOW_ON;
  d.dOff = D_SLOW_OFF;
  d.eOn  = E_SLOW_ON;
  d.eOff = E_SLOW_OFF;

  qtr.calibrate();

  for (uint8_t k = 0; k < SensorCount; k++) {
    d.calMin[k] = qtr.calibrationOn.minimum[k];
    d.calMax[k] = qtr.calibrationOn.maximum[k];
  }

  d.checksum = 0;
  d.checksum = checksum16((const uint8_t*)&d, sizeof(EepromData));

  EEPROM.put(EEPROM_ADDR, d);
}

// ================== LCD 显示 ==================
void updateScreen()
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print(isRunning ? "Run " : "Stop");

  lcd.setCursor(5, 0);
  switch (page)
  {
    case PAGE_KP:        lcd.print("Kp"); break;
    case PAGE_KI:        lcd.print("Ki"); break;
    case PAGE_KD:        lcd.print("Kd"); break;
    case PAGE_BASE:      lcd.print("Base"); break;
    case PAGE_MAX:       lcd.print("Max"); break;
    case PAGE_TURNSPEED: lcd.print("TurnSpd"); break;
    case PAGE_LINETH:    lcd.print("LINE_TH"); break;

    case PAGE_E_ON:      lcd.print("E_ON"); break;
    case PAGE_E_OFF:     lcd.print("E_OFF"); break;
    case PAGE_D_ON:      lcd.print("D_ON"); break;
    case PAGE_D_OFF:     lcd.print("D_OFF"); break;
    case PAGE_SLOWBASE:  lcd.print("SLOW"); break;

    default:             lcd.print("Menu"); break;
  }

  lcd.setCursor(0, 1);
  lcd.print("V:");

  switch (page)
  {
    case PAGE_KP:        lcd.print(Kp, 2);  lcd.print("  step0.1");  break;
    case PAGE_KI:        lcd.print(Ki, 3);  lcd.print(" step0.01");  break;
    case PAGE_KD:        lcd.print(Kd, 2);  lcd.print("  step0.1");  break;
    case PAGE_BASE:      lcd.print(baseSpeed); lcd.print("   step1"); break;
    case PAGE_MAX:       lcd.print(maxSpeed);  lcd.print("   step1"); break;
    case PAGE_TURNSPEED: lcd.print(speed);     lcd.print("   step1"); break;
    case PAGE_LINETH:    lcd.print(LINE_TH);   lcd.print("  step10"); break;

    case PAGE_E_ON:      lcd.print(E_SLOW_ON, 2);  lcd.print(" step0.1"); break;
    case PAGE_E_OFF:     lcd.print(E_SLOW_OFF, 2); lcd.print(" step0.1"); break;
    case PAGE_D_ON:      lcd.print(D_SLOW_ON, 2);  lcd.print(" step0.1"); break;
    case PAGE_D_OFF:     lcd.print(D_SLOW_OFF, 2); lcd.print(" step0.1"); break;
    case PAGE_SLOWBASE:  lcd.print(SLOW_BASE);     lcd.print("   step1"); break;
  }
}

// ================== 按键：短按翻页，长按 Run/Stop（Stop时保存） ==================
void handleButton()
{
  static bool lastBtn = HIGH;
  static unsigned long pressStart = 0;

  bool curBtn = digitalRead(BUTTON_PIN);

  if (lastBtn == HIGH && curBtn == LOW) pressStart = millis();

  if (lastBtn == LOW && curBtn == HIGH)
  {
    unsigned long t = millis() - pressStart;

    if (t < 500)
    {
      page = (MenuPage)((page + 1) % PAGE_COUNT);
    }
    else
    {
      isRunning = !isRunning;
      if (!isRunning)
      {
        setMotor(0, 0);
        Iterm = 0;
        inRecover = false;
        slowMode = false;

        // ✅ 重置平滑状态
        baseLastMs = 0; baseOut = 0;
        turnLastMs = 0; turnOut = 0;

        saveToEEPROM();
      }
      else
      {
        // Run 初始化
        baseLastMs = 0;
        turnLastMs = 0;
      }
    }

    updateScreen();
    lcdLastUpdate = millis();
  }

  lastBtn = curBtn;
}

// ================== 旋钮：改当前页参数 ==================
void applyEncoderDelta(int dir)
{
  switch (page)
  {
    case PAGE_KP:        Kp = clampFloat(Kp + (dir>0?0.10f:-0.10f), 0.0f, 200.0f); break;
    case PAGE_KI:        Ki = clampFloat(Ki + (dir>0?0.01f:-0.01f), 0.0f, 50.0f);  break;
    case PAGE_KD:        Kd = clampFloat(Kd + (dir>0?0.10f:-0.10f), 0.0f, 200.0f); break;
    case PAGE_BASE:      baseSpeed = clampInt(baseSpeed + (dir>0?1:-1), 0, 100);    break;
    case PAGE_MAX:       maxSpeed  = clampInt(maxSpeed  + (dir>0?1:-1), 0, 100);    break;
    case PAGE_TURNSPEED: speed     = clampInt(speed     + (dir>0?1:-1), 0, 100);    break;
    case PAGE_LINETH:    LINE_TH = (uint16_t)clampInt((int)LINE_TH + (dir>0?10:-10), 0, 1000); break;

    case PAGE_E_ON:
      E_SLOW_ON = clampFloat(E_SLOW_ON + (dir>0?0.10f:-0.10f), 0.0f, 20.0f);
      if (E_SLOW_OFF > E_SLOW_ON) E_SLOW_OFF = E_SLOW_ON;
      break;
    case PAGE_E_OFF:
      E_SLOW_OFF = clampFloat(E_SLOW_OFF + (dir>0?0.10f:-0.10f), 0.0f, 20.0f);
      if (E_SLOW_OFF > E_SLOW_ON) E_SLOW_OFF = E_SLOW_ON;
      break;
    case PAGE_D_ON:
      D_SLOW_ON = clampFloat(D_SLOW_ON + (dir>0?0.10f:-0.10f), 0.0f, 20.0f);
      if (D_SLOW_OFF > D_SLOW_ON) D_SLOW_OFF = D_SLOW_ON;
      break;
    case PAGE_D_OFF:
      D_SLOW_OFF = clampFloat(D_SLOW_OFF + (dir>0?0.10f:-0.10f), 0.0f, 20.0f);
      if (D_SLOW_OFF > D_SLOW_ON) D_SLOW_OFF = D_SLOW_ON;
      break;
    case PAGE_SLOWBASE:
      SLOW_BASE = clampInt(SLOW_BASE + (dir>0?1:-1), 0, 100);
      break;
  }
}

void handleEncoder()
{
  uint8_t cur = (digitalRead(PUSH2) << 1) | digitalRead(PUSH1);

  if (cur != lastState)
  {
    if ((lastState == 0b00 && cur == 0b01) ||
        (lastState == 0b01 && cur == 0b11) ||
        (lastState == 0b11 && cur == 0b10) ||
        (lastState == 0b10 && cur == 0b00))
      stepAcc++;
    else
      stepAcc--;

    lastState = cur;

    if (cur == 0b00)
    {
      if (stepAcc > 0) applyEncoderDelta(+1);
      else if (stepAcc < 0) applyEncoderDelta(-1);

      stepAcc = 0;

      if (millis() - lcdLastUpdate >= 80)
      {
        updateScreen();
        lcdLastUpdate = millis();
      }
    }
  }
}

// ================== 开机校准选择 ==================
bool askBootCalibrate()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Boot: Calib?");
  lcd.setCursor(0, 1);
  lcd.print("S=Skip L=Yes");

  bool lastBtn = HIGH;
  unsigned long pressStart = 0;

  while (1)
  {
    bool curBtn = digitalRead(BUTTON_PIN);

    if (lastBtn == HIGH && curBtn == LOW) pressStart = millis();

    if (lastBtn == LOW && curBtn == HIGH)
    {
      unsigned long t = millis() - pressStart;
      if (t < 500) return false;
      else return true;
    }

    lastBtn = curBtn;
    delay(5);
  }
}

// ================== 50次校准：Serial.println(i) + LCD 显示 i ==================
void doCalibration50()
{
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating");

  for (i = 0; i < 50; i++)
  {
    qtr.calibrate();

    Serial.println(i);

    lcd.setCursor(0, 1);
    lcd.print("Step:");
    lcd.print(i);
    lcd.print("/49   ");

    delay(20);
  }

  digitalWrite(13, LOW);
}

// ================== setup ==================
void setup()
{
  Wire.begin();
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PUSH1, INPUT_PULLUP);
  pinMode(PUSH2, INPUT_PULLUP);

  lcd.begin(16, 2);

  qtr.setTypeI2C(9, SensorCount);
  qtr.setEmitterPin(2);

  delay(300);

  bool hasEEP = loadFromEEPROM(true);

  bool wantCalib = false;
  if (hasEEP) {
    wantCalib = askBootCalibrate();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No EEPROM data");
    lcd.setCursor(0, 1);
    lcd.print("Calibrating...");
    delay(800);
    wantCalib = true;
  }

  if (wantCalib)
  {
    doCalibration50();
    saveToEEPROM();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Calib Saved");
    lcd.setCursor(0, 1);
    lcd.print("EEPROM OK");
    delay(800);
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Use EEPROM Cal");
    lcd.setCursor(0, 1);
    lcd.print("Ready (Stop)");
    delay(800);
  }

  isRunning = false;
  setMotor(0, 0);
  Iterm = 0;
  inRecover = false;
  slowMode = false;

  baseLastMs = 0; baseOut = 0;
  turnLastMs = 0; turnOut = 0;

  updateScreen();

  lastState = (digitalRead(PUSH2) << 1) | digitalRead(PUSH1);
}

// ================== loop ==================
void loop()
{
  handleButton();
  handleEncoder();
  if (!isRunning)
  {
    delay(5);
    return;
  }

  // ================== 读取循迹 ==================
  qtr.readLineBlack(sensorValues);

  // ===== 丢线判定：全部 > LINE_TH =====
  bool lost = true;
  for (uint8_t k = 0; k < SensorCount; k++)
  {
    if (sensorValues[k] <= LINE_TH)
    {
      lost = false;
      break;
    }
  }

  // ===== 计算 / 保持 error =====
  float error = holdError;

  if (!lost)
  {
    float sum = 0, sum_w = 0;
    for (uint8_t k = 0; k < SensorCount; k++)
    {
      float black = 1000.0f - sensorValues[k];
      if (black < 0) black = 0;
      sum   += black;
      sum_w += black * W[k];
    }
    error = sum_w / sum;
    holdError = error;
  }

  // ================== 出线逻辑 ==================
  if (lost)
  {
    if (!wasLost)
    {
      if (holdError > 0) car_turn('l');
      else               car_turn('r');

      inRecover = true;
      recoverStartMs = millis();
      slowMode = true;
      Iterm = 0;
      lastError = holdError;

      // 出线回来，重新初始化平滑（避免跳）
      baseLastMs = 0;
      turnLastMs = 0;
    }
    wasLost = true;
    return;
  }

  wasLost = false;

  // ================== PID 直行 ==================
  float prevErr = lastError;
  float P = error;
  Iterm += error;
  float D = error - prevErr;
  lastError = error;

  float targetTurn = Kp * P + Ki * Iterm + Kd * D;

  // ===== |error| 越大 base 越低（0~10 -> 100%~50%）=====
  float eabs = absf(error);
  eabs = clampFloat(eabs, 0.0f, 10.0f);

  float scale = 1.0f - 0.5f * (eabs / 10.0f);
  int baseNow = (int)(baseSpeed * scale);

  // ===== |D| / |error| 慢速触发（阈值可调，带迟滞）=====
  float Dabs = absf(D);

  if (!slowMode)
  {
    if (Dabs > D_SLOW_ON || eabs > E_SLOW_ON) slowMode = true;
  }
  else
  {
    if (Dabs < D_SLOW_OFF && eabs < E_SLOW_OFF) slowMode = false;
  }

  if (slowMode)
  {
    if (baseNow > SLOW_BASE) baseNow = SLOW_BASE;
  }

  // ===== 恢复期“缓慢加速”（cap baseNow）=====
  if (inRecover)
  {
    unsigned long elapsed = millis() - recoverStartMs;

    if (elapsed >= RECOVER_RAMP_MS)
    {
      inRecover = false;
    }
    else
    {
      float t = (float)elapsed / (float)RECOVER_RAMP_MS;
      float capFrac = RECOVER_START_FRAC + (1.0f - RECOVER_START_FRAC) * t;
      int capBase = (int)(baseSpeed * capFrac);
      if (baseNow > capBase) baseNow = capBase;
    }
  }

  // ================== ✅ base 平滑渐变 ==================
  int targetBase = baseNow;

  unsigned long nowMs = millis();
  if (baseLastMs == 0) {
    baseLastMs = nowMs;
    baseOut = targetBase;
  }
  float dtBase = (nowMs - baseLastMs) / 1000.0f;
  baseLastMs = nowMs;

  int baseSmooth = rampToTarget((int)baseOut, targetBase,
                                BASE_RAMP_UP_PER_S, BASE_RAMP_DOWN_PER_S, dtBase);
  baseOut = baseSmooth;

  // ================== ✅ turn 平滑渐变 ==================
  // 让 turn 不会突然猛打方向
  if (turnLastMs == 0) {
    turnLastMs = nowMs;
    turnOut = targetTurn;
  }
  float dtTurn = (nowMs - turnLastMs) / 1000.0f;
  turnLastMs = nowMs;

  float turnSmooth = rampToTargetF(turnOut, targetTurn, TURN_RAMP_PER_S, dtTurn);
  turnOut = turnSmooth;

  // 额外保护：turn 太大时会导致一边被夹到 0，产生“顿挫”
  // 这里把 turn 限制在 baseSmooth 左右范围内（保守但顺）
  float turnLim = (float)baseSmooth;
  if (turnSmooth >  turnLim) turnSmooth =  turnLim;
  if (turnSmooth < -turnLim) turnSmooth = -turnLim;

  int left  = (int)(baseSmooth + turnSmooth);
  int right = (int)(baseSmooth - turnSmooth);

  left  = clampInt(left,  0, maxSpeed);
  right = clampInt(right, 0, maxSpeed);

  setMotor(left, right);

  // ===== 串口输出（保留你原有格式）=====
  for (uint8_t k = 0; k < SensorCount; k++)
  {
    Serial.print(sensorValues[k]);
    Serial.print(",");
  }

  Serial.print(error, 3);
  Serial.print(",");
  Serial.println(lost ? 1 : 0);
  Serial.print("ERRO");
  Serial.println(D);

  delay(5);
}
