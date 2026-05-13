#include "ModeScreen.h"
#include "net_pypilot.h"

ModeScreen::ModeScreen(int w, int h) : Screen(w, h, "MODE") {}

// --- Helpers -----------------------------------------------------------------

void ModeScreen::drawQuadrant(M5Canvas *cv, int cx, int cy,
                               const char *label, bool active,
                               const ColorPalette &pal) {
    uint16_t bg = active ? pal.hilite   : pal.bg;
    uint16_t fg = active ? pal.hiliteFg : pal.fg;

    if (active)
        cv->fillRect(cx - 77, cy - 57, 154, 114, bg);

    cv->setTextColor(fg, bg);
    cv->setTextDatum(MC_DATUM);
    cv->setFont(&fonts::FreeSansBold18pt7b);
    cv->setTextSize(1);
    cv->drawString(label, cx, cy);
}

void ModeScreen::drawCentreCircle(M5Canvas *cv, const ColorPalette &pal) {
    cv->fillCircle(CX, CY, CR, pal.bg);
    cv->drawCircle(CX, CY, CR, pal.fg);
    cv->setTextColor(pal.fg, pal.bg);
    cv->setTextDatum(MC_DATUM);
    cv->setFont(&fonts::FreeSansBold9pt7b);
    cv->setTextSize(1);
    cv->drawString("Rudder", CX, CY);
}

// --- Screen interface --------------------------------------------------------

void ModeScreen::enter(State &state) {
    _pressedZone = 0;
    _lastApState = ApState::ENGAGED;   // differs → triggers immediate draw
    _lastApMode  = ApMode::NA;
    _lastPalette = static_cast<PaletteId>(-1);
    Screen::enter(state);
}

void ModeScreen::draw(State &state) {
    const ColorPalette &pal = state.pal();

    canvas->clear(pal.bg);

    // Dividing cross
    canvas->drawFastHLine(0,  CY, width,  pal.dim);
    canvas->drawFastVLine(CX, 0,  height, pal.dim);

    // Quadrant labels — highlight the active mode when engaged
    bool engaged = (state.apState == ApState::ENGAGED);

    drawQuadrant(canvas, CX / 2,      CY / 2 - 5,  "Compass",
                 engaged && state.apMode == ApMode::COMPASS,   pal);
    drawQuadrant(canvas, CX + CX / 2, CY / 2 - 5,  "GPS",
                 engaged && state.apMode == ApMode::GPS,        pal);
    drawQuadrant(canvas, CX / 2,      CY + CY / 2 + 5, "T Wind",
                 engaged && state.apMode == ApMode::TRUE_WIND,  pal);
    drawQuadrant(canvas, CX + CX / 2, CY + CY / 2 + 5, "Wind",
                 engaged && state.apMode == ApMode::WIND,       pal);

    drawCentreCircle(canvas, pal);

    canvas->pushSprite(0, 0);

    _lastApState = state.apState;
    _lastApMode  = state.apMode;
    _lastPalette = state.paletteId;
}

int ModeScreen::run(const m5::touch_detail_t &t, State &state) {
    // Redraw on state or palette change
    bool changed = (state.apState  != _lastApState ||
                    state.apMode   != _lastApMode  ||
                    state.paletteId != _lastPalette);
    if (changed) draw(state);

    // Zone helper: 1=circle, 2=compass, 3=GPS, 4=true_wind, 5=wind
    auto zoneAt = [](int tx, int ty) -> int {
        int ddx = tx - CX, ddy = ty - CY;
        if (ddx*ddx + ddy*ddy <= CR*CR) return 1;
        if (ty < CY) return (tx < CX) ? 2 : 3;
        return (tx < CX) ? 4 : 5;
    };

    // --- Swipe (resets any in-progress press) ---
    if (t.wasFlicked()) {
        _pressedZone = 0;
        int dx = t.distanceX();
        int dy = t.distanceY();
        if (abs(dx) > abs(dy) && abs(dx) > 50)
            return (dx > 0) ? 1 : 2;
    }

    // --- Press: arm the zone ---
    if (t.wasPressed()) {
        _pressedZone = zoneAt(t.x, t.y);
        if (_pressedZone != 0)
            M5.Speaker.tone(1000, 100, 0, false);
    }

    // --- Release: confirm or cancel ---
    if (t.wasReleased() && _pressedZone != 0) {
        int zone = _pressedZone;
        _pressedZone = 0;
        if (zoneAt(t.x, t.y) == zone) {
            M5.Speaker.tone(2000, 100, 0, false);
            if (zone == 1) {
                state.rudderMode   = true;
                state.updateRudder = false;
                state.rudderTarget = state.servoPosition;
                return 2;
            }
            static const char * const modeStrs[] = { nullptr, nullptr, "compass", "gps", "true wind", "wind" };
            static const ApMode modes[]           = { ApMode::NA, ApMode::NA, ApMode::COMPASS, ApMode::GPS, ApMode::TRUE_WIND, ApMode::WIND };
            pypilot_cmd_mode(modeStrs[zone]);
            pypilot_cmd_engage();
            if (modes[zone] == ApMode::COMPASS || modes[zone] == ApMode::GPS)
                pypilot_cmd_heading(state.heading);
            return 1;
        } else {
            M5.Speaker.tone(500, 100, 0, false);
        }
    }

    return -1;
}
