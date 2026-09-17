#include <QTRSensors.h>
#include <Wire.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

QTRSensors qtr;
SoftwareSerial BT(10, 11); // HC-06: Arduino(10=TX->BT RX), (11=RX<-BT TX)

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

// ===== 出线转向差速强度（可调）=====
int turnSpeed = 25;  // 0~100, 越大回线越猛

// ===== 状态 =====
float holdError = 0.0f;
float lastError = 0.0f;
float Iterm     = 0.0f;
bool  wasLost   = false;

// 开机默认停
bool isRunning = false;

// 遥测控制
bool streamOn = true;
uint16_t streamPeriodMs = 50;  // 默认 50ms
uint32_t lastStreamMs = 0;

// ================= EEPROM 存储结构 =================
static const uint32_t EEPROM_MAGIC = 0x31525451UL; // 'QTR1'
static const uint16_t EEPROM_VER   = 1;
static const int EEPROM_ADDR       = 0;

struct EepromData {
  uint32_t magic;
  uint16_t ver;

  float Kp, Ki, Kd;
  uint16_t LINE_TH;
  int16_t baseSpeed;
  int16_t maxSpeed;
  int16_t turnSpeed;

  uint16_t calMin[SensorCount];
  uint16_t calMax[SensorCount];

  uint16_t checksum;
};

static inline uint16_t checksum16(const uint8_t* p, size_t n) {
  uint16_t s = 0;
  for (size_t k = 0; k < n; k++) s = (uint16_t)(s + p[k]);
  return s;
}

static inline int clampInt(int x, int lo, int hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}
static inline float clampFloat(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}
static inline float absf(float x) { return x < 0 ? -x : x; }

// ================= 电机控制（你原来的 I2C 协议保持）=================
void setMotor(int a, int b) {
  a = clampInt(a, 0, 100);
  b = clampInt(b, 0, 100);

  Wire.beginTransmission(42);
  Wire.write("baffff");

  Wire.write((int)b); Wire.write(0); // 右前
  Wire.write((int)b); Wire.write(0); // 右后
  Wire.write((int)a); Wire.write(0); // 左前
  Wire.write((int)a); Wire.write(0); // 左后

  Wire.endTransmission();
  delay(1);
}

// ================= 出线回线：差速转（更可控，不倒车）=================
void recoverTurn(float lastHoldError) {
  // lastHoldError > 0: 线在右边(车偏左)，需要向右转
  // lastHoldError < 0: 线在左边(车偏右)，需要向左转
  int ts = clampInt(turnSpeed, 0, 100);
  if (ts == 0) return;

  if (lastHoldError > 0) {
    // 向右转：左轮快，右轮慢
    int left = clampInt(ts, 0, 100);
    int right = clampInt(ts / 5, 0, 100); // 右轮几乎不动
    setMotor(left, right);
  } else {
    // 向左转：右轮快，左轮慢
    int right = clampInt(ts, 0, 100);
    int left = clampInt(ts / 5, 0, 100);
    setMotor(left, right);
  }
}

// ================== EEPROM：保存/读取 =================
bool loadFromEEPROM(bool applyCalibration) {
  EepromData d;
  EEPROM.get(EEPROM_ADDR, d);

  if (d.magic != EEPROM_MAGIC) return false;
  if (d.ver != EEPROM_VER) return false;

  uint16_t cs = d.checksum;
  d.checksum = 0;
  uint16_t calc = checksum16((const uint8_t*)&d, sizeof(EepromData));
  if (calc != cs) return false;

  Kp = d.Kp; Ki = d.Ki; Kd = d.Kd;
  LINE_TH = d.LINE_TH;
  baseSpeed = d.baseSpeed;
  maxSpeed  = d.maxSpeed;
  turnSpeed = d.turnSpeed;

  if (applyCalibration) {
    qtr.calibrate(); // 确保 calibrationOn 分配
    for (uint8_t k = 0; k < SensorCount; k++) {
      qtr.calibrationOn.minimum[k] = d.calMin[k];
      qtr.calibrationOn.maximum[k] = d.calMax[k];
    }
  }
  return true;
}

void saveToEEPROM() {
  EepromData d;
  d.magic = EEPROM_MAGIC;
  d.ver = EEPROM_VER;

  d.Kp = Kp; d.Ki = Ki; d.Kd = Kd;
  d.LINE_TH = LINE_TH;
  d.baseSpeed = (int16_t)baseSpeed;
  d.maxSpeed  = (int16_t)maxSpeed;
  d.turnSpeed = (int16_t)turnSpeed;

  qtr.calibrate(); // 确保 calibrationOn 有内存
  for (uint8_t k = 0; k < SensorCount; k++) {
    d.calMin[k] = qtr.calibrationOn.minimum[k];
    d.calMax[k] = qtr.calibrationOn.maximum[k];
  }

  d.checksum = 0;
  d.checksum = checksum16((const uint8_t*)&d, sizeof(EepromData));
  EEPROM.put(EEPROM_ADDR, d);
}

