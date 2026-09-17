#include <QTRSensors.h>
#include <Wire.h>

QTRSensors qtr;

const uint8_t SensorCount = 8;
uint16_t sensorValues[SensorCount];

// 最左 -10，最右 +10
const float W[SensorCount] = { -10, -7, -4, -1, 1, 4, 7, 10 };

// 丢线：没有任何一路 <= 400（即全部 > 400）
const uint16_t LINE_TH = 400;

// PID
float Kp = 15.0f, Ki = 0.0f, Kd = 0.5f;

// speed (0~100)
int baseSpeed = 80;
int maxSpeed  = 100;

// state
float holdError = 0.0f;
float lastError = 0.0f;
float Iterm     = 0.0f;

static inline int clampInt(int x, int lo, int hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

void setMotor(int a, int b)
{
  Wire.beginTransmission(42);
  Wire.write("baffff");

  // Motor1: 右前
  Wire.write((int)b); Wire.write(0);
  // Motor2: 右后
  Wire.write((int)b); Wire.write(0);
  // Motor3: 左前
  Wire.write((int)a); Wire.write(0);
  // Motor4: 左后
  Wire.write((int)a); Wire.write(0);

  Wire.endTransmission();
  delay(1);
}

void setup()
{
  Wire.begin();
  Serial.begin(9600);

  qtr.setTypeI2C(9, SensorCount);
  qtr.setEmitterPin(2);

  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  for (uint16_t i = 0; i < 100; i++) {
    qtr.calibrate();
    Serial.println(i);
  }
  digitalWrite(LED_BUILTIN, LOW);

  delay(1000);
}

void loop()
{
  qtr.readLineBlack(sensorValues); // 你当前：黑=低，白=高

  // ===== 1) 丢线判定：所有>400 才丢线 =====
  bool lost = true;
  for (uint8_t i = 0; i < SensorCount; i++) {
    if (sensorValues[i] <= LINE_TH) { // 看到“黑”（低值）就算在线
      lost = false;
      break;
    }
  }

  // ===== 2) 计算/保持 error（blackness = 1000 - value）=====
  float error = holdError;

  if (!lost) {
    float sum = 0.0f, sum_w = 0.0f;
    for (uint8_t i = 0; i < SensorCount; i++) {
      float black = 1000.0f - (float)sensorValues[i]; // 黑越大
      if (black < 0) black = 0;
      sum   += black;
      sum_w += black * W[i];
    }
    error = sum_w / sum;
    holdError = error;
  }

  // ===== 3) 丢线就停车；在线才 PID 跑 =====
  int left = 0, right = 0;

  if (lost) {
    // 丢线：停车 + 不更新 I/D（保持上次 holdError）
    left = 0;
    right = 0;
  } else {
    // PID
    float P = error;
    Iterm += error;
    float D = error - lastError;
    lastError = error;

    float turn = Kp * P + Ki * Iterm + Kd * D;

    // 差速（error>0 => 右转：右轮减速、左轮加速）
    left  = (int)(baseSpeed + turn);
    right = (int)(baseSpeed - turn);

    left  = clampInt(left,  0, maxSpeed);
    right = clampInt(right, 0, maxSpeed);
  }

  setMotor(left, right);

  // 输出：8路,error,left,right,lost
  for (uint8_t i = 0; i < SensorCount; i++) {
    Serial.print(sensorValues[i]);
    Serial.print(",");
  }
  Serial.print(error, 3); Serial.print(",");
  Serial.print(left);     Serial.print(",");
  Serial.print(right);    Serial.print(",");
  Serial.println(lost ? 1 : 0);

  delay(5);
}
