#include <Wire.h>

#define encoder_shift 0x80000000UL

unsigned long e1_raw, e2_raw, e3_raw, e4_raw;

long cnt(unsigned long raw){
    return (long)raw - (long)encoder_shift;
}

void readEnc(){
    Wire.beginTransmission(42);
    Wire.write("i0");
    Wire.endTransmission();
    delay(1);

    Wire.requestFrom(42,8);
    e1_raw = (unsigned long)Wire.read();
    e1_raw += (unsigned long)Wire.read()<<8;
    e1_raw += (unsigned long)Wire.read()<<16;
    e1_raw += (unsigned long)Wire.read()<<24;

    e2_raw = (unsigned long)Wire.read();
    e2_raw += (unsigned long)Wire.read()<<8;
    e2_raw += (unsigned long)Wire.read()<<16;
    e2_raw += (unsigned long)Wire.read()<<24;


    Wire.beginTransmission(42);
    Wire.write("i5");
    Wire.endTransmission();
    delay(1);

    Wire.requestFrom(42,8);
    e3_raw = (unsigned long)Wire.read();
    e3_raw += (unsigned long)Wire.read()<<8;
    e3_raw += (unsigned long)Wire.read()<<16;
    e3_raw += (unsigned long)Wire.read()<<24;

    e4_raw = (unsigned long)Wire.read();
    e4_raw += (unsigned long)Wire.read()<<8;
    e4_raw += (unsigned long)Wire.read()<<16;
    e4_raw += (unsigned long)Wire.read()<<24;
}

void setup(){
    Serial.begin(57600);
    Wire.begin();
    Serial.println("Turn a wheel by hand and watch the encoder values change");
}

void loop(){
    readEnc();
    Serial.print("E1 = ");
    Serial.print(cnt(e1_raw));
    Serial.print("\tE2 = ");
    Serial.print(cnt(e2_raw));
    Serial.print("\tE3 = ");
    Serial.print(cnt(e3_raw));
    Serial.print("\tE4 = ");
    Serial.println(cnt(e4_raw));
    delay(200);
}
