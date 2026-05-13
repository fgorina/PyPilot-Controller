#pragma once
#include <Arduino.h>
#include "Theme.h"

enum class DisplaySaverState { SLEEPING = 0, WAKING = 1, ACTIVE = 2 };

enum class ApState   { STANDBY, ENGAGED };
enum class ApMode    { COMPASS, GPS, WIND, TRUE_WIND, NA };
enum class TackState { NONE, BEGIN, WAITING, TACKING };
enum class TackDir   { PORT, STARBOARD };

struct State {
    DisplaySaverState displaySaver = DisplaySaverState::ACTIVE;

    // Autopilot data
    ApState apState = ApState::STANDBY;
    ApMode  apMode  = ApMode::COMPASS;
    float   heading = 0.0f;   // degrees — current heading from AP
    float   command = 0.0f;   // degrees — heading setpoint

    // Tack
    TackState tackState = TackState::NONE;
    TackDir   tackDir   = TackDir::PORT;

    // Servo telemetry
    float servoVoltage  = 0.0f;
    float servoAmpHr    = 0.0f;
    float servoTemp     = 0.0f;
    float servoPosition = 0.0f;  // degrees

    // Rudder
    float rudderAngle = 0.0f;   // degrees (from rudder.angle sensor)

    // Fly-by-wire rudder control
    bool  rudderMode   = false;
    bool  updateRudder = false;
    float rudderTarget = 0.0f;

    // Set by parse() when servo.position arrives so the net task can run the loop
    bool servoPosDirty = false;

    // --- Colour palette -------------------------------------------------------
    PaletteId paletteId = PaletteId::DAY_WHITE;

    // Returns the active palette (reference to a global const).
    const ColorPalette &pal() const { return paletteById(paletteId); }

    // Advance to the next palette (DAY_GREEN → DAY_WHITE → NIGHT → DAY_GREEN).
    void nextPalette();

    // --- Mode helpers ---------------------------------------------------------
    int         modeIndex()  const;
    const char *modeString() const;  // "Compass" / "GPS" / "Wind" / "True Wind"
    const char *modeCmd()    const;  // pypilot wire value: "compass" / "gps" / ...

    // Parse one PyPilot TCP line; returns true if recognised
    bool parse(const String &line);
};
