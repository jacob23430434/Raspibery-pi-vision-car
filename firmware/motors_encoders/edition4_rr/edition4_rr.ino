#include <Wire.h>

void setup() {
  unsigned char i = 0;
  pinMode(13, HIGH);
  Serial.begin(57600);    // 初始化串口通信
  Wire.begin();           // 初始化I2C总线（作为主机）
  Serial.println("Program started!");  
  delay(2000);
  digitalWrite(13, HIGH);
//------------accelerate--------
  Wire.beginTransmission(42);  // begin to commuication with slave at 42 address
  Wire.write("sa");
  // Motor1: right front
  Wire.write(100);    // speed  
  // Motor2: right back
  Wire.write(100);   
  // Motor3: left front
  Wire.write(100);
  // Motor4: left back
  Wire.write(100);

  Wire.endTransmission();// end of transmission
  delay(1550);// the car will go 1550ms forward
// ----------turn left-----------
  Wire.beginTransmission(42);  // begin to commuication with slave at 42 address
  Wire.write("baffrr"); // set the direction of each whell, f representing forward, r representing backward
  // Motor1: right front
  Wire.write(100);    // speed  
  // Motor2: right back
  Wire.write(100);   
  // Motor3: left front
  Wire.write(100);
  // Motor4: left back
  Wire.write(100);
  Wire.endTransmission();
  delay(250);// the turning process will take 250ms

  // -------acceleration------
  Wire.beginTransmission(42);  
  Wire.write("baffff");
  // Motor1: right front
  Wire.write(96);    // speed  
  // Motor2: right back
  Wire.write(96);   
  // Motor3: left front
  Wire.write(100);
  // Motor4: left back
  Wire.write(100);
  Wire.endTransmission();
  delay(300); // the car will run forward 300ms to pass the 0.5m area
// --------右转-------
  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("barrff");
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
  delay(170);


  // 前进
  Wire.beginTransmission(42);  // 开始与地址42的从机通信

  Wire.write("baffff");
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