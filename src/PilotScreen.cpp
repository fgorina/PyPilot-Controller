#include "PilotScreen.h"
#include "net_pypilot.h"

PilotScreen::PilotScreen(int w, int h) : Screen(w, h, "PILOT"),
    _btn{
        Button(BTN_X0 + 0*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "-10", {0,0,0},{0,0,0}),
        Button(BTN_X0 + 1*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "-1",  {0,0,0},{0,0,0}),
        Button(BTN_X0 + 2*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "+1",  {0,0,0},{0,0,0}),
        Button(BTN_X0 + 3*(BTN_W+BTN_GAP), BTN_Y, BTN_W, BTN_H, true, "+10", {0,0,0},{0,0,0}),
    }
{}

// --- Layout constants --------------------------------------------------------

static constexpr int TOP_H   = 38;
static constexpr int MARGIN  = 4;   // uniform screen edge margin
static constexpr int STOP_X  = 242, STOP_Y = MARGIN, STOP_W = 74, STOP_H = 74, STOP_CUT = 16;
static constexpr int CMD_Y   = 50;    // command line centre y
static constexpr int HDG_Y   = 130;   // heading number centre y
static constexpr int ARR_HALF = 22, ARR_DEPTH = 28;
static constexpr int TACK_W  = 80;   // touch-zone width for tack arrows
static constexpr int TACK_Y0 = 80;   // top of tack touch zone

// --- Helpers -----------------------------------------------------------------

void PilotScreen::drawTackArrow(M5Canvas *cv, bool port,
                                 bool active, bool intent,
                                 const ColorPalette &pal) const {
    uint16_t col = intent ? pal.warn
                 : active ? pal.danger
                           : pal.fg;
    if (port) {
        cv->fillTriangle(MARGIN,              HDG_Y,
                         MARGIN + ARR_DEPTH,  HDG_Y - ARR_HALF,
                         MARGIN + ARR_DEPTH,  HDG_Y + ARR_HALF, col);
    } else {
        cv->fillTriangle(width - MARGIN,              HDG_Y,
                         width - MARGIN - ARR_DEPTH,  HDG_Y - ARR_HALF,
                         width - MARGIN - ARR_DEPTH,  HDG_Y + ARR_HALF, col);
    }
}

// --- Screen interface --------------------------------------------------------

void PilotScreen::enter(State &state) {
    _lastApState = ApState::STANDBY;
    _lastApMode  = ApMode::NA;
    _lastHeading = -9999.0f;
    _lastCommand = -9999.0f;
    _lastTackSt  = TackState::BEGIN;
    _lastPalette = static_cast<PaletteId>(-1);
    _stopPressed = false;
    _armed      = 0;
    _lastArmed  = 0;
    _holdDir     = 0;
    _holdActive  = false;
    _waitRelease = false;

    const ColorPalette &pal = state.pal();
    ButtonColors off_c = { pal.bg, pal.fg, pal.fg };
    ButtonColors on_c  = { pal.fg, pal.bg, pal.fg };
    for (int i = 0; i < 4; i++) {
        _btn[i].off     = off_c;
        _btn[i].on      = on_c;
        _btn[i].pressed = false;
    }

    Screen::enter(state);
}

void PilotScreen::draw(State &state) {
    const ColorPalette &pal = state.pal();

    canvas->clear(pal.bg);

    // --- Top bar: mode label ---
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextSize(1);
    canvas->setTextColor(pal.fg, pal.bg);
    canvas->setTextDatum(ML_DATUM);
    canvas->drawString(state.modeString(), 6, TOP_H / 2);

    // --- STOP button (octagon) ---
    {
        uint16_t stopFill = _stopPressed ? TFT_WHITE : TFT_RED;
        uint16_t stopText = _stopPressed ? TFT_RED   : TFT_WHITE;
        int ox = STOP_X, oy = STOP_Y, ow = STOP_W, oh = STOP_H, c = STOP_CUT;
        int cx = ox + ow/2, cy = oy + oh/2;
        int16_t vx[8] = { int16_t(ox+c),    int16_t(ox+ow-c), int16_t(ox+ow), int16_t(ox+ow),
                           int16_t(ox+ow-c), int16_t(ox+c),    int16_t(ox),    int16_t(ox) };
        int16_t vy[8] = { int16_t(oy),       int16_t(oy),      int16_t(oy+c),  int16_t(oy+oh-c),
                           int16_t(oy+oh),    int16_t(oy+oh),   int16_t(oy+oh-c), int16_t(oy+c) };
        for (int i = 0; i < 8; i++)
            canvas->fillTriangle(cx, cy, vx[i], vy[i], vx[(i+1)%8], vy[(i+1)%8], stopFill);
        for (int i = 0; i < 8; i++)
            canvas->drawLine(vx[i], vy[i], vx[(i+1)%8], vy[(i+1)%8], pal.fg);
        canvas->setFont(&fonts::FreeSansBold12pt7b);
        canvas->setTextSize(1);
        canvas->setTextColor(stopText, stopFill);
        canvas->setTextDatum(MC_DATUM);
        canvas->drawString("STOP", cx, cy);
    }

    // --- Command (setpoint) ---
    char buf[20];
    snprintf(buf, sizeof(buf), "%.0f", state.command);
    canvas->setFont(&fonts::FreeSansBold24pt7b);
    canvas->setTextSize(1);
    canvas->setTextColor(pal.fg, pal.bg);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString(buf, width / 2, CMD_Y);

    // --- Heading (large) ---
    snprintf(buf, sizeof(buf), "%.0f", state.heading);
    canvas->setFont(&fonts::DejaVu72);
    canvas->setTextSize(1);
    canvas->setTextColor(pal.fg, pal.bg);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString(buf, width / 2, HDG_Y);

    // --- Tack arrows ---
    bool tackingPort = (state.tackState != TackState::NONE &&
                        state.tackDir   == TackDir::PORT);
    bool tackingStbd = (state.tackState != TackState::NONE &&
                        state.tackDir   == TackDir::STARBOARD);
    drawTackArrow(canvas, true,  tackingPort, _armed == -1, pal);
    drawTackArrow(canvas, false, tackingStbd, _armed ==  1, pal);

    canvas->pushSprite(0, 0);

    // Buttons draw directly to M5.Display (M5Button bypasses canvas)
    ButtonColors off_c = { pal.bg, pal.fg, pal.fg };
    ButtonColors on_c  = { pal.fg, pal.bg, pal.fg };
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextSize(1);
    for (int i = 0; i < 4; i++) {
        _btn[i].off = off_c;
        _btn[i].on  = on_c;
        _btn[i].draw();
    }

    _lastApState = state.apState;
    _lastApMode  = state.apMode;
    _lastHeading = state.heading;
    _lastCommand = state.command;
    _lastTackSt  = state.tackState;
    _lastTackDir = state.tackDir;
    _lastPalette = state.paletteId;
    _lastArmed   = _armed;
}

