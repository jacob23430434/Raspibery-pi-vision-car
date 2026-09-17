#include <SPI.h>
#include "RF24.h"
#include <Wire.h>
RF24 rf24(9,10);         // CE, CSN
const byte addr[] = "1Node";
unsigned long lastRecvTime = 0;

char cmd;

//-------------------- 参数 --------------------//
int speed = 75;
unsigned int meter = 1000;
unsigned int mval;

// encoder
#define encoder_shift 0x80000000UL
unsigned long e1_raw, e2_raw, e3_raw, e4_raw;

//================================================
// 初始化
//================================================
void setup(){
  Serial.begin(9600);
  Wire.begin();
  Wire.setWireTimeout(25000, true);
  rf24.begin();
  rf24.setChannel(93);
  rf24.setPALevel(RF24_PA_MAX);
  rf24.setDataRate(RF24_2MBPS);

  rf24.openReadingPipe(1, addr);
  rf24.startListening();
  mval = meter * 1083;
  Serial.println("PS5 Bluetooth control ready");
  Serial.println("RX Ready (Test mode)");
}

void loop(){
  if(rf24.available()){
    rf24.read(&cmd, sizeof(cmd));
    Serial.print("Received: ");
    lastRecvTime = millis();
    Serial.println(cmd);
  }
  Serial.println("ok");
  if (cmd == 'F')
  {
    Serial.println("go forward");
    car_control(speed,speed);
  }
    if (cmd == 'B')
  {
    Serial.println("go forward");
    car_control1(speed,speed);
  }
   if (cmd == 'S')
  {
    Serial.println("go forward");
    car_control(0,0);
  }
     if (cmd == 'L')
  {
    Serial.println("go forward");
    car_turn();
  }
       if (cmd == 'R')
  {
    Serial.println("go forward");
    car_turn1();
  }
}



void car_stop(){
    unsigned int average = (cnt(e1_raw)+cnt(e2_raw)+cnt(e3_raw)+cnt(e4_raw))/4;
    unsigned int val = -(average-65555);
    if(val >= mval){
        //car_control(0,0);
    }
}

long cnt(unsigned long raw){
    return (long)raw - (long)encoder_shift;
}


void car_control(float a, float b){
  Wire.beginTransmission(42);
  Wire.write("baffff");

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
  delay(2);
}

void car_control1(float a, float b){
  Wire.beginTransmission(42);
  Wire.write("barrrr");

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
//================================================
// **保持原样 — turn 旋转**
//================================================
void car_turn(){
  Wire.beginTransmission(42);
  Wire.write("tr");

  for(int i=0;i<=3;i++){
    Wire.write(speed);
    Wire.write(0);
  }

  for(int i=0;i<=3;i++){
    Wire.write(256);
    Wire.write(100);
    Wire.write(0);
    Wire.write(0);
  }

  Wire.endTransmission();
  delay(1);
}
void car_turn1(){
  Wire.beginTransmission(42);
  Wire.write("tl");

  for(int i=0;i<=3;i++){
    Wire.write(speed);
    Wire.write(0);
  }

  for(int i=0;i<=3;i++){
    Wire.write(256);
    Wire.write(100);
    Wire.write(0);
    Wire.write(0);
  }

  Wire.endTransmission();
  delay(1);
}

