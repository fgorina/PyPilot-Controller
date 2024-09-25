#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#define SERVICE_UUID "f85015df-6af5-4ee3-a8cb-a8f7250d4466" // General service for configuring WiFi in all my products
#define WIFI_NAME_UUID "bea80929-aa42-4641-a0c2-8f08b70e0aaa"
#define WIFI_PASSWORD_UUID "22643f77-dcfd-4e01-9b9f-bb63692a215f"
#define COMMAND_UUID "02804fff-8c38-485f-964a-474dc4f179b2"
#define STATE_UUID "de7f5161-6f48-4c9a-aacc-6079082e6cc7"
// Processes values written to commandCharacteristic.
// There are action commands and configurtation commands

BLECharacteristic *nameCharacteristic;
BLECharacteristic *passwordCharacteristic;
BLECharacteristic *commandCharacteristic;
BLECharacteristic *stateCharacteristic;

bool deviceConnected = false;
char buffer[256];

void sendInfo(){
  if (shipDataModel.steering.autopilot.ap_state.st == ap_state_e::ENGAGED){
    setStateCharacteristicEnabled(1);
    setStateCharacteristicMode(modeToInt(shipDataModel.steering.autopilot.ap_mode.mode)); 
  }else{
    setStateCharacteristicEnabled(0);
    setStateCharacteristicMode(4); 
  }
  setStateCharacteristicCommand(shipDataModel.steering.autopilot.command.deg); 
  setStateCharacteristicHeading(shipDataModel.steering.autopilot.heading.deg);
  setStateCharacteristicRudder(shipDataModel.steering.rudder_angle.deg);
  setStateCharacteristicTackState(shipDataModel.steering.autopilot.tack.st);
  setStateCharacteristicTackDirection(shipDataModel.steering.autopilot.tack.direction);
}

void doCommand(String command){

  char s = command[0];

  if (s == 'E'){
    pypilot_send_engage(pypClient.c);
  }else if (s == 'D'){
    pypilot_send_disengage(pypClient.c);
  }else if (s == 'M'){
    String mode = command.substring(1);
    USBSerial.print("eStting mode to "); USBSerial.println(mode);
    if(mode == "rudder"){
      pypilot_send_disengage(pypClient.c);
    }else{
      pypilot_send_engage(pypClient.c);
      if (mode == "gps"){
        pypilot_send_mode(pypClient.c, AP_MODE_GPS);
      }else if (mode == "wind"){
        pypilot_send_mode(pypClient.c, AP_MODE_WIND);
      }else if (mode == "true wind"){
        pypilot_send_mode(pypClient.c, AP_MODE_WIND_TRUE);
      }else if (mode == "compass"){
        pypilot_send_mode(pypClient.c, AP_MODE_COMPASS);
      }
    }
  } else if(s == 'C'){
    String s_value = command.substring(1);
    float heading = atof(s_value.c_str());
     USBSerial.print("Setting command to "); USBSerial.println(s_value);
    pypilot_send_command(pypClient.c, heading);
  }else if (s ==  'T'){
    char direction = command[1];
    if (direction == 'P'){
      pypilot_send_tack(pypClient.c, "port");
    }else{
      pypilot_send_tack(pypClient.c, "starboard");
    }
  }else if (s == 'X'){
    pypilot_send_cancel_tack(pypClient.c);
  }else if (s == 'R'){
    char direction = command[1];
    if (direction == 'P'){
      pypilot_send_rudder_command(pypClient.c, 1.0);
    }else {
      pypilot_send_rudder_command(pypClient.c, -1.0);
    }
    
  }else if (s == 'Z'){
    if (strlen(command.c_str()) > 1){
      String s_value = command.substring(1);

      float angle = atof(s_value.c_str());
      USBSerial.print("Setting rudder angle to "); USBSerial.println(s_value);
      edit_position = angle;
    }else{
      edit_position = 0.0;
    }
    sendRudderCommand();
  }else if (s == 'I'){  // Send all information to sync states
    sendInfo();
  }




}


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
            pypilot_tcp_port = 0;
            writePreferences();
            WiFi.disconnect();
         }else if (uuid == COMMAND_UUID){
            USBSerial.print("Command received: "); USBSerial.println(value.c_str());        
            doCommand(String(value.c_str()));
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
    USBSerial.println("Disconnected from BLE central");
    deviceConnected = false;
    BLEDevice::startAdvertising();
  }
};




void setup_ble() {
  USBSerial.println("Starting BLE work!");

  BLEDevice::init("PyPilot");
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

  commandCharacteristic = pService->createCharacteristic(
    COMMAND_UUID,
    BLECharacteristic::PROPERTY_WRITE);
    commandCharacteristic->setCallbacks(characteristicCallback);

  stateCharacteristic = pService->createCharacteristic(
    STATE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    stateCharacteristic->setCallbacks(characteristicCallback);

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