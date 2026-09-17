
#include <Wire.h>
int speed = 75;

void setup() {
  // put your setup code here, to run once:
  Wire.begin();

}

void loop() {
  // put your main code here, to run repeatedly:
car_control1(speed,speed);
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