#include "Screen.h"

M5Canvas* Screen::canvas = nullptr;

Screen::Screen(int width, int height, const char *title) {
  this->width = width;
  this->height = height;
  this->title = title;
}

void Screen::enter(State &state) {
  if (!canvas) {
    canvas = new M5Canvas(&M5.Display);
  }
  canvas->createSprite(width, height);
  draw(state);
}

void Screen::exit(State &state) {
  canvas->deleteSprite();
}

void Screen::draw(State &state) {}
int Screen::run(const m5::touch_detail_t &t, State &state) {
  Serial.println("Screen::run");
  return -1;
}
