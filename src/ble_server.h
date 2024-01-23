#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SERVICE_UUID "f85015df-6af5-4ee3-a8cb-a8f7250d4466" // General service for configuring WiFi in all my products
#define WIFI_NAME_UUID "bea80929-aa42-4641-a0c2-8f08b70e0aaa"
#define WIFI_PASSWORD_UUID "22643f77-dcfd-4e01-9b9f-bb63692a215f"
// Processes values written to commandCharacteristic.
// There are action commands and configurtation commands

BLECharacteristic *nameCharacteristic;
BLECharacteristic *passwordCharacteristic;

bool deviceConnected = false;


class MyCallbacks : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string uuid = pCharacteristic->getUUID().toString();
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
        if (uuid == WIFI_NAME_UUID){
            USBSerial.print("Wifi name: "); USBSerial.println(value.c_str());
            wifi_ssid = String(value.c_str());

            
        }else if (uuid == WIFI_PASSWORD_UUID){
            USBSerial.print("Wifi pwd: "); USBSerial.println(value.c_str());           
            wifi_password = String(value.c_str());
            writePreferences();
            WiFi.disconnect();
            
            
        }
    } 
  }
};


MyCallbacks* characteristicCallback = new MyCallbacks();


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    USBSerial.println("Connected to BLE central");
    deviceConnected = true;
    BLEDevice::stopAdvertising();
  };

  void onDisconnect(BLEServer *pServer) {
    Serial.println("Disconnected from BLE central");
    deviceConnected = false;
    BLEDevice::startAdvertising();
  }
};


void setup_ble() {
  Serial.println("Starting BLE work!");

  BLEDevice::init("M5Dial");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  nameCharacteristic = pService->createCharacteristic(
    WIFI_NAME_UUID,
    BLECharacteristic::PROPERTY_WRITE);
    nameCharacteristic->setCallbacks(characteristicCallback);
  passwordCharacteristic = pService->createCharacteristic(
    WIFI_PASSWORD_UUID,
    BLECharacteristic::PROPERTY_WRITE);
    passwordCharacteristic->setCallbacks(characteristicCallback);

  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE Initialized!");
}







#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif