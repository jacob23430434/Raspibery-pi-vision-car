#include "Wire.h"
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#include <SoftwareSerial.h>

MPU6050 mpu;
SoftwareSerial BT(10, 11); // RX, TX

#define INTERRUPT_PIN 2
bool dmpReady = false;
uint8_t fifoBuffer[64];
uint16_t packetSize;

Quaternion q;
VectorInt16 aa, aaReal, aaWorld;
VectorFloat gravity;

//================= 积分变量 =================
float ax_ms2 = 0;
float ay_ms2 = 0;
float vx = 0;
float vy = 0;
float sx = 0;
float sy = 0;
unsigned long lastTime = 0;

// ============== 滤波变量 ==============
float ax_f = 0;
float ay_f = 0;
const float alpha = 0.15;  // 滤波系数

const float ACC_SCALE = 16384.0;
const float G = 9.80665;

volatile bool mpuInterrupt = false;
void dmpDataReady() { mpuInterrupt = true; }

void setup() {
    Serial.begin(115200);
    BT.begin(9600);
    Wire.begin();
    Wire.setClock(400000);

    Serial.println("MPU init...");
    mpu.initialize();
    pinMode(INTERRUPT_PIN, INPUT);

    if (!mpu.testConnection()) {
        Serial.println("MPU6050 connect error!");
        while (1);
    }

    uint8_t devStatus = mpu.dmpInitialize();

    mpu.CalibrateAccel(5);
    mpu.CalibrateGyro(5);

    if (devStatus == 0) {
        mpu.setDMPEnabled(true);
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        packetSize = mpu.dmpGetFIFOPacketSize();
        dmpReady = true;
        lastTime = micros();
        Serial.println("DMP Ready.");
    } else {
        Serial.print("DMP Failed: ");
        Serial.println(devStatus);
        while (1);
    }
}

void loop() {
    if (!dmpReady) return;
    if (!mpuInterrupt) return;
    mpuInterrupt = false;

    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {

        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetAccel(&aa, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

        unsigned long now = micros();
        float dt = (now - lastTime) / 1e6;
        lastTime = now;
        if (dt > 0.2) return;

        // ======== 原始加速度 ===========
        float ax_raw = ((float)aaWorld.x / ACC_SCALE) * G;
        float ay_raw = ((float)aaWorld.y / ACC_SCALE) * G;

        // ========= 一阶低通滤波 ==========
        ax_f = alpha * ax_raw + (1 - alpha) * ax_f;
        ay_f = alpha * ay_raw + (1 - alpha) * ay_f;

        // 使用滤波值
        ax_ms2 = ax_f;
        ay_ms2 = ay_f;

        // 小噪声清零
        if (abs(ax_ms2) < 0.05) ax_ms2 = 0;
        if (abs(ay_ms2) < 0.05) ay_ms2 = 0;

        // ==================积分====================
        vx += ax_ms2 * dt;
        vy += ay_ms2 * dt;
        sx += vx * dt;
        sy += vy * dt;


        // ======== FireWater CSV 输出 ==========
        Serial.print("FW:");
        Serial.print(ax_ms2, 6); Serial.print(",");
        //Serial.print(ay_ms2, 6); Serial.print(",");
        Serial.print(vx, 6); Serial.print(",");
        //Serial.print(vy, 6); Serial.print(",");
        Serial.print(sx, 6); Serial.print(",");
        //Serial.println(sy, 6);

        BT.print("FW:");
        BT.print(ax_ms2, 6); BT.print(",");
        //BT.print(ay_ms2, 6); BT.print(",");
        BT.print(vx, 6); BT.print(",");
        //T.print(vy, 6); BT.print(",");
        BT.print(sx, 6); BT.print(",");
        //BT.println(sy, 6);
    }
}
