

/*

I2C 读取：readSensorData() 完整保持你原来的逻辑。

传感器滑动平均：每路传感器用 3 点滑动平均，平滑 ADC 抖动。

归一化 + 重心计算 error：保持原来的方法，error 代表线相对小车中心的偏差。

PID 控制：P、I、D 结构保持一致，可调参数。

*/





#include <Wire.h>
#include <SoftwareSerial.h>
#define N 8

// ================= 蓝牙 =================
SoftwareSerial BT(10, 11); // TX, RX

// ================= 蓝牙命令缓冲 =================
String bt_cmd = "";


// ===== I2C 传感器数据 =====
unsigned char dataRaw[16];
unsigned int sensorData[N];

// ===== 位置权重 =====
int weight[N] = {-35, -25, -15, -5, 5, 15, 25, 35};

// ===== 标定值（示例，需实测）=====
int white[N] = {472, 544, 529, 467, 431, 452, 500, 455};
int black[N] = {64, 75, 68, 56, 54, 59, 66, 62};

// ===== PID 参数 =====
float Kp = 0.8;
float Ki = 0.0;
float Kd = 0.0;

// ===== 电机参数 =====
int baseSpeed = 30;
int maxSpeed  = 100;

// ===== 全局变量 =====
float norm[N];
float error = 0;
float last_error = 0;
float I_term = 0;

// ===== 3 点滑动平均历史变量 =====
float S1[N] = {0};
float S2[N] = {0};
float S3[N] = {0};

void setup()
{
  BT.begin(9600);
  Wire.begin();              // I2C 主机
  Serial.begin(57600);

  // 启动时填充滑动平均历史
  readSensorData();
  for (int i = 0; i < N; i++)
  {
    S1[i] = S2[i] = S3[i] = sensorData[i];
  }
}

// ================= 蓝牙读取 PID =================
void readPIDfromBT() {
  while (BT.available()) {
    char c = BT.read();

    if (c == '\n' || c == '\r') {
      bt_cmd.trim();
      if (bt_cmd.length() >= 2) {
        char type = bt_cmd.charAt(0);
        float value = bt_cmd.substring(1).toFloat();
        bool updated = false;

        if (type == 'P') { Kp = value / 100.0; updated = true; }
        if (type == 'I') { Ki = value / 100.0; updated = true; }
        if (type == 'D') { Kd = value / 100.0; updated = true; }

        if (updated) {
          String out = "PID => P=" + String(Kp, 3) +
                       " I=" + String(Ki, 3) +
                       " D=" + String(Kd, 3);
          BT.println(out);
          Serial.println(out);
        }
      }
      bt_cmd = "";
    } else {
      bt_cmd += c;  // 累积命令
    }
  }
}

// ================= 主循环 =================
void loop()
{

  // 1. 蓝牙非阻塞调 PID
  readPIDfromBT();

  long sum = 0;
  long wsum = 0;

  // ===== 1. 读取 I2C 传感器 =====
  readSensorData();

  // ===== 2. 传感器层 3 点平均 =====
  for (int i = 0; i < N; i++)
  {
    // 滑动
    S3[i] = S2[i];
    S2[i] = S1[i];
    S1[i] = sensorData[i];

    // 平均值覆盖 sensorData
    sensorData[i] = (S1[i] + S2[i] + S3[i]) / 3.0;
  }

  // ===== 3. 幅值归一化 + 重心 error =====
  for (int i = 0; i < N; i++)
  {
    float adc = sensorData[i];

    // 幅值归一化
    norm[i] = (white[i] - adc) / (white[i] - black[i]);
    norm[i] = constrain(norm[i], 0.0, 1.0);

    sum  += norm[i] * 1000;
    wsum += weight[i] * norm[i] * 1000;
  }

  // ===== 4. 计算 error =====
  if (sum > 0)
    error = (float)wsum / sum;
  else
    error = last_error;   // 防丢线

  // ===== 5. PID =====
  float P = Kp * error;
  I_term += Ki * error;
  float D = Kd * (error - last_error);

  float output = P + I_term + D;
  last_error = error;

  // ===== 6. 转成左右轮速度 =====
  int leftSpeed  = baseSpeed + output;
  int rightSpeed = baseSpeed - output;

  leftSpeed  = constrain(leftSpeed,  0, maxSpeed);
  rightSpeed = constrain(rightSpeed, 0, maxSpeed);

  setMotor(leftSpeed, rightSpeed);

  // ===== 7. 调试输出 =====
  BT.print("error: ");
  BT.println(error);
  BT.println("PID ");
  BT.print(Kp);
  BT.print(Ki);
  BT.print(Kd);


  delay(5);
}

// ================= I2C 读传感器 =================
void readSensorData(void)
{
  unsigned char n = 0;

  Wire.requestFrom(9, 16);   // I2C 地址 0x09
  while (Wire.available())
  {
    if (n < 16)
      dataRaw[n++] = Wire.read();
    else
      Wire.read();            // 多余数据丢弃
  }

  // 拼成 10-bit 数据
  for (n = 0; n < N; n++)
  {
    sensorData[n] = (dataRaw[n * 2] << 2) + dataRaw[n * 2 + 1];
  }
}

// ================= 电机函数（你自己实现） =================
void setMotor(int a, int b)
{
 Wire.beginTransmission(42);
  Wire.write("sa");

  // Motor1: 右前
  Wire.write((int)b);
  Wire.write(0);

  // Motor2: 右后
  Wire.write((int)b);
  Wire.write(0);

  // Motor3: 左前
  Wire.write((int)a);
  Wire.write(0);

  // Motor4: 左后
  Wire.write((int)a);
  Wire.write(0);

  Wire.endTransmission();
  delay(1);
}


// ================= 电机控制（原样保留） =================
void car_control1(float a, float b) {
  Wire.beginTransmission(42);
  Wire.write("sa");

  // Motor1: 右前
  Wire.write((int)b);
  Wire.write(0);

  // Motor2: 右后
  Wire.write((int)b);
  Wire.write(0);

  // Motor3: 左前
  Wire.write((int)a);
  Wire.write(0);

  // Motor4: 左后
  Wire.write((int)a);
  Wire.write(0);

  Wire.endTransmission();
  delay(1);
}
