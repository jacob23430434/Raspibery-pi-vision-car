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
float ax_ms2 = 0;  // 加速度 m/s²
float vx = 0;     // 速度 m/s
float sx = 0;     // 距离 m
unsigned long lastTime = 0;

// LSB 转实际加速度
const float ACC_SCALE = 16384.0;
const float G = 9.80665;

// ================ DMP中断 ============
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

    // 初始化 DMP
    uint8_t devStatus = mpu.dmpInitialize();

    // 校准（强烈建议）
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

    // 没有数据则等待
    if (!mpuInterrupt) return;
    mpuInterrupt = false;

    // 读取DMP数据包
    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {

        // 获取姿态和重力补偿加速度
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetAccel(&aa, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

        // ================== 时间 ==================
        unsigned long now = micros();
        float dt = (now - lastTime) / 1e6;
        lastTime = now;
        if (dt > 0.2) return;  // 防止积分爆炸

        // ================== X轴加速度 转 m/s² ==================
        ax_ms2 = ((float)aaWorld.x / ACC_SCALE) * G;

        // ======== IMU噪声过滤 (可微调) =========
        if (abs(ax_ms2) < 0.05) ax_ms2 = 0;

        // ================== 积分求速度 ==================
        vx += ax_ms2 * dt;

        // ================== 积分求距离 ==================
        sx += vx * dt;

        // ============ 串口输出 ============
        Serial.print("AX(m/s2)=");
        Serial.print(ax_ms2, 6);
        Serial.print("\tVX(m/s)=");
        Serial.print(vx, 6);
        Serial.print("\tSX(m)=");
        Serial.println(sx, 6);

      
        /*  BT.println("AX(m/s2)=");
        BT.println(ax_ms2, 6);
        BT.println("\tVX(m/s)=");
        BT.println(vx, 6);
        BT.println("\tSX(m)=");
        BT.println(sx, 6);*/

        BT.print(ax_ms2, 6);
        BT.print(",");
        BT.print(vx, 6);
        BT.print(",");
        BT.println(sx, 6);

    }
}