// ================== 校准：50次（保留 Serial.println(i)）=================
void doCalibration50() {
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  for (unsigned int i = 0; i < 50; i++) {
    qtr.calibrate();

    // 保留你要的：Serial.println(i)
    Serial.println(i);
    BT.println(i);

    delay(20);
  }

  digitalWrite(13, LOW);
}

// ================== 命令协议 ==================
// 一行一个命令（\n 结尾），例：
// RUN 1 / RUN 0
// SET KP 6.5
// SET KI 0
// SET KD 2.2
// SET BASE 70
// SET MAX 100
// SET LINE 600
// SET TURN 25
// CAL 50
// SAVE
// LOAD
// GET
// STREAM 1 / STREAM 0
// RATE 50   (ms)
String cmdLine;

void sendStatusOnce() {
  // 用一行 JSON，Python 解析更稳
  // {"type":"status","kp":...,"ki":...}
  BT.print("{\"type\":\"status\"");
  BT.print(",\"run\":");  BT.print(isRunning ? 1 : 0);
  BT.print(",\"kp\":");   BT.print(Kp, 4);
  BT.print(",\"ki\":");   BT.print(Ki, 4);
  BT.print(",\"kd\":");   BT.print(Kd, 4);
  BT.print(",\"base\":"); BT.print(baseSpeed);
  BT.print(",\"max\":");  BT.print(maxSpeed);
  BT.print(",\"line\":"); BT.print(LINE_TH);
  BT.print(",\"turn\":"); BT.print(turnSpeed);
  BT.print(",\"stream\":"); BT.print(streamOn ? 1 : 0);
  BT.print(",\"rate\":"); BT.print(streamPeriodMs);
  BT.println("}");
}

void ok(const char* msg) {
  BT.print("{\"type\":\"ok\",\"msg\":\""); BT.print(msg); BT.println("\"}");
}
void err(const char* msg) {
  BT.print("{\"type\":\"err\",\"msg\":\""); BT.print(msg); BT.println("\"}");
}

void handleCommand(const String& s) {
  String t = s;
  t.trim();
  if (t.length() == 0) return;

  // 统一大写前缀判断方便点
  String up = t;
  up.toUpperCase();

  if (up == "GET") {
    sendStatusOnce();
    return;
  }
  if (up == "SAVE") {
    saveToEEPROM();
    ok("saved");
    return;
  }
  if (up == "LOAD") {
    bool okk = loadFromEEPROM(true);
    if (okk) ok("loaded");
    else err("no_eeprom");
    sendStatusOnce();
    return;
  }
  if (up.startsWith("RUN ")) {
    int v = t.substring(4).toInt();
    isRunning = (v != 0);
    if (!isRunning) { setMotor(0, 0); Iterm = 0; }
    ok(isRunning ? "run=1" : "run=0");
    sendStatusOnce();
    return;
  }
  if (up.startsWith("STREAM ")) {
    int v = t.substring(7).toInt();
    streamOn = (v != 0);
    ok(streamOn ? "stream=1" : "stream=0");
    sendStatusOnce();
    return;
  }
  if (up.startsWith("RATE ")) {
    int v = t.substring(5).toInt();
    streamPeriodMs = (uint16_t)clampInt(v, 5, 1000);
    ok("rate_set");
    sendStatusOnce();
    return;
  }
  if (up.startsWith("CAL")) {
    // 只支持 CAL 50（你要的）
    doCalibration50();
    saveToEEPROM();
    ok("cal_done_saved");
    sendStatusOnce();
    return;
  }
  if (up.startsWith("SET ")) {
    // SET KEY VALUE
    int p1 = t.indexOf(' ');
    int p2 = t.indexOf(' ', p1 + 1);
    if (p2 < 0) { err("bad_set"); return; }
    String key = t.substring(p1 + 1, p2);
    String val = t.substring(p2 + 1);
    key.trim(); val.trim();
    String keyu = key; keyu.toUpperCase();

    if (keyu == "KP") {
      Kp = clampFloat(val.toFloat(), 0.0f, 200.0f);
      ok("kp_set");
    } else if (keyu == "KI") {
      Ki = clampFloat(val.toFloat(), 0.0f, 50.0f);
      ok("ki_set");
    } else if (keyu == "KD") {
      Kd = clampFloat(val.toFloat(), 0.0f, 200.0f);
      ok("kd_set");
    } else if (keyu == "BASE") {
      baseSpeed = clampInt(val.toInt(), 0, 100);
      ok("base_set");
    } else if (keyu == "MAX") {
      maxSpeed = clampInt(val.toInt(), 0, 100);
      ok("max_set");
    } else if (keyu == "LINE") {
      LINE_TH = (uint16_t)clampInt(val.toInt(), 0, 1000);
      ok("line_set");
    } else if (keyu == "TURN") {
      turnSpeed = clampInt(val.toInt(), 0, 100);
      ok("turn_set");
    } else {
      err("unknown_key");
      return;
    }
    sendStatusOnce();
    return;
  }

  err("unknown_cmd");
}

