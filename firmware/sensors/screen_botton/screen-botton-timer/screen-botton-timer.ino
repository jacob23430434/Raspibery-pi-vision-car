#include <LiquidCrystal.h> 

// ====== Pins ======
#define MIC_INT_PIN 2   // 中断引脚，用来捕捉拍手信号
const int LED_PIN = 13;
#define BUZZER_PIN 8
#define A4_PIN A4

// ====== Mario ======
int marioNotes[] = {659,659,0,659,0,523,659,0,784,0,392,0,523,0,392,0,330,0,440,0,494,0,466,440};
int marioDurations[] = {150,150,150,150,150,150,150,150,200,200,200,200,150,150,200,200,200,200,200,200,200,200,200,250};
const int NUM_MARIO = sizeof(marioNotes)/sizeof(marioNotes[0]);

// ====== Tetris ======
int tetrisNotes[] = {659,494,523,587,523,494,440,440,523,659,587,523,494,523,587,659,523,440,494,523,587,523,440,440};
int tetrisDurations[] = {300,150,150,300,150,150,300,150,150,300,150,150,300,150,150,300,150,150,300,150,150,300,150,200};
const int NUM_TETRIS = sizeof(tetrisNotes)/sizeof(tetrisDurations[0]);

// ====== LCD ======
const int rs = 12, en = 11, d4 = 4, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// ====== Rotary Encoder ======
volatile int step = 0;
int pushButton1 = 10;
int pushButton2 = 3;
uint8_t lastState;

// ====== Main Button ======
int buttonPin = 9;
unsigned long pressStart = 0;

// ====== Settings ======
int speedValue = 50;
char currentTask = 'A';
int cyclesValue = 3;
float metersValue = 2.0;

enum Mode { VIEW, SET_SPEED, SET_TASK, SET_PARAM };
Mode mode = VIEW;

// ====== 状态锁 ======
volatile bool clapDetected = false;
bool isPlaying = false;

// ====== 中断：只设标志 ======
void clapISR() {
  clapDetected = true;
}

// ====== 屏幕更新函数 ======
void updateScreen() {
  lcd.clear();
  switch(mode) {
    case VIEW:
      lcd.setCursor(0,0); lcd.print("Task: "); lcd.print(currentTask);

      lcd.setCursor(0,1);
      lcd.print("Speed:");
      lcd.print(speedValue);
      lcd.print("% ");

      if(isPlaying) { lcd.print("Run"); digitalWrite(LED_PIN,HIGH); }
      else { lcd.print("Stop"); digitalWrite(LED_PIN,LOW); }

      if(currentTask == 'A') { lcd.setCursor(8,0); lcd.print("Cyc:"); lcd.print(cyclesValue); }
      else { lcd.setCursor(8,0); lcd.print("M:"); lcd.print(metersValue,1); }
    break;

    case SET_SPEED:
      lcd.setCursor(0,0); lcd.print("Set Speed:");
      lcd.setCursor(0,1); lcd.print(speedValue); lcd.print("%");
    break;

    case SET_TASK:
      lcd.setCursor(0,0); lcd.print("Select Task:");
      lcd.setCursor(0,1); lcd.print("A / B  <"); lcd.print(currentTask); lcd.print(">");
    break;

    case SET_PARAM:
      if(currentTask=='A') {
        lcd.setCursor(0,0); lcd.print("Set Cycles:");
        lcd.setCursor(0,1); lcd.print(cyclesValue);
      } else {
        lcd.setCursor(0,0); lcd.print("Set Meters:");
        lcd.setCursor(0,1); lcd.print(metersValue,1); lcd.print("m");
      }
    break;
  }
}

// ====== 播放函数 ======
void playSong(int *notes, int *durations, int len) {
  for(int i=0;i<len;i++){
    if(notes[i] > 0) tone(BUZZER_PIN, notes[i], durations[i]);
    digitalWrite(LED_PIN, notes[i] > 0);
    delay(durations[i]);
  }
  digitalWrite(LED_PIN, LOW);
  noTone(BUZZER_PIN);
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  
  pinMode(pushButton1, INPUT);
  pinMode(pushButton2, INPUT);
  pinMode(buttonPin, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(A4_PIN, INPUT);

  lcd.begin(16, 2);
  lastState = (digitalRead(pushButton2)<<1) | digitalRead(pushButton1);

  // *** 关键修复点 ***
  pinMode(MIC_INT_PIN, INPUT);  
  attachInterrupt(digitalPinToInterrupt(MIC_INT_PIN), clapISR, RISING);

  updateScreen();
}

// ====== LOOP ======
void loop() {

  // A7 状态检测（保留）
  int a4Value = analogRead(A4_PIN);
  if(a4Value >= 1000) Serial.println("clockwise/forward");
  else Serial.println("anticlockwise/backward");
  Serial.println（

  // *** 拍手触发逻辑（不变，只换稳定版本中断） ***
  if(clapDetected && !isPlaying) {
    clapDetected = false;
    isPlaying = true;
    updateScreen();

    playSong(marioNotes, marioDurations, NUM_MARIO);
    delay(2000);
    playSong(tetrisNotes, tetrisDurations, NUM_TETRIS);

    isPlaying = false;
    updateScreen();
  }

  // 按键菜单逻辑（不变）
  if(digitalRead(buttonPin)==LOW) {
    pressStart = millis();
    while(digitalRead(buttonPin)==LOW);
    unsigned long t = millis() - pressStart;

    if(t < 500) {
      if(mode==VIEW) mode = SET_SPEED;
      else mode = VIEW;
    } else {
      if(mode==VIEW) mode = SET_TASK;
      else if(mode==SET_TASK) mode = SET_PARAM;
      else mode = VIEW;
    }
    updateScreen();
  }

  // 旋钮逻辑（不变）
  uint8_t cur = (digitalRead(pushButton2)<<1) | digitalRead(pushButton1);
  if(cur != lastState) {
    if ((lastState==0b00 && cur==0b01) ||
        (lastState==0b01 && cur==0b11) ||
        (lastState==0b11 && cur==0b10) ||
        (lastState==0b10 && cur==0b00)) step++;
    else step--;
    lastState = cur;
    if(cur == 0b00) {
      if(mode == SET_SPEED) speedValue = constrain(speedValue + (step>0?1:-1), 50, 80);
      else if(mode == SET_TASK) currentTask = (step>0?'A':'B');
      else if(mode == SET_PARAM) {
        if(currentTask=='A') cyclesValue = constrain(cyclesValue + (step>0?1:-1), 1, 20);
        else metersValue = constrain(metersValue + (step>0?0.1:-0.1), 1.0, 10.0);
      }
      step = 0;
      updateScreen();
    }
  }
}
