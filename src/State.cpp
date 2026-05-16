#include "State.h"

void State::nextPalette() {
    paletteId = static_cast<PaletteId>(
        (static_cast<int>(paletteId) + 1) % PALETTE_COUNT);
}

int State::modeIndex() const {
    switch (apMode) {
        case ApMode::COMPASS:   return 0;
        case ApMode::GPS:       return 1;
        case ApMode::WIND:      return 2;
        case ApMode::TRUE_WIND: return 3;
        default:                return 0;
    }
}

const char *State::modeString() const {
    switch (apMode) {
        case ApMode::COMPASS:   return "Compass";
        case ApMode::GPS:       return "GPS";
        case ApMode::WIND:      return "Wind";
        case ApMode::TRUE_WIND: return "T Wind";
        default:                return "---";
    }
}

const char *State::modeCmd() const {
    switch (apMode) {
        case ApMode::COMPASS:   return "compass";
        case ApMode::GPS:       return "gps";
        case ApMode::WIND:      return "wind";
        case ApMode::TRUE_WIND: return "true wind";
        default:                return "compass";
    }
}

bool State::parse(const String &line) {
    if (line.isEmpty()) return false;

    if (line.startsWith("ap.heading=")) {
        heading = strtof(line.substring(11).c_str(), nullptr);
        return true;
    }
    if (line.startsWith("ap.heading_command=")) {
        command = strtof(line.substring(19).c_str(), nullptr);
        return true;
    }
    if (line.startsWith("ap.enabled=true")) {
        apState = ApState::ENGAGED;
        return true;
    }
    if (line.startsWith("ap.enabled=false")) {
        apState = ApState::STANDBY;
        return true;
    }
    if (line.startsWith("ap.mode=\"")) {
        // format: ap.mode="compass"  (trailing quote included in line)
        String mode = line.substring(9, line.length() - 1);
        if      (mode == "compass")    apMode = ApMode::COMPASS;
        else if (mode == "gps")        apMode = ApMode::GPS;
        else if (mode == "wind")       apMode = ApMode::WIND;
        else if (mode == "true wind")  apMode = ApMode::TRUE_WIND;
        return true;
    }
    if (line.startsWith("ap.tack.state=\"")) {
        String s = line.substring(15, line.length() - 1);
        if      (s == "begin")   tackState = TackState::BEGIN;
        else if (s == "waiting") tackState = TackState::WAITING;
        else if (s == "tacking") tackState = TackState::TACKING;
        else                     tackState = TackState::NONE;
        return true;
    }
    if (line.startsWith("ap.tack.direction=\"")) {
        String d = line.substring(19, line.length() - 1);
        tackDir = (d == "starboard") ? TackDir::STARBOARD : TackDir::PORT;
        return true;
    }
    if (line.startsWith("servo.voltage=")) {
        servoVoltage = strtof(line.substring(14).c_str(), nullptr);
        return true;
    }
    if (line.startsWith("servo.amp_hours=")) {
        servoAmpHr = strtof(line.substring(16).c_str(), nullptr);
        return true;
    }
    if (line.startsWith("servo.controller_temp=")) {
        servoTemp = strtof(line.substring(22).c_str(), nullptr);
        return true;
    }
    if (line.startsWith("servo.position=")) {
        servoPosition  = strtof(line.substring(15).c_str(), nullptr);
        servoPosDirty  = true;
        return true;
    }
    if (line.startsWith("rudder.angle=")) {
        rudderAngle = strtof(line.substring(13).c_str(), nullptr);
        return true;
    }
    return false;
}
