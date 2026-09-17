#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-abcdef123456"

BLEServer *pServer;
BLECharacteristic *pCharacteristic;

int deviceCount = 0;

// ================== 连接管理 ==================
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceCount++;
    Serial.print("设备连接，当前数量: ");
    Serial.println(deviceCount);
  }

  void onDisconnect(BLEServer* pServer) {
    deviceCount--;
    Serial.print("设备断开，当前数量: ");
    Serial.println(deviceCount);

    // ⭐ 必须继续广播
    pServer->startAdvertising();
  }
};

// ================== 数据处理 ==================
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = pCharacteristic->getValue();

    Serial.print("收到数据: ");
    Serial.println(value);

    // 转发给所有设备（手机也能收到）
    pCharacteristic->setValue(value);
    pCharacteristic->notify();
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32 A 启动");

  BLEDevice::init("ESP32_A_Server");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("INIT");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("等待连接...");
}

void loop() {
  delay(1000);
}