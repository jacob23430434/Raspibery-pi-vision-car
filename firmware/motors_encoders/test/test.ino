

#include <Wire.h>  // 导入I2C通信库（用于与副板通信）


const int BLINK_ledPin = 13;    // 内置LED（文档1.1 Blink任务，1Hz闪烁）
const int FADE_ledPin = 9;      // 外部PWM LED（文档1.1 Fade任务，D9为PWM引脚）



int val = 0;
byte brightness;          // 渐变LED亮度（0-255）

void setup() {
  pinMode(BLINK_ledPin, OUTPUT);
  pinMode(FADE_ledPin, OUTPUT);
  Serial.begin(9600);              // 串口通信初始化（波特率9600，需与IDE一致）
  Wire.begin();                    // I2C作为主机初始化
}

void loop() {
  // blink
  digitalWrite(BLINK_ledPin, HIGH);  // 点亮LED
  delay(100);                         // 等待500毫秒（0.5秒）
  digitalWrite(BLINK_ledPin, LOW);   // 熄灭LED
  delay(100);                         // 再等待500毫秒
  //----------------above is for a seperate LED--------------- is ok




  int sensorValue = analogRead(A0); // read the input on pin 0
  float voltage = sensorValue * (5.0/1023.0); // turn the input to voltage

  
  if (voltage >= 1) {
    analogWrite(FADE_ledPin, 0);
    Serial.println("High");
  }
  else {
    //set the brightness of pin 9
    analogWrite(FADE_ledPin, brightness);
    brightness += 50;
    //reverse the direction of the fading at the ends of the fade
    delay(30);
  }
  if (brightness <= 0 || brightness >= 255) 
    {
      brightness = 1;
    }



   //----------------below is for test voltage--------------- is ok
  // read analog voltage

  Serial.println(voltage);
}

