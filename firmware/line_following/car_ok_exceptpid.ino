#include <QTRSensors.h>
#include <Wire.h>
#include <SoftwareSerial.h>
QTRSensors qtr;

const uint8_t SensorCount = 8;
uint16_t sensorValues[SensorCount];
SoftwareSerial BT(10, 11); // TX, RX

// ===== error 权重 =====
const float W[SensorCount] = { -10, -7, -4, -1, 1, 4, 7, 10 };

// ===== 丢线阈值：全部 > 400 =====
const uint16_t LINE_TH = 600;

// ===== PID =====
float Kp = 4.5f, Ki = 0.0f, Kd = 3.2f;

// ===== 速度 =====
int baseSpeed = 30;
int maxSpeed  = 100;

// ===== 转向参数（car_turn 用）=====
int speed = 8;   // 你原本用的转向速度
unsigned int i;

// ===== 状态 =====
float holdError = 0.0f;
float lastError = 0.0f;
float Iterm     = 0.0f;

bool wasLost = false;   // 防止反复触发 car_turn

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

  if (dir == 'r')
    Wire.write("tr");   // 右转
  else
    Wire.write("tl");   // 左转

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

static inline int clampInt(int x, int lo, int hi)
{
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

void setup()
{
  Wire.begin();
  Serial.begin(9600);
  BT.begin(9600);
  qtr.setTypeI2C(9, SensorCount);
  qtr.setEmitterPin(2);

  delay(500);
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);

  for (uint16_t k = 0; k < 50; k++)
  {
    qtr.calibrate();
    Serial.println(k);

  }
  digitalWrite(13, LOW);
  delay(1000);
}

void loop()
{
  qtr.readLineBlack(sensorValues);   // 黑=低，白=高

  // ===== 丢线判定：全部 > 400 =====
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
    // 只在“刚出线”的瞬间触发一次
    if (!wasLost)
    {
      if (holdError > 0)
        car_turn('l');   // 右侧出线
      else
        car_turn('r');   // 左侧出线
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

  int left  = (int)(baseSpeed + turn);
  int right = (int)(baseSpeed - turn);

  left  = clampInt(left,  0, maxSpeed);
  right = clampInt(right, 0, maxSpeed);

  setMotor(left, right);

  // ===== 串口 =====
  for (uint8_t k = 0; k < SensorCount; k++)
  {
    Serial.print(sensorValues[k]);
    Serial.print(",");
  }

  Serial.print(error, 3);
  Serial.print(",");
  Serial.println(lost ? 1 : 0);
  BT.print("left");
  BT.println(left);
  BT.print("Right");
  BT.println(right);
  BT.print(error, 3);
  BT.print(",");
  BT.println(lost ? 1 : 0);

  delay(5);
}
