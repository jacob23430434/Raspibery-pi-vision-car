float k = 0.6;// coefficient to control the offset
// larger k, smaller offset 
// smaller k, larger offset
int speed = 75;
int meter = 1;
unsigned int mval;// mval is converted to encoder pulses
int state =  0;
unsigned int i;// for counting
float cycles;
float yaw_now;
float yaw_prev;
float yaw_total;
//========parameter========

#include <SoftwareSerial.h>
SoftwareSerial BT(10, 11); // RX, TX


//========encoder data==========/

// each rotary encoder for the wheel is 235 pulses 

// diameter of the wheel is 21.7 cm

// 235 pulses per 0.217 m
// 1 meter is 1083 pulses
#define encoder_shift 0x80000000UL

unsigned long e1_raw, e2_raw, e3_raw, e4_raw;

long cnt(unsigned long raw){
    return (long)raw - (long)encoder_shift;
}

unsigned int average;

float leftspeed;
float rightspeed;
unsigned int val;


//=================offset setup====================//

// Declare Variables
float G;
float G1 = 0;
float G2 = 0;
float G3 = 0;
float G4 = 0;
float G5 = 0;

float H;
float H1 = 0;
float H2 = 0;
float H3 = 0;
float H4 = 0;
float H5 = 0;


//================offset setup===================//
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif


MPU6050 mpu;
#define OUTPUT_READABLE_YAWPITCHROLL
#define INTERRUPT_PIN 2  // use pin 2 on Arduino Uno & most boards
#define LED_PIN 13 // (Arduino is 13, Teensy is 11, Teensy++ is 6)
bool blinkState = false;
// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };




// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
}

// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {
      BT.begin(9600);

    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    // initialize serial communication
    Serial.begin(115200);

    Serial.println(F("Initializing I2C devices..."));
    mpu.initialize();
    pinMode(INTERRUPT_PIN, INPUT);

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    delay(500);// wait for 0.5 seconds
    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

    if (devStatus == 0) {
        // Calibration Time: generate offsets and calibrate MPU6050
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        mpu.PrintActiveOffsets();

        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
        Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
        Serial.println(F(")..."));
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }

    pinMode(LED_PIN, OUTPUT);
    mval = meter * 1083;// set the preset distance
}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() {

    if(mpuInterrupt)
    {
        mpuInterrupt = false;
        updateIMU();
        updateYawRotation();
        calculate_height();
        BT.println("The cycles:\t");
        BT.println(cycles);
    }
    updateEnc();
    calculate_offset();// when the car turns left, G goes from 0 to -180
    // when the car turns right, G goes from 0 to 180

    BT.println("Straight");
    if(state == 0 || state == 1 || state == 2){// state = 0 represents going straight 
        car_control(leftspeed,rightspeed);
    }
    if(state == 3){// uphill state
        yaw_total = 0;
        yaw_prev  = yaw_now;     // use the current direction as reference
        car_control(0,0);        // stop briefly
        delay(2000);
        state = 4;               // enter rotation state
        BT.println(">>> Start Rotate");
    }
    if(state == 4){// 4 is turning state
        car_turn();              // send rotation command every loop (continuous rotation)
        // check whether the target rotation cycles are reached
        if(cycles <= -0.9){
            car_control(0,0);    // stop the car
            state = 0;           // return to initial state
            BT.println(">>> Rotate Stop");
        }
    }
    if (state == 5)
    {
        delay(300);
        car_control(0,0);
    }
}


//==========stop===========
void car_stop(){
    average = (cnt(e1_raw)+cnt(e2_raw)+cnt(e3_raw)+cnt(e4_raw))/4;
    val = -(average-65555);
    if(val >= mval){// val is encoder data, mval is the preset target
        car_control(0,0);
    }
}

void updateIMU(){
    
    // if programming failed, do nothing
    if (!dmpReady) return;

    // read a packet from FIFO
    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {
        #ifdef OUTPUT_READABLE_YAWPITCHROLL
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
            Serial.print("ypr\t");
            Serial.print(ypr[0] * 180/M_PI);// this is yaw 
            Serial.print("\t");
        #endif

        blinkState = !blinkState;
        digitalWrite(LED_PIN, blinkState);
    }
}


//===============car_control==============
void car_control(float a, float b){
  Wire.beginTransmission(42);  // start communication with slave at address 42
  Wire.write("baffff");
  // Motor1: front right
  Wire.write((int)b);    // speed
  Wire.write(0);         // direction (0 = forward, 1 = backward)
  // Motor2: rear right
  Wire.write((int)b);
  Wire.write(0);
  // Motor3: front left
  Wire.write((int)a);
  Wire.write(0);  
  // Motor4: rear left
  Wire.write((int)a);
  Wire.write(0);
  Wire.endTransmission();
  delay(1); // small 1 ms delay is required after each transmission
}

//===============car_turn==============
void car_turn(){
  Wire.beginTransmission(42);
  Wire.write("tr");// change direction and speed simultaneously
  for(i=0;i<=3;i++)
  {
    Wire.write(speed);// lower 8-bit of speed
    Wire.write(0);     // higher 8-bit of speed
  }
  for(i=0;i<=3;i++)
  {
    Wire.write(256); // lower 8-bit of distance (encoder value)
    Wire.write(100); // next 8-bit of distance
    Wire.write(0);
    Wire.write(0);
  }
  Wire.endTransmission();
  delay(1);
}


void calculate_offset()
{
    // build 5-frame moving average filter
    G5 = G4;
    G4 = G3;
    G3 = G2;
    G2 = G1;
    G1 = ypr[0] * 180/M_PI;

    // average the last 5 readings
    G = (G1 + G2 + G3 + G4 + G5) / 5;
    leftspeed = speed - (G / k);
    rightspeed = speed + (G / k);
    Serial.println("left speed\t");
    Serial.println(leftspeed);
    Serial.println("right speed\t");
    Serial.println(rightspeed);
    BT.println(G);
}


void calculate_height()
{
    // build 5-frame moving average filter
    H5 = H4;
    H4 = H3;
    H3 = H2;
    H2 = H1;
    H1 = ypr[1] * 180/M_PI;

    // average the last 5 readings
    H = (H1 + H2 + H3 + H4 + H5) / 5;
    Serial.println("height\t");
    Serial.println(H);
    if (state == 0 && H > 10) {// flat to uphill
        state = 1;
        Serial.println(">>>> ENTER UPHILL");
        BT.println("UPHILL");
    }
    // uphill to flat
    if (state == 1 && H < 10) {
        state = 3;
        BT.println("<<<< BACK TO FLAT");
    }
    if (state == 0 && H < -10 )
    {
        state = 2;// flat to downhill
        BT.println("<<<< Enter Downhill");
    }
    if (state == 2 && H > -10)
    {
        state = 5;// downhill to flat
    }
    BT.println("state = \t");
    BT.println(state);
}

//=========encoder===========
void updateEnc(){
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


void updateYawRotation() { // calculate rotation values
    float yaw_now = ypr[0] * 180.0 / M_PI;
    float diff = yaw_now - yaw_prev;

    if(diff < -180) diff += 360;
    if(diff > 180) diff -= 360;

    yaw_total += diff;
    yaw_prev = yaw_now;
    cycles = yaw_total / 360;
}
