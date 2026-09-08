#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "AppConfig.h"
#include "State.h"

// --- Command queue (posted by screens + BLE, consumed by netPylotTask) --------

struct PylotCmd {
    enum class Type : uint8_t {
        ENGAGE, DISENGAGE,
        MODE,           // sval holds the mode string ("compass", "gps", ...)
        HEADING,        // fval holds the heading in degrees
        TACK_PORT, TACK_STBD, CANCEL_TACK,
        RUDDER_TARGET,  // fval holds the target angle in degrees
        RUDDER_STOP,
        RUDDER_JOG      // fval holds the servo speed (+/-0.5); manual, disengaged only
    } type;
    float fval  = 0.0f;
    char  sval[16] = {};
};

// Defined in main.cpp; declared here so screens and ble_server can post to it.
extern QueueHandle_t gCmdQueue;

// Convenience post helpers — safe to call from any task / callback.
// All return true if the command was accepted (queue not full).

inline bool pypilot_cmd_engage() {
    PylotCmd c{}; c.type = PylotCmd::Type::ENGAGE;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_disengage() {
    PylotCmd c{}; c.type = PylotCmd::Type::DISENGAGE;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_mode(const char *mode) {
    PylotCmd c{}; c.type = PylotCmd::Type::MODE;
    strlcpy(c.sval, mode, sizeof(c.sval));
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_heading(float h) {
    PylotCmd c{}; c.type = PylotCmd::Type::HEADING; c.fval = h;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_tack_port() {
  Serial.println("Tacking Port");
    PylotCmd c{}; c.type = PylotCmd::Type::TACK_PORT;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_tack_stbd() {
    Serial.println("Tacking Starboard");
    PylotCmd c{}; c.type = PylotCmd::Type::TACK_STBD;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_cancel_tack() {
    PylotCmd c{}; c.type = PylotCmd::Type::CANCEL_TACK;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_rudder_target(float target) {
    PylotCmd c{}; c.type = PylotCmd::Type::RUDDER_TARGET; c.fval = target;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_rudder_stop() {
    PylotCmd c{}; c.type = PylotCmd::Type::RUDDER_STOP;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}
inline bool pypilot_cmd_rudder_jog(float speed) {
    PylotCmd c{}; c.type = PylotCmd::Type::RUDDER_JOG; c.fval = speed;
    return xQueueSend(gCmdQueue, &c, 0) == pdTRUE;
}

// --- Task params struct -------------------------------------------------------

struct NetPylotParams {
    AppConfig *config;
    State     *state;
};

// Defined in net_pypilot.cpp
void netPylotTask(void *param);
