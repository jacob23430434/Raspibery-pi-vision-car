#include <QTRSensors.h>
#include <Wire.h>
#include <LiquidCrystal.h>

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

// ================== UI 状态 ==================
enum MenuPage {
  PAGE_KP = 0,
  PAGE_KI,
  PAGE_KD,
  PAGE_BASE,
  PAGE_MAX,
  PAGE_TURNSPEED,
  PAGE_LINETH,
  PAGE_COUNT
};

MenuPage page = PAGE_KP;
bool isRunning = true;

// encoder 解码
static uint8_t lastState = 0;
static int stepAcc = 0;

// LCD 刷新节流
unsigned long lcdLastUpdate = 0;

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

// ================= 出线转向 =================
void car_turn(char dir)   // dir = 'r' or 'l'
{
  Wire.beginTransmission(42);

  if (dir == 'r') Wire.write("tr");   // 右转
  else           Wire.write("tl");   // 左转

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

// ================== LCD 显示 ==================
void updateScreen()
{
  lcd.clear();

  // 第一行：Run/Stop + 当前页名
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
    default:             lcd.print("Menu"); break;
  }

  // 第二行：值
  lcd.setCursor(0, 1);
  lcd.print("V:");

  switch (page)
  {
    case PAGE_KP:
      lcd.print(Kp, 2);
      lcd.print("  step0.1");
      break;
    case PAGE_KI:
      lcd.print(Ki, 3);
      lcd.print(" step0.01");
      break;
    case PAGE_KD:
      lcd.print(Kd, 2);
      lcd.print("  step0.1");
      break;
    case PAGE_BASE:
      lcd.print(baseSpeed);
      lcd.print("   step1");
      break;
    case PAGE_MAX:
      lcd.print(maxSpeed);
      lcd.print("   step1");
      break;
    case PAGE_TURNSPEED:
      lcd.print(speed);
      lcd.print("   step1");
      break;
    case PAGE_LINETH:
      lcd.print(LINE_TH);
      lcd.print("  step10");
      break;
  }
}

// ================== 按键：短按翻页，长按 Run/Stop ==================
void handleButton()
{
  static bool lastBtn = HIGH;
  static unsigned long pressStart = 0;

  bool curBtn = digitalRead(BUTTON_PIN);

  if (lastBtn == HIGH && curBtn == LOW)
  {
    pressStart = millis();
  }

  if (lastBtn == LOW && curBtn == HIGH)
  {
    unsigned long t = millis() - pressStart;

    if (t < 500)
    {
      // 短按：切页
      page = (MenuPage)((page + 1) % PAGE_COUNT);
    }
    else
    {
      // 长按：Run/Stop
      isRunning = !isRunning;
      if (!isRunning)
      {
        setMotor(0, 0);
        Iterm = 0;
      }
    }

    // 立即刷新
    updateScreen();
    lcdLastUpdate = millis();
  }

  lastBtn = curBtn;
}

// ================== 旋钮：改当前页参数 ==================
void applyEncoderDelta(int dir) // dir: +1 or -1
{
  switch (page)
  {
    case PAGE_KP:
      Kp = clampFloat(Kp + (dir > 0 ? 0.10f : -0.10f), 0.0f, 200.0f);
      break;
    case PAGE_KI:
      Ki = clampFloat(Ki + (dir > 0 ? 0.01f : -0.01f), 0.0f, 50.0f);
      break;
    case PAGE_KD:
      Kd = clampFloat(Kd + (dir > 0 ? 0.10f : -0.10f), 0.0f, 200.0f);
      break;
    case PAGE_BASE:
      baseSpeed = clampInt(baseSpeed + (dir > 0 ? 1 : -1), 0, 100);
      break;
    case PAGE_MAX:
      maxSpeed = clampInt(maxSpeed + (dir > 0 ? 1 : -1), 0, 100);
      break;
    case PAGE_TURNSPEED:
      speed = clampInt(speed + (dir > 0 ? 1 : -1), 0, 100);
      break;
    case PAGE_LINETH:
      LINE_TH = (uint16_t)clampInt((int)LINE_TH + (dir > 0 ? 10 : -10), 0, 1000);
      break;
  }
}

void handleEncoder()
{
  // 2-bit state
  uint8_t cur = (digitalRead(PUSH2) << 1) | digitalRead(PUSH1);

  if (cur != lastState)
  {
    // 你之前项目那套判向逻辑
    if ((lastState == 0b00 && cur == 0b01) ||
        (lastState == 0b01 && cur == 0b11) ||
        (lastState == 0b11 && cur == 0b10) ||
        (lastState == 0b10 && cur == 0b00))
      stepAcc++;
    else
      stepAcc--;

    lastState = cur;

    // 只在卡位(00)时生效
    if (cur == 0b00)
    {
      if (stepAcc > 0) applyEncoderDelta(+1);
      else if (stepAcc < 0) applyEncoderDelta(-1);

      stepAcc = 0;

      // 节流刷新
      if (millis() - lcdLastUpdate >= 80)
      {
        updateScreen();
        lcdLastUpdate = millis();
      }
    }
  }
}

void setup()
{
  Wire.begin();
  Serial.begin(9600);

  // UI 引脚
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(PUSH1, INPUT_PULLUP);
  pinMode(PUSH2, INPUT_PULLUP);

  // LCD
  lcd.begin(16, 2);
  updateScreen();

  // QTR
  qtr.setTypeI2C(9, SensorCount);
  qtr.setEmitterPin(2);

  delay(500);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  // ===== alignment/calibrate：i 从 0..49 =====
  for (i = 0; i < 50; i++)
  {
    qtr.calibrate();
    Serial.println(i);   // 你要的 print i
  }

  digitalWrite(13, LOW);
  delay(1000);

  // encoder 初始状态
  lastState = (digitalRead(PUSH2) << 1) | digitalRead(PUSH1);
}

void loop()
{
  // UI 先跑（随时可调参/停机）
  handleButton();
  handleEncoder();

  if (!isRunning)
  {
    // Stop 状态不跑循迹
    delay(5);
    return;
  }

  // ================== 读取循迹 ==================
  qtr.readLineBlack(sensorValues);   // 黑=低，白=高

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
      if (holdError > 0) car_turn('l'); // 右侧出线
      else               car_turn('r'); // 左侧出线
    }
    wasLost = true;
    return;   // 本轮 loop 不再 PID
  }

  wasLost = false;

  // ================== PID 直行 ==================
  float P = error;
  Iterm += error;
  float D = error - lastError;
  lastError = error;

  float turn = Kp * P + Ki * Iterm + Kd * D;

  // ===== |error| 越大 base 越低（0~10 -> 100%~50%）=====
  float eabs = absf(error);
  eabs = clampFloat(eabs, 0.0f, 10.0f);

  float scale = 1.0f - 0.2f * (eabs / 10.0f);   // 0->1.0, 10->0.5
  int baseNow = (int)(baseSpeed * scale);

  int left  = (int)(baseNow + turn);
  int right = (int)(baseNow - turn);

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

  delay(5);
}
