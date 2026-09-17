#include <Wire.h>

void setup() {
  unsigned char i = 0;
  pinMode(13, HIGH);
  Serial.begin(57600);    // 初始化串口通信
  Wire.begin();           // 初始化I2C总线（作为主机）
  Serial.println("Program started!");  




  delay(2000);
  digitalWrite(13, HIGH);

//--------全速-------

  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(100);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(100);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(100);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(100);
  Wire.write(0);
  Wire.endTransmission();
  delay(1550);
  


//      左转向

  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(100);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(100);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(0);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(0);
  Wire.write(0);
  Wire.endTransmission();
  delay(350);

  // 第一次传输：前进
  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(96);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(96);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(100);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(100);
  Wire.write(0);
  Wire.endTransmission();
  delay(300);

// --------右转-------
  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(0);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(0);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(90);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(90);
  Wire.write(0);
  Wire.endTransmission();
  delay(400);


  // 前进
  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(96);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(96);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(100);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(100);
  Wire.write(0);
  Wire.endTransmission();
  delay(750);




  // 第二次传输：发送"Ha"
  Wire.beginTransmission(42);  // 再次开始与地址42的从机通信               // 等待2秒
  Wire.write("Ha");
  Wire.endTransmission();      // 结束第二次传输
  digitalWrite(13,LOW);
}

void loop() {
 
}