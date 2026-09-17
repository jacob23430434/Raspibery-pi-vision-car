#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial BT(10, 11); // RX, TX

//-------------------- 参数 --------------------//
int speed = 100;
unsigned int meter = 1000;
unsigned int mval;
int differ = 20;
// encoder
#define encoder_shift 0x80000000UL
unsigned long e1_raw, e2_raw, e3_raw, e4_raw;



//================================================
// 初始化
//================================================
void setup() {
    Wire.begin();
    BT.begin(9600);
    Serial.begin(115200);
    
    updateEnc(); // 初始化一次避免垃圾值

    mval = meter * 1083;
    Serial.println("PS5 Bluetooth control ready");
    car_control(0, 0);


}

//================================================
// 主循环
//================================================
void loop() {

    // 蓝牙指令优先级最高
    if (BT.available()) {
        char cmd = BT.read();
        Serial.print("CMD = ");
        Serial.println(cmd);

        switch (cmd) {
            case 'F':  // forward
                car_control(speed, speed);
                break;

            case 'B':  // backward
                car_control1(speed, speed);
                break;

            case 'L':  // turn left
                car_turn();   // ← 只用 car_turn
                break;

            case 'R':  // turn right
                car_turn1();   // ← 只用 car_turn
                break;

            case 'X':  // left
                car_control(speed,speed-40);
                break;
            case 'Y':  // right
                car_control(speed-40,speed);
                break;
            case 'S':  // stop
            default:
                car_control(0, 0);
                break;
        }
        
    }

    // 如果你要自动停用encoder，这里保留
    //updateEnc();
    //car_stop();
}

//================================================
// encoder auto stop
//================================================
void car_stop(){
    unsigned int average = (cnt(e1_raw)+cnt(e2_raw)+cnt(e3_raw)+cnt(e4_raw))/4;
    unsigned int val = -(average-65555);
    if(val >= mval){
        car_control(0,0);
    }
}

long cnt(unsigned long raw){
    return (long)raw - (long)encoder_shift;
}

//================================================
// encoder 读取
//================================================
void updateEnc(){
    // read e1 e2
    Wire.beginTransmission(42);
    Wire.write("i0");
    Wire.endTransmission();
    delay(1);

    Wire.requestFrom(42, 8);
    e1_raw = read32();
    e2_raw = read32();

    // read e3 e4
    Wire.beginTransmission(42);
    Wire.write("i5");
    Wire.endTransmission();
    delay(1);

    Wire.requestFrom(42, 8);
    e3_raw = read32();
    e4_raw = read32();
}

unsigned long read32(){
    unsigned long v = 0;
    v |= (unsigned long)Wire.read();
    v |= (unsigned long)Wire.read()<<8;
    v |= (unsigned long)Wire.read()<<16;
    v |= (unsigned long)Wire.read()<<24;
    return v;
}

//================================================
// **保持原样 — car_control**
//================================================
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
  delay(1);
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