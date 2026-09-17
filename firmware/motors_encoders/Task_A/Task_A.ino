
float k = 0.45;// coefficient to control the offest
// larger k, small offset 
// smaller k, large offset
int speed = 40;
int meter = 4;
unsigned int mval;// mval is converting to pitch
int state =  0;
//========parameter========

#include <SoftwareSerial.h>
SoftwareSerial BT(10, 11); // RX, TX


//========encoder data==========/

// each rotary for the wheel is 235 pitch 

//diameter of the wheel is 21.7 cm

// 235 pitch per 0.217m
// 1 meter is 1083 pitch
#define encoder_shift 0x80000000UL

unsigned long e1_raw, e2_raw, e3_raw, e4_raw;

long cnt(unsigned long raw){
    return (long)raw - (long)encoder_shift;
}

unsigned int average;




float leftspeed;
float rightspeed;
unsigned int val;


//=================offest setup====================//

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
// ===               zINTERRUPT DETECTION ROUTINE                ===
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
    // (115200 chosen because it is required for Teapot Demo output, but it's
    // really up to you depending on your project)
    Serial.begin(115200);


    Serial.println(F("Initializing I2C devices..."));
    mpu.initialize();
    pinMode(INTERRUPT_PIN, INPUT);

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    // wait for ready
    Serial.println(F("\nSend any character to begin DMP programming and demo: "));
        
        
        /*while (Serial.available() && Serial.read()); // empty buffer
        while (!Serial.available());                 // wait for data
        while (Serial.available() && Serial.read()); // empty buffer again
        */
    delay(2000);//wait for 2 seconds
    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // Calibration Time: generate offsets and calibrate our MPU6050
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        mpu.PrintActiveOffsets();
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
        Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
        Serial.println(F(")..."));
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }

    // configure LED for output
    pinMode(LED_PIN, OUTPUT);
    mval = meter*1083;// set the preset distance
}







// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() {

    while(state !=4 )
    {
    updateIMU();
    updateEnc();
    calculate_offset();
    calculate_height();
    //car_control(leftspeed,rightspeed);
    car_control(leftspeed,rightspeed);
    BT.println("Straight");

    if(state == 3)
    {
      car_control(leftspeed,rightspeed);
      delay(500);
      car_contro0l(0,0);
      delay(500);
      car_control(0,50);
      BT.println("Rotate");
      delay(3170);      
      car_control(50,50);
      BT.println("Straight");
      delay(2000);
      state = 4;
      BT.println("Stop");
      break;
    }
    }
    delay(2);
    car_control(0,0);

    
//==========stop===========
    average = (cnt(e1_raw)+cnt(e2_raw)+cnt(e3_raw)+cnt(e4_raw))/4;
    // Serial.println("average\t");
    val = -(average-65555);
    // Serial.println(val);
    

   /*if(val >= mval){// val is the data from the encoder, maval is the preset data.
    car_control(0,0);
}*/ 

//==========================
    delay(10);// 100Hz control frequency
}



