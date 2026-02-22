#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- LM75A ---
#define LM75A_ADDRESS 0x48
#define LM75A_TEMP_REG 0x00

// --- BLE UUIDs (standard Environmental Sensing service) ---
#define SERVICE_UUID        "181A"          // Environmental Sensing
#define CHARACTERISTIC_UUID "2A6E"          // Temperature (standard GATT)

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// --- BLE Connection Callbacks ---
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Client connected!");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Client disconnected. Restarting advertising...");
    pServer->startAdvertising();
  }
};

// --- Read LM75A Temperature ---
float readTemperature() {
  Wire.beginTransmission(LM75A_ADDRESS);
  Wire.write(LM75A_TEMP_REG);
  Wire.endTransmission();

  Wire.requestFrom(LM75A_ADDRESS, 2);
  if (Wire.available() == 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();

    // Combine bytes — 11-bit value in upper bits
    int16_t raw = ((int16_t)(msb << 8) | lsb) >> 5;

    // Handle two's complement for negative temps
    if (raw & 0x400) raw |= 0xF800;

    return raw * 0.125f;
  }
  return -999.0f; // Error sentinel
}

void setup() {
  Serial.begin(9600);
  Wire.begin(); // SDA=21, SCL=22 by default

  // Initialize BLE
  BLEDevice::init("ESP32-TempSensor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create Characteristic with Read + Notify properties
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  // Add descriptor so clients can subscribe to notifications
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  // Start advertising so devices can discover the ESP32
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("BLE advertising started. Waiting for connections...");
}

void loop() {
  float tempC = readTemperature();

  if (tempC != -999.0f) {
    Serial.printf("Temperature: %.2f °C\n", tempC);

    // Format as string and update BLE characteristic
    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "%.2f C", tempC);
    pCharacteristic->setValue(tempStr);

    // Push notification to connected client
    if (deviceConnected) {
      pCharacteristic->notify();
    }
  } else {
    Serial.println("Error reading sensor!");
  }

  delay(2000); // Read every 2 seconds
}