// ================== 遥测输出（JSON 每帧一行）=================
void sendTelemetry(uint16_t leftOut, uint16_t rightOut, float error, bool lost) {
  // {"type":"tele","t":12345,"s":[...8...],"err":0.123,"lost":0,"L":70,"R":65}
  BT.print("{\"type\":\"tele\",\"t\":");
  BT.print(millis());
  BT.print(",\"s\":[");
  for (uint8_t k = 0; k < SensorCount; k++) {
    BT.print(sensorValues[k]);
    if (k != SensorCount - 1) BT.print(",");
  }
  BT.print("],\"err\":"); BT.print(error, 4);
  BT.print(",\"lost\":");  BT.print(lost ? 1 : 0);
  BT.print(",\"L\":");     BT.print(leftOut);
  BT.print(",\"R\":");     BT.print(rightOut);
  BT.println("}");
}

// ================== setup ==================
void setup() {
  Wire.begin();
  Serial.begin(115200);
  BT.begin(9600);

  // QTR
  qtr.setTypeI2C(9, SensorCount);
  qtr.setEmitterPin(2);

  delay(300);

  // 尝试加载 EEPROM（参数 + 校准）
  bool hasEEP = loadFromEEPROM(true);

  isRunning = false;
  setMotor(0, 0);
  Iterm = 0;

  BT.println("{\"type\":\"boot\",\"msg\":\"ready\"}");
  BT.print("{\"type\":\"boot\",\"eeprom\":"); BT.print(hasEEP ? 1 : 0); BT.println("}");
  sendStatusOnce();
}

// ================== loop ==================
void loop() {
  // ---- 1) 读蓝牙命令 ----
  while (BT.available()) {
    char c = (char)BT.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleCommand(cmdLine);
      cmdLine = "";
    } else {
      if (cmdLine.length() < 120) cmdLine += c;
    }
  }

  if (!isRunning) {
    // 停车状态也允许 stream 状态/参数同步，但不跑控制
    if (streamOn && (millis() - lastStreamMs >= streamPeriodMs)) {
      lastStreamMs = millis();
      // 停车时 error/lost 输出 0，电机 0
      qtr.readLineBlack(sensorValues);
      sendTelemetry(0, 0, 0.0f, 0);
    }
    delay(2);
    return;
  }

  // ---- 2) 读取循迹 ----
  qtr.readLineBlack(sensorValues); // 黑=低 白=高

  bool lost = true;
  for (uint8_t k = 0; k < SensorCount; k++) {
    if (sensorValues[k] <= LINE_TH) { lost = false; break; }
  }

  float error = holdError;

  if (!lost) {
    float sum = 0, sum_w = 0;
    for (uint8_t k = 0; k < SensorCount; k++) {
      float black = 1000.0f - sensorValues[k];
      if (black < 0) black = 0;
      sum   += black;
      sum_w += black * W[k];
    }
    if (sum > 1e-6f) {
      error = sum_w / sum;
      holdError = error;
    }
  }

  // ---- 3) 出线回线逻辑 ----
  if (lost) {
    if (!wasLost) {
      recoverTurn(holdError);
    }
    wasLost = true;

    // 丢线也发遥测
    if (streamOn && (millis() - lastStreamMs >= streamPeriodMs)) {
      lastStreamMs = millis();
      sendTelemetry(0, 0, error, lost);
    }
    return;
  }
  wasLost = false;

  // ---- 4) PID 直行 ----
  float P = error;
  Iterm += error;
  float D = error - lastError;
  lastError = error;

  float turn = Kp * P + Ki * Iterm + Kd * D;

  // |error| 越大 base 越低（0~10 -> 100%~50%）
  float eabs = clampFloat(absf(error), 0.0f, 10.0f);
  float scale = 1.0f - 0.5f * (eabs / 10.0f);
  int baseNow = (int)(baseSpeed * scale);

  int left  = (int)(baseNow + turn);
  int right = (int)(baseNow - turn);

  left  = clampInt(left,  0, maxSpeed);
  right = clampInt(right, 0, maxSpeed);

  setMotor(left, right);

  // ---- 5) USB 串口镜像输出（你原来的 CSV 风格保留）----
  for (uint8_t k = 0; k < SensorCount; k++) {
    Serial.print(sensorValues[k]); Serial.print(",");
  }
  Serial.print(error, 3); Serial.print(",");
  Serial.println(lost ? 1 : 0);

  // ---- 6) 蓝牙遥测 ----
  if (streamOn && (millis() - lastStreamMs >= streamPeriodMs)) {
    lastStreamMs = millis();
    sendTelemetry((uint16_t)left, (uint16_t)right, error, lost);
  }

  delay(2);
}
