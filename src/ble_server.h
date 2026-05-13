#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "AppConfig.h"
#include "State.h"

// BLE UUIDs — unchanged from original for Apple Watch compatibility
#define BLE_SERVICE_UUID   "f85015df-6af5-4ee3-a8cb-a8f7250d4466"
#define BLE_WIFI_NAME_UUID "bea80929-aa42-4641-a0c2-8f08b70e0aaa"
#define BLE_WIFI_PASS_UUID "22643f77-dcfd-4e01-9b9f-bb63692a215f"
#define BLE_CMD_UUID       "02804fff-8c38-485f-964a-474dc4f179b2"
#define BLE_STATE_UUID     "de7f5161-6f48-4c9a-aacc-6079082e6cc7"

class BleServer {
public:
    BleServer(AppConfig *cfg, void (*onSave)());

    void setup();

    // Call from main loop to push state to connected central.
    // Sends all values so the Watch can sync after (re)connect.
    void notifyAll(const State &s);

    bool isConnected() const { return _connected; }

private:
    AppConfig           *_config;
    void               (*_onSave)();
    BLECharacteristic  *_stateChr = nullptr;
    bool                _connected = false;

    // Singleton pointer used by static BLE callbacks
    static BleServer *_instance;

    void _notify(const char *msg);
    void _handleCommand(const std::string &cmd);

    // BLE callback classes (declared inside, defined in .cpp)
    class SrvCallbacks  : public BLEServerCallbacks {
        void onConnect   (BLEServer *) override;
        void onDisconnect(BLEServer *) override;
    };
    class ChrCallbacks  : public BLECharacteristicCallbacks {
        void onWrite(BLECharacteristic *) override;
    };
};
