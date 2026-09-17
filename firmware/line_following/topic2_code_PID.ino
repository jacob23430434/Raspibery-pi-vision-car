#include <SoftwareSerial.h>
#include <Wire.h>
// P 75 I6  D1 
// ================= 蓝牙 =================
SoftwareSerial BT(10, 11); // TX, RX

// ================= 速度设置 =================
int speed = 20;       // 基础直行速度（0-100）

float leftspeed, rightspeed;

// ================= PID 参数 =================
float Kp = 0.75;
float Ki = 0.06;
float Kd = 0.01;

float error = 0, lastError = 0, integral = 0, derivative = 0;
float pid_output = 0;

// ================= 蓝牙命令缓冲 =================
String bt_cmd = "";

// ================= 初始化 =================
void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(A6, INPUT);   // 左红外
  pinMode(A7, INPUT);   // 右红外

  Wire.begin();
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
void loop() {
  // 1. 蓝牙非阻塞调 PID
  readPIDfromBT();

  // 2. 读取红外并归一化
  float left  = analogRead(A6);
  float right = analogRead(A7);

  // 3. 计算误差
  error = left - right;

  // 4. PID 计算
  integral += error;
  integral = constrain(integral, -500, 500); // 积分限幅
  derivative = error - lastError;
  pid_output = Kp * error + Ki * integral + Kd * derivative;
  lastError = error;

  // 5. PID → 左右轮速度
  leftspeed  = speed - pid_output;
  rightspeed = speed + pid_output;

  leftspeed  = constrain(leftspeed, 0, 100);
  rightspeed = constrain(rightspeed, 0, 100);

  // 6. 调用电机控制
  car_control1(leftspeed, rightspeed);
  BT.println("PID_output");
  BT.println(pid_output);
  BT.println("leftspeed");
  BT.println(leftspeed);
  BT.println("rightspeed");
  BT.println(rightspeed);
  BT.println("Leftread");
  BT.println(left);
  BT.println("Rightread");
  BT.println(right);

  delay(100);
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
