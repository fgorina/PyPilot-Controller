#include "Theme.h"

// --- Day Green: green on black — classic sailing chart look ------------------
// fg  = full GREEN,  hilite = GREEN block + BLACK text
// warn = ORANGE (intent), danger = RED (active / stop)
const ColorPalette PALETTE_DAY_GREEN = {
    /* bg       */ TFT_BLACK,
    /* fg       */ TFT_GREEN,
    /* hilite   */ TFT_GREEN,
    /* hiliteFg */ TFT_BLACK,
    /* warn     */ TFT_ORANGE,
    /* danger   */ TFT_RED,
    /* dim      */ TFT_NAVY,            // dark blue — good contrast for button fills
    /* name     */ "Day Green"
};

// --- Day White: black on white — maximum visibility in direct sunlight -------
// fg  = BLACK,  hilite = BLACK block + WHITE text
// warn = ORANGE, danger = RED (still visible on white bg)
const ColorPalette PALETTE_DAY_WHITE = {
    /* bg       */ TFT_WHITE,
    /* fg       */ TFT_BLACK,
    /* hilite   */ TFT_BLACK,
    /* hiliteFg */ TFT_WHITE,
    /* warn     */ TFT_ORANGE,
    /* danger   */ TFT_RED,
    /* dim      */ TFT_DARKGREY,
    /* name     */ "Day White"
};

// --- Night: red on black — preserve night vision -----------------------------
// fg  = RED,  hilite = dark red block + bright red text
// warn = DARKGREEN (not blinding), danger = DARKGREEN
const ColorPalette PALETTE_NIGHT = {
    /* bg       */ TFT_BLACK,
    /* fg       */ TFT_RED,
    /* hilite   */ (uint16_t)0xA000,   // lgfx::color565(160,0,0) — medium-dark red
    /* hiliteFg */ TFT_RED,
    /* warn     */ (uint16_t)0x0300,   // very dim green — gentle caution
    /* danger   */ TFT_DARKGREEN,
    /* dim      */ (uint16_t)0x4000,   // very dark red
    /* name     */ "Night"
};

// --- Lookup ------------------------------------------------------------------

const ColorPalette &paletteById(PaletteId id) {
    switch (id) {
        case PaletteId::DAY_WHITE: return PALETTE_DAY_WHITE;
        case PaletteId::NIGHT:     return PALETTE_NIGHT;
        default:                   return PALETTE_DAY_GREEN;
    }
}
