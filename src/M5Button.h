/* M5Button.h
   Button and ButtonColors compatibility layer for M5Unified.
   Replaces the Button class from M5Tough which does not exist in M5Unified.
*/

#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <cstdint>
#include <cstring>

struct ButtonColors {
  uint16_t bg;
  uint16_t text;
  uint16_t outline;
};

class Button {
public:
  int16_t x, y, w, h;
  ButtonColors off, on;
  char _label[32];
  uint8_t datum;
  bool pressed = false;

  Button(int16_t x, int16_t y, int16_t w, int16_t h, bool outline,
         const char *label, ButtonColors off_clrs, ButtonColors on_clrs,
         uint8_t datum = MC_DATUM)
      : x(x), y(y), w(w), h(h), off(off_clrs), on(on_clrs), datum(datum) {
    strncpy(_label, label, sizeof(_label) - 1);
    _label[sizeof(_label) - 1] = '\0';
  }

  void draw() {
    ButtonColors &clr = pressed ? on : off;
    M5.Display.fillRoundRect(x, y, w, h, 20, clr.bg);
    M5.Display.drawRoundRect(x, y, w, h, 20, clr.outline);
    M5.Display.setTextColor(clr.text, clr.bg);
    M5.Display.setTextDatum(datum);
    M5.Display.drawString(_label, x + w / 2, y + h / 2);
  }

  void hide(uint16_t color) { M5.Display.fillRect(x, y, w, h, color); }

  void setLabel(const char *label) {
    strncpy(_label, label, sizeof(_label) - 1);
    _label[sizeof(_label) - 1] = '\0';
  }

  // No-op: M5Tough compatibility stub
  void delHandlers() {}

  bool contains(int px, int py) {
    return px >= x && px < x + w && py >= y && py < y + h;
  }

  bool wasReleased() {
    auto count = M5.Touch.getCount();
    for (uint_fast8_t i = 0; i < count; i++) {
      auto t = M5.Touch.getDetail(i);
      if (t.wasReleased() && contains(t.x, t.y)) {
        return true;
      }
    }
    return false;
  }

  bool isPressed() {
    auto count = M5.Touch.getCount();
    for (uint_fast8_t i = 0; i < count; i++) {
      auto t = M5.Touch.getDetail(i);
      if (t.isPressed() && contains(t.x, t.y)) {
        return true;
      }
    }
    return false;
  }

  bool handleTouch(const m5::touch_detail_t &t) {

    if (contains(t.x, t.y)) {

      if (pressed) {
        if (t.wasReleased()) {
          M5.Speaker.tone(2000, 100, 0, false);

          pressed = false;
          draw();
          return true;
          // Do Someting
        }
      } else {
        if (t.wasPressed()) {
          M5.Speaker.tone(1000, 100, 0, false);

          pressed = true;
          draw();
          return false;
        }
      }
    } else {
      if (pressed) {
        M5.Speaker.tone(500, 100, 0, false);

        pressed = false;
        draw();
        return false;
      } else {

        return false;
      }
    }
    return false;
  }
};
