/*
 * MPU6050 DMP + 线性加速度积分计算速度/距离
 * 作者优化：ChatGPT
 * 基于 Jeff Rowberg DMP6 示例
 */

#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

// ================================================================
// 定义 MPU
// ================================================================
MPU6050 mpu;

#define INTERRUPT_PIN 2
#define LED_PIN 13

// DMP 状态
bool dmpReady = false;
uint16_t packetSize;
uint8_t fifoBuffer[64];

// 姿态与加速度容器
Quaternion q;
VectorInt16 aa;      
VectorInt16 aaReal;  
VectorInt16 aaWorld; 
VectorFloat gravity; 
float ypr[3];

// ================================================================
// 积分变量
// ================================================================
float acc_forward;   // m/s^2
float vel = 0;       // m/s
float dist = 0;      // m
unsigned long lastTime;

// ZUPT 阈值
float TH_ACC = 0.15;  // m/s^2
float TH_GYRO = 2.0;  // deg/s

// ================================================================
volatile bool mpuInterrupt = false;
void dmpDataReady() { mpuInterrupt = true; }

// ================================================================
// 初始化
// ================================================================
void setup() {
    Serial.begin(115200);
    while (!Serial);

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    Wire.setClock(400000);
#endif

    Serial.println(F("\nInitializing MPU6050..."));
    mpu.initialize();
    pinMode(INTERRUPT_PIN, INPUT);

    if (!mpu.testConnection())
        Serial.println("MPU6050 connection failed");
    else
        Serial.println("MPU6050 OK");

    // DMP 初始化
    Serial.println("DMP init...");
    uint8_t devStatus = mpu.dmpInitialize();

    // (可选)自定义偏移，根据你的芯片调整
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788);

    if (devStatus == 0) {
        Serial.println("Calibrating...");
        mpu.CalibrateAccel(6);
        mpu.CalibrateGyro(6);
        mpu.PrintActiveOffsets();

        mpu.setDMPEnabled(true);
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        dmpReady = true;
        packetSize = mpu.dmpGetFIFOPacketSize();

        Serial.println("DMP Ready");
        lastTime = millis();
    } else {
        Serial.print("DMP Fail: ");
        Serial.println(devStatus);
    }

    pinMode(LED_PIN, OUTPUT);
}

// ================================================================
// 主循环
// ================================================================
void loop() {
    if (!dmpReady) return;

    // DMP packet ready
    if (mpu.dmpGetCurrentFIFOPacket(fifoBuffer)) {

        // ① 获取姿态（四元数 + 重力向量）
        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

        // ② 获取线性加速度（去重力）
        mpu.dmpGetAccel(&aa, fifoBuffer);
        mpu.dmpGetLinearAccel(&aaReal, &aa, &gravity);
        mpu.dmpGetLinearAccelInWorld(&aaWorld, &aaReal, &q);

        // aaWorld 单位 = LSB => 1 LSB = 1/8192 g
        float ax_ms2 = (float)aaWorld.x / 8192.0 * 9.81;

        // === 🚗 假设 X 朝前 ===
        acc_forward = ax_ms2;

        // 时间差
        unsigned long now = millis();
        float dt = (now - lastTime) / 1000.0;
        lastTime = now;

        // ======== ZUPT：静止自动归零 =========
        float gyroMag = abs(aaReal.x)+abs(aaReal.y)+abs(aaReal.z);
        if (abs(acc_forward) < TH_ACC && gyroMag < TH_GYRO) {
            vel = 0;
        } else {
            // ======== 积分计算速度和距离 =========
            vel  += acc_forward * dt;
            dist += vel * dt;
        }

        // 输出值
        Serial.print("acc=");    Serial.print(acc_forward);
        Serial.print("\t v=");   Serial.print(vel);
        Serial.print("\t s=");   Serial.println(dist);

        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
}
