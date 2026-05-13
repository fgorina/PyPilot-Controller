#pragma once
#include "Screen.h"

// Screen 0: mode / standby selection.
//
// Layout (320 × 240):
//   Four quadrants separated by a cross at (160, 120):
//     TL Compass  |  TR GPS
//     ────────────+────────────
//     BL True Wind|  BR Wind
//   Centre circle (r = 48): tap → RudderScreen
//
// Tap a quadrant → engage AP in that mode → switch to PilotScreen (1).
// Swipe right  → PilotScreen (1).
// Swipe left   → RudderScreen (2).

class ModeScreen : public Screen {
public:
    ModeScreen(int width, int height);

    void enter(State &state) override;
    void draw (State &state) override;
    int  run  (const m5::touch_detail_t &t, State &state) override;

private:
    int       _pressedZone = 0;   // 0=none 1=circle 2=compass 3=GPS 4=T.Wind 5=Wind

    // Snapshot for change detection
    ApState   _lastApState = ApState::STANDBY;
    ApMode    _lastApMode  = ApMode::NA;
    PaletteId _lastPalette = static_cast<PaletteId>(-1);

    static constexpr int CX = 160;  // screen centre x
    static constexpr int CY = 120;  // screen centre y
    static constexpr int CR = 48;   // centre circle radius

    static void drawQuadrant(M5Canvas *cv, int cx, int cy,
                              const char *label, bool active,
                              const ColorPalette &pal);
    static void drawCentreCircle(M5Canvas *cv, const ColorPalette &pal);
};
