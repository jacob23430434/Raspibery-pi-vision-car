#include <LiquidCrystal.h>
#include <Wire.h>
#include <MsTimer2.h>

// ====== Pins ======
#define MIC_INT_PIN 2
#define LED_PIN 13
#define BUZZER_PIN 8
#define DIR_SWITCH_PIN A7
#define LED A1
#define BUTTON_PIN 9
#define PUSH1 3
#define PUSH2 10

const int rs=12,en=11,d4=4,d5=5,d6=6,d7=7;
LiquidCrystal lcd(rs,en,d4,d5,d6,d7);

// ====== MsTimer2 RT setup ======
volatile bool updateFlag = false;
const unsigned long controlInterval = 5; // 5ms → 200Hz

// ====== Small car parameters ======
int steerForward = 5;    // Forward steering compensation
int steerBackward = -5;  // Backward steering compensation
float distanceCompensation = 1.33; // Distance correction factor
int sp = 50;             // Default speed

// ====== I2C motor constants ======
constexpr uint8_t MOTOR_ADDR = 0x2A;
constexpr int8_t WHEEL_SENSE[4] = {-1, 1, 1, -1};
constexpr int8_t LINEAR_TRIM[4] = {-2, 6, -2, 6};
constexpr char CMD_SET_SPEED_DIR = 'b';
constexpr char CMD_HALT_ALL     = 'h';
constexpr char TARGET_ALL       = 'a';

// ====== Movement control ======
bool moving = false;
bool forward = true;
float targetMeters = 0;
unsigned long moveStartTime = 0;
unsigned long moveDuration = 0;

// ====== Variables ======
volatile int step=0;
uint8_t lastState;
int speedValue=50;
float metersValue=2.0;
int cyclesValue=3;
char currentTask='B';   // Default TaskB
bool isPlaying=false;
volatile bool triggered=false;
bool locked=false;
unsigned long lockStartTime=0;
const unsigned long lockDuration=6000;

enum MenuMode { MODE_SPEED, MODE_TASK_PARAM };
MenuMode menuMode = MODE_SPEED;
unsigned long lcdLastUpdate=0;

// ====== Motor functions ======
static inline int16_t clamp100(int v){
  if(v>100) return 100;
  if(v<-100) return -100;
  return v;
}

static inline void writeUint16LE(uint16_t x){
  Wire.write((uint8_t)(x & 0xFF));
  Wire.write((uint8_t)(x >> 8));
}

void driveAll(int m1, int m2, int m3, int m4){
  int req[4] = {m1,-m2,-m3,m4};
  char dir[4];
  uint16_t mag[4];

  for(uint8_t i=0;i<4;i++){
    int v=req[i];
    v+=(v>=0)?LINEAR_TRIM[i]:-LINEAR_TRIM[i];
    v=clamp100(v);
    int adj = clamp100(v * WHEEL_SENSE[i]);
    dir[i]=(adj>=0)?'f':'r';
    mag[i]=(uint16_t)(adj>=0?adj:-adj);
  }

  Wire.beginTransmission(MOTOR_ADDR);
  Wire.write(CMD_SET_SPEED_DIR); Wire.write(TARGET_ALL);
  Wire.write(dir[0]); Wire.write(dir[2]); Wire.write(dir[1]); Wire.write(dir[3]);
  writeUint16LE(mag[0]); writeUint16LE(mag[2]); writeUint16LE(mag[1]); writeUint16LE(mag[3]);
  Wire.endTransmission();
}

void stopAll(){
  Wire.beginTransmission(MOTOR_ADDR);
  Wire.write(CMD_HALT_ALL);
  Wire.write(TARGET_ALL);
  Wire.endTransmission();
}

// ====== Movement control using non-blocking & MsTimer2 ======
void startMove(bool fwd, int speed, float meters){
    forward = fwd;
    sp = speed;
    targetMeters = meters;
    moveDuration = meters * distanceCompensation * 1000;
    moveStartTime = millis();
    moving = true;
}

void updateMove(){
    if(!moving) return;

    int offset = forward ? steerForward : steerBackward;
    int m1, m2, m3, m4;

    if(offset > 0){
        m1 = sp; m3 = sp;
        m2 = sp - offset; m4 = sp - offset;
    } else if(offset < 0){
        m1 = sp + offset; m3 = sp + offset;
        m2 = sp; m4 = sp;
    } else {
        m1 = m2 = m3 = m4 = sp;
    }

    if(!forward){
        m1 = -m1; m2 = -m2; m3 = -m3; m4 = -m4;
    }

    driveAll(m1, m2, m3, m4);

    if(millis() - moveStartTime >= moveDuration){
        stopAll();
        moving = false;
    }
}

// ====== Timer ISR ======
void controlISR(){
    updateFlag = true;
}

// ====== Clap detection ======
void clapISR(){
  if(!locked) triggered=true;
}

