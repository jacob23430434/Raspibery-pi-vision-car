// =====================================================
//                Joystick + LCD + nRF24 TX
// =====================================================
#include <SPI.h>
#include "RF24.h"
#include <LiquidCrystal.h>

// ========= LCD1602 =========
// RS,E,D4,D5,D6,D7
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

// ========= Joystick =========
const int JOY_X_PIN = A0;
const int JOY_Y_PIN = A1;

// ========= nRF24 =========
RF24 rf24(9, 10);              // CE, CSN
const byte addr[] = "1Node";   // 与车端一致

// ========= 控制 =========
char cmd = 'S';
unsigned long lastUpdate = 0;
const uint16_t REFRESH_MS = 100; // 10Hz 刷新

// ========== 链路状态 ==========
bool linkOK = false;
unsigned long lastLinkOKTime = 0;
const uint16_t LINK_TIMEOUT = 1000; //1秒

void setup() {
  Serial.begin(9600);

  // LCD 初始化
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("CMD: ");
  lcd.setCursor(0,1);
  lcd.print("X:    Y:   ");

  // RF24 初始化
  rf24.begin();
  rf24.setChannel(93);
  rf24.setPALevel(RF24_PA_MAX);
  rf24.setDataRate(RF24_2MBPS);
  rf24.openWritingPipe(addr);
  rf24.stopListening();

  delay(300);
  Serial.println("TX Ready!");
}

void loop() {

  unsigned long now = millis();
  if(now - lastUpdate < REFRESH_MS) return;
  lastUpdate = now;

  // -------- 1.读取摇杆 ADC --------
  int xRaw = analogRead(JOY_X_PIN);
  int yRaw = analogRead(JOY_Y_PIN);

  // -------- 2.ADC → 指令 --------
  if (yRaw > 1000)       cmd = 'F';
  else if (yRaw < 30)    cmd = 'B';
  else if (xRaw < 30)    cmd = 'R';
  else if (xRaw > 1000)  cmd = 'L';
  else                   cmd = 'S';

  // -------- 3.RF24发送 --------
  bool ok = rf24.write(&cmd, sizeof(cmd));
  if(ok){
    linkOK = true;
    lastLinkOKTime = now;
  }

  // -------- 4.掉线检测 --------
  if(linkOK && now - lastLinkOKTime > LINK_TIMEOUT){
    linkOK = false;
  }

  // -------- 5.调试输出 --------
  Serial.print("CMD=");
  Serial.print(cmd);
  Serial.print("  X=");
  Serial.print(xRaw);
  Serial.print("  Y=");
  Serial.print(yRaw);
  Serial.println(linkOK ? " [OK]" : " [Lost]");

  // -------- 6.LCD 显示 --------

  // CMD
  lcd.setCursor(5,0);
  lcd.print(cmd);
  lcd.print(" ");

  // X ADC
  lcd.setCursor(3,1);
  lcd.print(xRaw);
  lcd.print("   ");

  // Y ADC
  lcd.setCursor(9,1);
  lcd.print(yRaw);
  lcd.print("   ");
  delay(100);
}