void updateIMU(){
    
    // if programming failed, don't try to do anything
    if (!dmpReady) return;
    // read a packet from FIFO
    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) { // Get the Latest packet 
        #ifdef OUTPUT_READABLE_QUATERNION
            // display quaternion values in easy matrix form: w x y z
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            Serial.print("quat\t");
            Serial.print(q.w);
            Serial.print("\t");
            Serial.print(q.x);
            Serial.print("\t");
            Serial.print(q.y);
            Serial.print("\t");
            Serial.println(q.z);
        #endif

        #ifdef OUTPUT_READABLE_EULER
            // display Euler angles in degrees
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetEuler(euler, &q);
            Serial.print("euler\t");
            Serial.print(euler[0] * 180/M_PI);
            Serial.print("\t");
            Serial.print(euler[1] * 180/M_PI);
            Serial.print("\t");
            Serial.println(euler[2] * 180/M_PI);
        #endif

        #ifdef OUTPUT_READABLE_YAWPITCHROLL
            // display Euler angles in degrees
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
            Serial.print("ypr\t");
            Serial.print(ypr[0] * 180/M_PI);// this is yaw 
            Serial.print("\t");
            // Serial.print(ypr[1] * 180/M_PI); // this is pitch 
            // Serial.print("\t");
            // Serial.println(ypr[2] * 180/M_PI);// this is roll
        #endif

        #ifdef OUTPUT_READABLE_REALACCEL
            // display real acceleration, adjusted to remove gravity
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetAccel(&aa, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
            Serial.print("areal\t");
            Serial.print(aaReal.x);
            Serial.print("\t");
            Serial.print(aaReal.y);
            Serial.print("\t");
            Serial.println(aaReal.z);
        #endif

        #ifdef OUTPUT_READABLE_WORLDACCEL
            // display initial world-frame acceleration, adjusted to remove gravity
            // and rotated based on known orientation from quaternion
            mpu.dmpGetQuaternion(&q, fifoBuffer);
            mpu.dmpGetAccel(&aa, fifoBuffer);
            mpu.dmpGetGravity(&gravity, &q);
            mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
            mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);
            Serial.print("aworld\t");
            Serial.print(aaWorld.x);
            Serial.print("\t");
            Serial.print(aaWorld.y);
            Serial.print("\t");
            Serial.println(aaWorld.z);
        #endif
    
        #ifdef OUTPUT_TEAPOT
            // display quaternion values in InvenSense Teapot demo format:
            teapotPacket[2] = fifoBuffer[0];
            teapotPacket[3] = fifoBuffer[1];
            teapotPacket[4] = fifoBuffer[4];
            teapotPacket[5] = fifoBuffer[5];
            teapotPacket[6] = fifoBuffer[8];
            teapotPacket[7] = fifoBuffer[9];
            teapotPacket[8] = fifoBuffer[12];
            teapotPacket[9] = fifoBuffer[13];
            Serial.write(teapotPacket, 14);
            teapotPacket[11]++; // packetCount, loops at 0xFF on purpose
        #endif

        // blink LED to indicate activity
        blinkState = !blinkState;
        digitalWrite(LED_PIN, blinkState);
    }
    delay(1);

}



//===============car_control==============
void car_control(float a, float b){
//========methond A===========

  /*Wire.beginTransmission(42);  // begin to commuication with slave at 42 address
  Wire.write("baffff"); // set the direction of each whell, f representing forward, r representing backward
  // Motor1: right front
  Wire.write((int)rightspeed);    // speed  
  // Motor2: right back
  Wire.write((int)leftspeed);   
  // Motor3: left front
  Wire.write((int)rightspeed);
  // Motor4: left back
  Wire.write((int)leftspeed);
  Wire.endTransmission();*/
  //==========method B==========

  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");

  // Motor1: 右前
  Wire.write((int)b);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
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
  delay(1); // according to experience you need small delay 1ms after the end of each transmission

}


//===============car_control==============
void car_control1(float a, float b){
//========methond A===========

  /*Wire.beginTransmission(42);  // begin to commuication with slave at 42 address
  Wire.write("baffff"); // set the direction of each whell, f representing forward, r representing backward
  // Motor1: right front
  Wire.write((int)rightspeed);    // speed  
  // Motor2: right back
  Wire.write((int)leftspeed);   
  // Motor3: left front
  Wire.write((int)rightspeed);
  // Motor4: left back
  Wire.write((int)leftspeed);
  Wire.endTransmission();*/
  //==========method B==========

  Wire.beginTransmission(42);  // 开始与地址42的从机通信
  Wire.write("sa");
  Wire.write("baffff");

  // Motor1: 右前
  Wire.write((int)b);    // 速度
  Wire.write(0);     // 方向（0=前进, 1=后退）
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
  delay(1); // according to experience you need small delay 1ms after the end of each transmission
}


    void calculate_offset()
    {
    // Build 5 frame averager filter
    G5 = G4;
    G4 = G3;
    G3 = G2;
    G2 = G1;
    G1 = ypr[0] * 180/M_PI;
    // Average the last 5 readings
    G = (G1 + G2 + G3 + G4 + G5) / 5;
    leftspeed = speed - (G / k);
    rightspeed = speed + (G / k);
    Serial.println("left speed\t");
    Serial.println(leftspeed);
    Serial.println("right speed\t");
    Serial.println(rightspeed);
    delay(1);
    }


    void calculate_height()
    {
    // Build 5 frame averager filter
    H5 = H4;
    H4 = H3;
    H3 = H2;
    H2 = H1;
    H1 = ypr[1] * 180/M_PI;
    // Average the last 5 readings
    H = (H1 + H2 + H3 + H4 + H5) / 5;
    Serial.println("height\t");
    Serial.println(H);
    delay(1);
    if (state == 0 && H > 10) {
        state = 1;
        Serial.println(">>>> ENTER UPHILL");
        BT.println("UPHILL");

    }
    // uphill -> ground
    if (state == 1 && H < 10) {
        state = 3;
        Serial.println("<<<< BACK TO FLAT");
    }
    }


//=========encoder===========//
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
