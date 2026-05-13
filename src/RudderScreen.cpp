#include "RudderScreen.h"
#include "net_pypilot.h"

RudderScreen::RudderScreen(int w, int h) : Screen(w, h, "RUDDER"),
    _btn{
        Button(BTN_X0 + 0*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "-5", {0,0,0},{0,0,0}),
        Button(BTN_X0 + 1*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "-1", {0,0,0},{0,0,0}),
        Button(BTN_X0 + 2*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "0",  {0,0,0},{0,0,0}),
        Button(BTN_X0 + 3*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "+1", {0,0,0},{0,0,0}),
        Button(BTN_X0 + 4*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "+5", {0,0,0},{0,0,0}),
    }
{}

static constexpr int EXIT_X = 242, EXIT_Y = 4, EXIT_W = 74, EXIT_H = 52;
static constexpr int TGT_Y  = 65;
static constexpr int POS_Y  = 135;

// --- Screen interface --------------------------------------------------------

void RudderScreen::enter(State &state) {
    state.rudderMode   = true;
    state.rudderTarget = state.servoPosition;
    state.updateRudder = false;
    _exitPressed = false;
    _lastAngle   = -9999.0f;
    _lastTarget  = -9999.0f;
    _lastUpdate  = !state.updateRudder;   // force first draw
    _lastPalette = static_cast<PaletteId>(-1);

    const ColorPalette &pal = state.pal();
    ButtonColors off_c = { pal.bg, pal.fg, pal.fg };
    ButtonColors on_c  = { pal.fg, pal.bg, pal.fg };
    for (int i = 0; i < BTN_N; i++) {
        _btn[i].off     = off_c;
        _btn[i].on      = on_c;
        _btn[i].pressed = false;
    }

    Screen::enter(state);
}

void RudderScreen::draw(State &state) {
    const ColorPalette &pal = state.pal();

    canvas->clear(pal.bg);

    // --- Title bar ---
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextSize(1);
    canvas->setTextColor(pal.fg, pal.bg);
    canvas->setTextDatum(ML_DATUM);
    canvas->drawString("RUDDER", 6, 19);

    // --- EXIT button ---
    uint16_t exitFill = _exitPressed ? pal.fg : pal.bg;
    uint16_t exitText = _exitPressed ? pal.bg : pal.fg;
    canvas->fillRoundRect(EXIT_X, EXIT_Y, EXIT_W, EXIT_H, 8, exitFill);
    canvas->drawRoundRect(EXIT_X, EXIT_Y, EXIT_W, EXIT_H, 8, pal.fg);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    canvas->setTextColor(exitText, exitFill);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString("EXIT", EXIT_X + EXIT_W / 2, EXIT_Y + EXIT_H / 2);

    // --- Target (small, orange when loop running) ---
    char buf[24];
    snprintf(buf, sizeof(buf), "%.0f", state.rudderTarget);
    canvas->setFont(&fonts::FreeSansBold24pt7b);
    canvas->setTextSize(1);
    canvas->setTextColor(state.updateRudder ? pal.warn : pal.dim, pal.bg);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString(buf, width / 2, TGT_Y);

    // --- Current position (large) ---
    snprintf(buf, sizeof(buf), "%.0f", state.rudderAngle);
    canvas->setFont(&fonts::DejaVu72);
    canvas->setTextSize(1);
    canvas->setTextColor(pal.fg, pal.bg);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString(buf, width / 2, POS_Y);

    canvas->pushSprite(0, 0);

    // Buttons draw directly to M5.Display (M5Button bypasses canvas)
    ButtonColors off_c = { pal.bg, pal.fg, pal.fg };
    ButtonColors on_c  = { pal.fg, pal.bg, pal.fg };
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextSize(1);
    for (int i = 0; i < BTN_N; i++) {
        _btn[i].off = off_c;
        _btn[i].on  = on_c;
        _btn[i].draw();
    }

    _lastAngle   = state.rudderAngle;
    _lastTarget  = state.rudderTarget;
    _lastUpdate  = state.updateRudder;
    _lastPalette = state.paletteId;
}

int RudderScreen::run(const m5::touch_detail_t &t, State &state) {
    bool changed = (fabsf(state.rudderAngle  - _lastAngle)  >= 0.2f ||
                    fabsf(state.rudderTarget - _lastTarget) >= 0.2f ||
                    state.updateRudder  != _lastUpdate  ||
                    state.paletteId     != _lastPalette);
    if (changed) draw(state);

    // --- Swipe ---
    if (t.wasFlicked()) {
        int dx = t.distanceX();
        int dy = t.distanceY();
        if (abs(dx) > abs(dy) && abs(dx) > 50) {
            state.rudderMode   = false;
            state.updateRudder = false;
            pypilot_cmd_rudder_stop();
            return (dx > 0) ? 0 : 1;
        }
    }

    if (!t.isPressed() && !t.wasClicked() && !t.wasReleased()) return -1;

    int tx = t.x, ty = t.y;

    // --- EXIT button ---
    bool inExit = (tx >= EXIT_X && tx < EXIT_X + EXIT_W &&
                   ty >= EXIT_Y && ty < EXIT_Y + EXIT_H);
    if (inExit && t.wasPressed()) {
        M5.Speaker.tone(1000, 100, 0, false);
        _exitPressed = true;
        draw(state);
    } else if (_exitPressed && t.wasReleased()) {
        M5.Speaker.tone(inExit ? 2000 : 500, 100, 0, false);
        _exitPressed = false;
        if (inExit) {
            state.rudderMode   = false;
            state.updateRudder = false;
            pypilot_cmd_rudder_stop();
            return 0;
        }
        draw(state);
    }

    // --- Rudder buttons ---
    static constexpr float deltas[] = { -5.0f, -1.0f, 0.0f, 1.0f, 5.0f };
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextSize(1);
    for (int i = 0; i < BTN_N; i++) {
        if (_btn[i].handleTouch(t)) {
            if (i == 2) {
                state.rudderTarget = 0.0f;
            } else {
                state.rudderTarget += deltas[i];
                state.rudderTarget  = constrain(state.rudderTarget, -MAX_RUDDER, MAX_RUDDER);
            }
            state.updateRudder = true;
            pypilot_cmd_rudder_target(state.rudderTarget);
            draw(state);
            break;
        }
    }

    return -1;
}
