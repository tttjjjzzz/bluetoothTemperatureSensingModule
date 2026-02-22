#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// Must match the slave exactly
#define SERVICE_UUID        "181A"
#define CHARACTERISTIC_UUID "2A6E"
#define SLAVE_NAME          "ESP32-TempSensor"

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteChar = nullptr;
BLEAdvertisedDevice* targetDevice = nullptr;

bool deviceFound = false;
bool connected = false;

// --- Scan Callback: fires when a BLE device is discovered ---
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.printf("Found device: %s\n", advertisedDevice.getName().c_str());

    if (advertisedDevice.getName() == SLAVE_NAME) {
      Serial.println("Target device found! Stopping scan...");
      BLEDevice::getScan()->stop();
      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      deviceFound = true;
    }
  }
};

// --- Connect to the slave and get the characteristic ---
bool connectToSlave() {
  Serial.printf("Connecting to %s...\n", targetDevice->getAddress().toString().c_str());

  pClient = BLEDevice::createClient();
  pClient->connect(targetDevice);
  Serial.println("Connected to slave!");

  // Get the service
  BLERemoteService* pService = pClient->getService(SERVICE_UUID);
  if (pService == nullptr) {
    Serial.println("ERROR: Could not find service on slave.");
    pClient->disconnect();
    return false;
  }

  // Get the characteristic
  pRemoteChar = pService->getCharacteristic(CHARACTERISTIC_UUID);
  if (pRemoteChar == nullptr) {
    Serial.println("ERROR: Could not find characteristic on slave.");
    pClient->disconnect();
    return false;
  }

  Serial.println("Ready to read temperature!");
  return true;
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32-Master");

  // Start scanning
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setActiveScan(true); // Active scan gets more info from devices
  pScan->start(10, false);    // Scan for 10 seconds
}

void loop() {
  // Once device is found, connect
  if (deviceFound && !connected) {
    connected = connectToSlave();
  }

  // Once connected, request temperature every 3 seconds
  if (connected) {
    if (!pClient->isConnected()) {
      Serial.println("Lost connection. Rescanning...");
      connected = false;
      deviceFound = false;
      BLEDevice::getScan()->start(10, false); // Rescan
      return;
    }

    if (pRemoteChar->canRead()) {
      String value = pRemoteChar->readValue().c_str();
      Serial.printf("Temperature from slave: %s\n", value.c_str());
    }

    delay(3000); // Request every 3 seconds
  }
}
