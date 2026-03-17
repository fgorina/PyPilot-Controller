#ifndef BLE_CALYPSO_H
#define BLE_CALYPSO_H

// BLE Central client for Calypso ULTRASONIC anemometer.
// BLEDevice must already be initialized (by ble_server.h) before using this.

#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define CALYPSO_SERVICE              "180D"
#define WIND_SPEED_CHARACTERISTIC    "2A39"
#define DATA_RATE_CHARACTERISTIC     "A002"
#define SENSORS_CHARACTERISTIC       "A003"
#define STATUS_CHARACTERISTIC        "A001"

typedef struct {
  int windSpeed    = 0;  // m/s * 100
  int windDirection = 0; // degrees
  int batteryLevel = 0;  // percent
  int temperature  = 0;  // degrees C
  int roll         = 0;  // degrees
  int pitch        = 0;  // degrees
  int compass      = 0;  // degrees
} wind_data_t;

typedef struct {
  uint8_t dataRate    = 1; // 1, 4, or 8 Hz
  uint8_t sensorValue = 0; // compass/accel enable flag
  uint8_t status      = 2;
} calypso_device_info_t;

static wind_data_t           windData;
static calypso_device_info_t calypsoDeviceInfo;

static bool               calypsoFound     = false;
static bool               calypsoConnected = false;
static BLEAdvertisedDevice* calypsoDevice  = nullptr;
static BLEClient*           calypsoClient  = nullptr;
static BLERemoteService*    calypsoService = nullptr;
static BLERemoteCharacteristic* windSpeedChar = nullptr;

// Forward declaration — defined in net_signalk.h
void sendWindData(int windSpeed, int windDirection, int batteryLevel);

class CalypsoAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (strcmp(advertisedDevice.getName().c_str(), "ULTRASONIC") == 0) {
      calypsoDevice = new BLEAdvertisedDevice(advertisedDevice);
      calypsoFound  = true;
      Serial.printf("Calypso: found %s\n", advertisedDevice.toString().c_str());
    }
  }
};

class CalypsoClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    calypsoConnected = true;
    Serial.println("Calypso: connected");
  }
  void onDisconnect(BLEClient* pclient) {
    calypsoConnected = false;
    calypsoService   = nullptr;
    windSpeedChar    = nullptr;
    Serial.println("Calypso: disconnected");
  }
};

static void decodeWindData(uint8_t* data, size_t length) {
  if (length >= 2)  windData.windSpeed    = data[1] * 256 + data[0];
  if (length >= 4)  windData.windDirection = data[3] * 256 + data[2];
  if (length >= 5)  windData.batteryLevel  = data[4] * 10;
  if (length >= 6)  windData.temperature   = data[5] - 100;
  if (length >= 7)  windData.roll          = data[6] - 90;
  if (length >= 8)  windData.pitch         = data[7] - 90;
  if (length >= 10) windData.compass       = 360 - (data[9] * 256 + data[8]);
}

static void calypsoNotifyCallback(
    BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  decodeWindData(pData, length);
  sendWindData(windData.windSpeed, windData.windDirection, windData.batteryLevel);
}

uint8_t calypso_getUInt8(BLERemoteService* service, const char* charUuid) {
  BLERemoteCharacteristic* c = service->getCharacteristic(charUuid);
  return (c != nullptr) ? c->readUInt8() : 0;
}

void calypso_putUInt8(BLERemoteService* service, const char* charUuid, uint8_t value) {
  BLERemoteCharacteristic* c = service->getCharacteristic(charUuid);
  if (c != nullptr && c->canWrite()) c->writeValue(value);
}

bool calypso_connect() {
  calypsoClient->connect(calypsoDevice);

  calypsoService = calypsoClient->getService(CALYPSO_SERVICE);
  if (calypsoService == nullptr) {
    Serial.println("Calypso: wind service not found");
    calypsoClient->disconnect();
    return false;
  }

  calypsoDeviceInfo.dataRate    = calypso_getUInt8(calypsoService, DATA_RATE_CHARACTERISTIC);
  calypsoDeviceInfo.sensorValue = calypso_getUInt8(calypsoService, SENSORS_CHARACTERISTIC);
  Serial.printf("Calypso: dataRate=%d sensors=%d\n",
                calypsoDeviceInfo.dataRate, calypsoDeviceInfo.sensorValue);

  windSpeedChar = calypsoService->getCharacteristic(WIND_SPEED_CHARACTERISTIC);
  if (windSpeedChar == nullptr) {
    Serial.println("Calypso: wind characteristic not found");
    calypsoClient->disconnect();
    return false;
  }

  windSpeedChar->registerForNotify(calypsoNotifyCallback);
  Serial.println("Calypso: registered for wind notifications");
  return true;
}

void setup_calypso_ble() {
  if (calypsoConnected) return;

  calypsoFound = false;
  Serial.println("Calypso: scanning (5 s)...");

  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new CalypsoAdvertisedDeviceCallbacks());
  pScan->setActiveScan(true);
  pScan->setInterval(100);
  pScan->setWindow(99);
  pScan->start(5, false);
  pScan->clearResults();

  if (calypsoFound) {
    Serial.printf("Calypso: connecting to %s\n",
                  calypsoDevice->getAddress().toString().c_str());
    calypsoClient = BLEDevice::createClient();
    calypsoClient->setClientCallbacks(new CalypsoClientCallback());
    calypso_connect();
  } else {
    Serial.println("Calypso: not found in scan");
  }
}

#endif // BLE_CALYPSO_H
