#pragma once
#include <M5Unified.h>

// ColorPalette holds every colour a screen needs.
// All screens access it via state.pal() — no hard-coded colour constants in screens.

struct ColorPalette {
    uint16_t    bg;        // screen background
    uint16_t    fg;        // primary text / inactive elements
    uint16_t    hilite;    // selected / active element fill
    uint16_t    hiliteFg;  // text drawn on top of hilite fill
    uint16_t    warn;      // caution indicator (tack intent, orange)
    uint16_t    danger;    // danger / stop / active-tack
    uint16_t    dim;       // de-emphasised text (labels, disconnected)
    const char *name;
};

enum class PaletteId { DAY_GREEN = 0, DAY_WHITE = 1, NIGHT = 2 };

static constexpr int PALETTE_COUNT = 3;

// Predefined palettes — defined in Theme.cpp
extern const ColorPalette PALETTE_DAY_GREEN;
extern const ColorPalette PALETTE_DAY_WHITE;
extern const ColorPalette PALETTE_NIGHT;

// Returns the palette for the given id (reference to a global const object).
const ColorPalette &paletteById(PaletteId id);