// ====== Buzzer ======
unsigned long buzzerStart=0;
unsigned long buzzerDuration=0;
bool buzzerActive=false;

void playBuzzer(int freq, int duration){
    tone(BUZZER_PIN,freq,duration);
    buzzerStart = millis();
    buzzerDuration = duration;
    buzzerActive = true;
}

void updateBuzzer(){
    if(buzzerActive && millis()-buzzerStart >= buzzerDuration){
        noTone(BUZZER_PIN);
        buzzerActive = false;
    }
}

// ====== LCD ======
void updateScreen(){
  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print("Task: "); lcd.print(currentTask);
  lcd.setCursor(7,0); 
  if(currentTask=='A') lcd.print("C:"); else lcd.print("M:");
  lcd.print((currentTask=='A')?cyclesValue:metersValue,1);

  lcd.setCursor(0,1); 
  lcd.print("Speed:"); lcd.print(speedValue); lcd.print("% ");

  lcd.setCursor(11,1);
  bool dirState=(analogRead(DIR_SWITCH_PIN)>1000);
  lcd.print(dirState?"FC":"BAC");

  lcd.setCursor(12,0);
  if(isPlaying){lcd.print("Run"); digitalWrite(LED_PIN,HIGH);}
  else{lcd.print("Stop"); digitalWrite(LED_PIN,LOW);}
}

// ====== Setup ======
void setup(){
    Wire.begin();
    stopAll();
    Serial.begin(115200);

    pinMode(LED,OUTPUT);
    pinMode(PUSH1,INPUT);
    pinMode(PUSH2,INPUT);
    pinMode(BUTTON_PIN,INPUT);
    pinMode(LED_PIN,OUTPUT);
    pinMode(BUZZER_PIN,OUTPUT);
    pinMode(DIR_SWITCH_PIN,INPUT);
    pinMode(MIC_INT_PIN,INPUT);

    attachInterrupt(digitalPinToInterrupt(MIC_INT_PIN),clapISR,RISING);

    lastState=(digitalRead(PUSH2)<<1)|digitalRead(PUSH1);

    lcd.begin(16,2);
    updateScreen();

    MsTimer2::set(controlInterval, controlISR); // 5ms → 200Hz
    MsTimer2::start();
}

// ====== Loop ======
void loop(){
    // 1. MsTimer2 control update
    if(updateFlag){
        updateFlag = false;
        updateMove();
    }

    // 2. Update buzzer
    updateBuzzer();

    // 3. Clap-triggered Task B
    bool dirForward = (analogRead(DIR_SWITCH_PIN)>1000);
    if(triggered && !isPlaying && !locked && currentTask=='B'){
        triggered=false;
        locked=true;
        lockStartTime=millis();
        isPlaying=true;
        updateScreen();

        digitalWrite(LED,HIGH);
        playBuzzer(1000,200); delay(250);
        playBuzzer(800,200); delay(250);

        startMove(dirForward,speedValue,metersValue);
        while(moving){ 
            if(updateFlag){ updateFlag=false; updateMove(); }
            updateBuzzer();
        }

        playBuzzer(600,400); delay(500);
        playBuzzer(1200,400); delay(500);
        digitalWrite(LED,LOW);

        isPlaying=false;
        updateScreen();
    }

    // 4. Automatic unlock
    if(locked && millis()-lockStartTime>=lockDuration) locked=false;

    // 5. Button for menu/task selection
    static unsigned long pressStart=0;
    if(digitalRead(BUTTON_PIN)==LOW){
        pressStart = millis();
        while(digitalRead(BUTTON_PIN)==LOW); // Wait release
        unsigned long t = millis()-pressStart;

        if(t < 500){ // Short press → toggle menu
            menuMode = (menuMode==MODE_SPEED)?MODE_TASK_PARAM:MODE_SPEED;
        } else {    // Long press → toggle task
            currentTask = (currentTask=='A')?'B':'A';
        }
        updateScreen();
    }

    // 6. Rotary encoder
    uint8_t cur=(digitalRead(PUSH2)<<1)|digitalRead(PUSH1);
    if(cur!=lastState){
        if ((lastState==0b00 && cur==0b01) ||
            (lastState==0b01 && cur==0b11) ||
            (lastState==0b11 && cur==0b10) ||
            (lastState==0b10 && cur==0b00)) step++;
        else step--;
        lastState=cur;

        if(cur==0b00){
            if(menuMode==MODE_SPEED){
                speedValue=constrain(speedValue+(step>0?1:-1),1,100);
            }else{
                if(currentTask=='A') cyclesValue=constrain(cyclesValue+(step>0?1:-1),1,20);
                else metersValue=constrain(metersValue+(step>0?0.1:-0.1),1.0,10.0);
            }
            step=0;
            if(millis()-lcdLastUpdate>=100){
                updateScreen();
                lcdLastUpdate=millis();
            }
        }
    }
}
