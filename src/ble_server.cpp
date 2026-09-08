#include "ble_server.h"
#include "net_pypilot.h"
#include <WiFi.h>

// --- Singleton ---------------------------------------------------------------

BleServer *BleServer::_instance = nullptr;

// --- Constructor / setup -----------------------------------------------------

BleServer::BleServer(AppConfig *cfg, void (*onSave)())
    : _config(cfg), _onSave(onSave) {
    _instance = this;
}

void BleServer::setup() {
    BLEDevice::init("PYPILOT");

    BLEServer  *srv = BLEDevice::createServer();
    srv->setCallbacks(new SrvCallbacks());

    BLEService *svc = srv->createService(BLE_SERVICE_UUID);

    // WiFi name (write-only)
    BLECharacteristic *nameChr = svc->createCharacteristic(
        BLE_WIFI_NAME_UUID, BLECharacteristic::PROPERTY_WRITE);
    nameChr->setCallbacks(new ChrCallbacks());

    // WiFi password (write-only)
    BLECharacteristic *passChr = svc->createCharacteristic(
        BLE_WIFI_PASS_UUID, BLECharacteristic::PROPERTY_WRITE);
    passChr->setCallbacks(new ChrCallbacks());

    // Command (write-only)
    BLECharacteristic *cmdChr = svc->createCharacteristic(
        BLE_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
    cmdChr->setCallbacks(new ChrCallbacks());

    // State (read + notify)
    _stateChr = svc->createCharacteristic(
        BLE_STATE_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

    svc->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("BLE advertising as \"%s\"\n", _config->deviceName.c_str());
}

// --- Notify helpers ----------------------------------------------------------

void BleServer::_notify(const char *msg) {
    if (!_connected || !_stateChr) return;
    _stateChr->setValue((uint8_t *)msg, strlen(msg));
    _stateChr->notify();
}

void BleServer::notifyAll(const State &s) {
    if (!_connected) return;
    char buf[32];

    // Preserve original wire protocol (Apple Watch compatibility)
    snprintf(buf, sizeof(buf), "H%.0f", s.heading);  _notify(buf);
    snprintf(buf, sizeof(buf), "C%.0f", s.command);  _notify(buf);
    snprintf(buf, sizeof(buf), "R%.0f", s.rudderAngle); _notify(buf);

    _notify(s.apState == ApState::ENGAGED ? "E" : "D");

    snprintf(buf, sizeof(buf), "M%d", s.modeIndex());  _notify(buf);
    snprintf(buf, sizeof(buf), "T%d", (int)s.tackState); _notify(buf);
    snprintf(buf, sizeof(buf), "U%d", s.tackDir == TackDir::STARBOARD ? 1 : -1); _notify(buf);
}

// --- Command dispatch --------------------------------------------------------

void BleServer::_handleCommand(const std::string &cmd) {
    if (cmd.empty()) return;
    char s = cmd[0];

    if (s == 'E') {
        pypilot_cmd_engage();
    } else if (s == 'D') {
        pypilot_cmd_disengage();
    } else if (s == 'M') {
        // Mode: "Mcompass" / "Mgps" / "Mwind" / "Mtrue wind"
        std::string mode = cmd.substr(1);
        pypilot_cmd_mode(mode.c_str());
    } else if (s == 'C') {
        float h = atof(cmd.substr(1).c_str());
        pypilot_cmd_heading(h);
    } else if (s == 'T') {
        // 'TP' = tack port, 'TS' = tack starboard
        if (cmd.size() > 1 && cmd[1] == 'S')
            pypilot_cmd_tack_stbd();
        else
            pypilot_cmd_tack_port();
    } else if (s == 'X') {
        pypilot_cmd_cancel_tack();
    } else if (s == 'R') {
        // Manual rudder jog (disengaged only): 'RP' = port (+), else starboard (-)
        float speed = (cmd.size() > 1 && cmd[1] == 'P') ? 0.5f : -0.5f;
        pypilot_cmd_rudder_jog(speed);
    } else if (s == 'Z') {
        // Rudder target: "Z-5.0"  or just "Z" for centre
        float target = (cmd.size() > 1) ? atof(cmd.substr(1).c_str()) : 0.0f;
        pypilot_cmd_rudder_target(target);
    } else if (s == 'I') {
        // Full-state sync request — force an immediate notifyAll from the loop.
        _syncRequested = true;
    }
}

// --- BLE callbacks -----------------------------------------------------------

void BleServer::SrvCallbacks::onConnect(BLEServer *) {
    Serial.println("BLE: central connected");
    if (_instance) _instance->_connected = true;
    BLEDevice::stopAdvertising();
}

void BleServer::SrvCallbacks::onDisconnect(BLEServer *) {
    Serial.println("BLE: central disconnected");
    if (_instance) _instance->_connected = false;
    BLEDevice::startAdvertising();
}

void BleServer::ChrCallbacks::onWrite(BLECharacteristic *chr) {
    if (!_instance) return;
    std::string uuid  = chr->getUUID().toString().c_str();
    std::string value = chr->getValue().c_str();
    if (value.empty()) return;

    if (uuid == BLE_WIFI_NAME_UUID) {
        _instance->_config->wifiSsid = String(value.c_str());
        Serial.printf("BLE: WiFi SSID set to \"%s\"\n", value.c_str());

    } else if (uuid == BLE_WIFI_PASS_UUID) {
        _instance->_config->wifiPassword = String(value.c_str());
        if (_instance->_onSave) _instance->_onSave();
        WiFi.disconnect();
        Serial.println("BLE: WiFi credentials updated, reconnecting...");

    } else if (uuid == BLE_CMD_UUID) {
        Serial.printf("BLE cmd: %s\n", value.c_str());
        _instance->_handleCommand(value);
    }
}
