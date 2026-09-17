#include <Wire.h>

void setup() {
  unsigned char i = 0;
  pinMode(13, HIGH);
  Serial.begin(57600);    // 初始化串口通信
  Wire.begin();           // 初始化I2C总线（作为主机）
  Serial.println("Program started!");  
  delay(2000);
  digitalWrite(13, HIGH);

// -------加速--------

  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(70);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(30);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(85);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(35);
  Wire.write(0);
  Wire.endTransmission();
  delay(200);
  
  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(96);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(60);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(96);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(60);
  Wire.write(0);
  Wire.endTransmission();
  delay(200);

//--------全速-------

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
  delay(1350);
  


/*
  // --------减速---------
  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(10);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(10);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(0);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(0);
  Wire.write(0);
  Wire.endTransmission();
  delay(70);
  */

//      左转向

  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(30);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(30);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(0);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(0);
  Wire.write(0);
  Wire.endTransmission();
  delay(1300);

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
    // --------减速---------
  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(10);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(10);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(10);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(10);
  Wire.write(0);
  Wire.endTransmission();
  delay(70);


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
  Wire.write(50);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(50);
  Wire.write(0);
  Wire.endTransmission();
  delay(655);


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
  delay(700);


  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  // Motor1: 右前
  Wire.write(46);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
  // Motor2: 右后
  Wire.write(46);
  Wire.write(0);
  // Motor3: 左前
  Wire.write(40);
  Wire.write(0);  
  // Motor4: 左后
  Wire.write(40);
  Wire.write(0);
  Wire.endTransmission();
  delay(80);


  // 第二次传输：发送"Ha"
  Wire.beginTransmission(42);  // 再次开始与地址42的从机通信               // 等待2秒
  Wire.write("Ha");
  Wire.endTransmission();      // 结束第二次传输
  digitalWrite(13,LOW);
}

void loop() {
 
}