int PilotScreen::run(const m5::touch_detail_t &t, State &state) {
    bool changed = (state.apState   != _lastApState  ||
                    state.apMode    != _lastApMode    ||
                    fabsf(state.heading - _lastHeading) >= 0.5f ||
                    fabsf(state.command - _lastCommand) >= 0.5f ||
                    state.tackState != _lastTackSt    ||
                    state.tackDir   != _lastTackDir   ||
                    state.paletteId != _lastPalette   ||
                    _armed          != _lastArmed);
    if (changed) draw(state);

    // --- Swipe ---
    if (t.wasFlicked()) {
        int dx = t.distanceX();
        int dy = t.distanceY();
        if (abs(dx) > abs(dy) && abs(dx) > 50)
            return (dx > 0) ? 2 : 0;   // right → RudderScreen, left → ModeScreen
    }

    if (!t.isPressed() && !t.wasClicked() && !t.wasReleased()) return -1;

    int tx = t.x, ty = t.y;

    // --- STOP button ---
    bool inStop = (tx >= STOP_X && tx < STOP_X + STOP_W &&
                   ty >= STOP_Y && ty < STOP_Y + STOP_H);
    if (inStop && t.wasPressed()) {
        M5.Speaker.tone(1000, 100, 0, false);
        _stopPressed = true;
        draw(state);
    } else if (_stopPressed && t.wasReleased()) {
        M5.Speaker.tone(inStop ? 2000 : 500, 100, 0, false);
        _stopPressed = false;
        if (inStop) {
            pypilot_cmd_disengage();
            return 0;
        }
        draw(state);
    }

    // --- Tack arrow state machine ---
    bool inPortZone = (tx < TACK_W && ty >= TACK_Y0 && ty < BTN_Y);
    bool inStbdZone = (tx >= width - TACK_W && ty >= TACK_Y0 && ty < BTN_Y);

    if (state.tackState != TackState::NONE) {
        // ACTIVE: tacking in progress — any tap cancels
        if (t.wasClicked()) {
            M5.Speaker.tone(500, 100, 0, false);
            pypilot_cmd_cancel_tack();
            _armed = 0;
            _holdActive = false;
            draw(state);
        }
    } else if (_armed != 0) {
        // ARMED: wait for finger from long-press to lift, then accept one tap
        if (_waitRelease) {
            if (!t.isPressed()) _waitRelease = false;
        } else {
            if (t.wasPressed()) {
                M5.Speaker.tone(1000, 100, 0, false);
            }
            if (t.wasReleased()) {
                bool onArmedZone = (_armed == -1 && inPortZone) || (_armed == 1 && inStbdZone);
                if (onArmedZone) {
                    M5.Speaker.tone(2000, 100, 0, false);
                    if (_armed == -1) pypilot_cmd_tack_port();
                    else              pypilot_cmd_tack_stbd();
                } else {
                    M5.Speaker.tone(500, 100, 0, false);
                }
                _armed = 0;
                _holdActive = false;
                draw(state);
            }
        }
    } else {
        // STANDBY: long press on a zone to arm
        if (t.wasPressed() && (inPortZone || inStbdZone)) {
            M5.Speaker.tone(1000, 100, 0, false);  // touch down
            _holdActive = true;
            _holdStart  = millis();
            _holdDir    = inPortZone ? -1 : 1;
        }
        if (_holdActive && !t.isPressed()) {
            _holdActive = false;  // released before hold completed — cancel
        }
        if (_holdActive && (millis() - _holdStart >= HOLD_MS)) {
            M5.Speaker.tone(2000, 100, 0, false);  // armed
            _armed       = _holdDir;
            _holdActive  = false;
            _waitRelease = true;  // finger still down — ignore first release
            draw(state);
        }
    }

    // --- Course-change buttons ---
    const float deltas[] = { -10.0f, -1.0f, 1.0f, 10.0f };
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextSize(1);
    for (int i = 0; i < 4; i++) {
        if (_btn[i].handleTouch(t)) {
            float newCmd = state.command + deltas[i];
            if (state.apMode == ApMode::COMPASS || state.apMode == ApMode::GPS) {
                while (newCmd <   0.0f) newCmd += 360.0f;
                while (newCmd >= 360.0f) newCmd -= 360.0f;
            } else {
                while (newCmd < -180.0f) newCmd += 360.0f;
                while (newCmd >=  180.0f) newCmd -= 360.0f;
            }
            pypilot_cmd_heading(newCmd);
            break;
        }
    }

    return -1;
}
