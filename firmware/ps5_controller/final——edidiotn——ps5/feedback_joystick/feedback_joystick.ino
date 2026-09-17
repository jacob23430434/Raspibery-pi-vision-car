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

unsigned long lastEncTime = 0;
long e1_prev, e2_prev, e3_prev, e4_prev;
float v1, v2, v3, v4, v_avg;



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
void loop(){

    // ============= 蓝牙控制区域（保持原样） =============

    if (BT.available()) {
        char cmd = BT.read();
        controlBT(cmd);
    }

    // ============= 速度计算核心 =============
    if(millis() - lastEncTime >= 500) {   // 100Hz
        lastEncTime = millis();
        updateSpeed();
    }
    updateEnc();
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
void controlBT(char cmd){
    Serial.print("CMD = ");
    Serial.println(cmd);

    switch(cmd){
        case 'F':
            car_control(speed, speed);
            break;

        case 'B':
            car_control1(speed, speed);
            break;

        case 'L':
            car_turn();
            break;

        case 'R':
            car_turn1();
            break;

        case 'X':
            car_control(speed, speed-50);
            break;

        case 'Y':
            car_control(speed-50, speed);
            break;

        case 'S':
        default:
            car_control(0,0);
            break;
    }
}
void updateSpeed(){
    // 更新编码器原始数据
    updateEnc();

    long c1 = cnt(e1_raw);
    long c2 = cnt(e2_raw);
    long c3 = cnt(e3_raw);
    long c4 = cnt(e4_raw);

    // Δcount
    long d1 = c1 - e1_prev;
    long d2 = c2 - e2_prev;
    long d3 = c3 - e3_prev;
    long d4 = c4 - e4_prev;

    // 存当前值用于下一次
    e1_prev = c1;
    e2_prev = c2;
    e3_prev = c3;
    e4_prev = c4;

    // 时间固定100ms → 0.5s
    const float dt = 0.5f;

    // 根据 1083 count = 1m
    // → m/s = Δcount / 1083 / dt
    v1 = (float)d1 / 1083.0 / dt;
    v2 = (float)d2 / 1083.0 / dt;
    v3 = (float)d3 / 1083.0 / dt;
    v4 = (float)d4 / 1083.0 / dt;

    v_avg = (v1+v2+v3+v4)/-4.0;

    Serial.print("V1=");
    Serial.print(v1,3);
    Serial.print("\tV2=");
    Serial.print(v2,3);
    Serial.print("\tV3=");
    Serial.print(v3,3);
    Serial.print("\tV4=");
    Serial.print(v4,3);
    Serial.print("\tAVG=");
    Serial.println(v_avg,3);

    BT.println(v_avg, 3);

}
