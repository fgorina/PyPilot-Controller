#pragma once
#include "Screen.h"
#include "M5Button.h"

// Screen 2: fly-by-wire rudder control.
//
// Layout (320 × 240):
//   Title bar (y 0-38):  "RUDDER" label | EXIT button (orange)
//   Target    (y ~65):   rudder setpoint (small, orange when loop active)
//   Position  (y ~140):  current rudder angle (large)
//   Buttons   (y 188-235): [-5] [-1] [0] [+1] [+5]
//
// EXIT / swipe right → ModeScreen (0).
// Swipe left → PilotScreen (1).

class RudderScreen : public Screen {
public:
    RudderScreen(int width, int height);

    void enter(State &state) override;
    void draw (State &state) override;
    int  run  (const m5::touch_detail_t &t, State &state) override;

private:
    float     _lastAngle   = -9999.0f;
    float     _lastTarget  = -9999.0f;
    bool      _lastUpdate  = false;
    PaletteId _lastPalette = static_cast<PaletteId>(-1);
    bool      _exitPressed = false;

    static constexpr float MAX_RUDDER = 30.0f;

    static constexpr int BTN_Y   = 180;
    static constexpr int BTN_H   = 52;
    static constexpr int BTN_W   = 58;
    static constexpr int BTN_N   = 5;
    static constexpr int BTN_GAP = 5;
    static constexpr int BTN_X0  = 9;
    Button _btn[5];
};
