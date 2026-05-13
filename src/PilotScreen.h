#pragma once
#include "Screen.h"
#include "M5Button.h"

// Screen 1: active autopilot.
//
// Layout (320 × 240):
//   Top bar  (y 0-38):   mode label (left) | STOP button (right)
//   Command  (y 45-80):  "^ 220" — heading setpoint
//   Heading  (y 80-170): large current heading
//   Tack arrows on left/right edges of heading area
//   Buttons  (y 178-235): [-10] [-1] [+1] [+10]
//
// Swipe right → RudderScreen (2).
// Swipe left  → ModeScreen (0).
// STOP button → disengage → ModeScreen (0).
// Tack arrows: hold to show intent, release to commit.

class PilotScreen : public Screen {
public:
    PilotScreen(int width, int height);

    void enter(State &state) override;
    void draw (State &state) override;
    int  run  (const m5::touch_detail_t &t, State &state) override;

private:
    ApState   _lastApState  = ApState::ENGAGED;
    ApMode    _lastApMode   = ApMode::NA;
    float     _lastHeading  = -9999.0f;
    float     _lastCommand  = -9999.0f;
    TackState _lastTackSt   = TackState::NONE;
    TackDir   _lastTackDir  = TackDir::PORT;
    PaletteId _lastPalette  = static_cast<PaletteId>(-1);

    bool          _stopPressed = false;
    int           _armed      = 0;   // -1 port armed, 0 none, +1 stbd armed
    int           _lastArmed  = 0;
    int           _holdDir    = 0;   // direction of in-progress long-press
    bool          _holdActive   = false;
    bool          _waitRelease  = false;  // ignore first release after arming
    unsigned long _holdStart    = 0;
    static constexpr unsigned long HOLD_MS = 600;

    static constexpr int BTN_Y   = 180, BTN_H = 52, BTN_W = 72, BTN_GAP = 5;
    static constexpr int BTN_X0  = 9;
    Button _btn[4];

    void drawTackArrow(M5Canvas *cv, bool port, bool active, bool intent,
                       const ColorPalette &pal) const;
};
