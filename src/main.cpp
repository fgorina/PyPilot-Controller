#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240

#include <Arduino.h>
#include <M5Dial.h>
#include <Preferences.h>
#include <lvgl.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Define states

#define T_NONE 0

#define T_TOUCH 1
#define T_TOUCH_END 2
#define T_TOUCH_BEGIN 3

#define T_HOLD 5
#define T_HOLD_END 6
#define T_HOLD_BEGIN 7

#define T_FLICK 9
#define T_FLICK_END 10
#define T_FLICK_BEGIN 11

#define T_DRAG 13
#define T_DRAG_END 14
#define T_DRAG_BEGIN 15

static constexpr const char* state_name[16] = {
            "none", "touch", "touch_end", "touch_begin",
            "___",  "hold",  "hold_end",  "hold_begin",
            "___",  "flick", "flick_end", "flick_begin",
            "___",  "drag",  "drag_end",  "drag_begin"};



void setup() {

  auto cfg = M5.config();

  M5Dial.begin(cfg, true, false);
  USBSerial.begin(115200);

    //Set font color
  M5Dial.Display.setTextColor(GREEN);
    //Set font alignment
  M5Dial.Display.setTextDatum(middle_center);
    //Set font
  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
  //M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
    //Set font size
  M5Dial.Display.setTextSize(1);
    //Print text

  drawMainMenu();


}
long oldPosition = -999;
int prev_x = -1;
int prev_y = -1;

static m5::touch_state_t prev_state;

void drawMainMenu(){
    M5Dial.Display.drawFastHLine(0, LV_VER_RES_MAX/2, LV_HOR_RES_MAX, CYAN);
    M5Dial.Display.drawFastVLine(LV_VER_RES_MAX/2, 0 , LV_VER_RES_MAX, CYAN);
    M5Dial.Display.drawString("Compass", M5Dial.Display.width() / 4, M5Dial.Display.height() / 4);
    M5Dial.Display.drawString("GPS", M5Dial.Display.width() / 4 * 3, M5Dial.Display.height() / 4);
    M5Dial.Display.drawString("Wind", M5Dial.Display.width() / 4, M5Dial.Display.height() / 4 * 3);
    M5Dial.Display.drawString("T Wind", M5Dial.Display.width() / 4 * 3, M5Dial.Display.height() / 4 * 3);



}

void doMainMenu(){
  long newPosition = M5Dial.Encoder.read();
    if (newPosition != oldPosition) {
        oldPosition = newPosition;
    
      const char* s = std::to_string(newPosition).c_str();
      M5Dial.Display.clear(BLACK);
      M5Dial.Display.drawFastHLine(0, LV_VER_RES_MAX/2, LV_HOR_RES_MAX, CYAN);
      M5Dial.Display.drawFastVLine(LV_VER_RES_MAX/2, 0 , LV_VER_RES_MAX, CYAN);


      
      M5Dial.Display.drawString(s, M5Dial.Display.width() / 2,
                              M5Dial.Display.height() / 2);
      Serial.println(s);
    }

    // Touch
    auto t = M5Dial.Touch.getDetail();
    
    if (prev_x != t.x || prev_y != t.y || prev_state != t.state) {
       prev_state  = t.state;

       static constexpr const char* state_name[16] = {
            "none", "touch", "touch_end", "touch_begin",
            "___",  "hold",  "hold_end",  "hold_begin",
            "___",  "flick", "flick_end", "flick_begin",
            "___",  "drag",  "drag_end",  "drag_begin"};

        if(t.state == 2){
          USBSerial.println("Finished Touch");
        }
        
        char buff[100] = "";

        sprintf(buff, "x: %d y: %d",  t.x, t.y);

        M5Dial.Display.clear(BLACK);
        M5Dial.Display.drawString(state_name[t.state], M5Dial.Display.width() / 2,
                              (M5Dial.Display.height() / 2)-16);

        M5Dial.Display.drawString(buff, M5Dial.Display.width() / 2,
                              (M5Dial.Display.height() / 2) + 16);

        USBSerial.println("X:" + String(t.x) + " / " + "Y:" + String(t.y));
        prev_x = t.x;
        prev_y = t.y;
    }
}

void setup() {

  auto cfg = M5.config();

  M5Dial.begin(cfg, true, false);
  USBSerial.begin(115200);

    //Set font color
  M5Dial.Display.setTextColor(GREEN);
    //Set font alignment
  M5Dial.Display.setTextDatum(middle_center);
    //Set font
  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
  //M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
    //Set font size
  M5Dial.Display.setTextSize(1);
    //Print text

  drawMainMenu();


}
// Encoder dona 64 / volta
void loop() {
  M5Dial.update();

    // Dial
    if (M5Dial.BtnA.wasPressed()) {

      M5Dial.Display.fillScreen(BLACK);
      M5Dial.Display.drawString("Pressed", M5Dial.Display.width() / 2,
                              M5Dial.Display.height() / 2);
    }
    if (M5Dial.BtnA.wasReleased()) {
      M5Dial.Display.clear(BLACK);
      M5Dial.Display.drawString("Released", M5Dial.Display.width() / 2,
                              M5Dial.Display.height() / 2);

     /* delay(10000);
      M5Dial.Power.timerSleep(5);
      */
    }

    long newPosition = M5Dial.Encoder.read();
    if (newPosition != oldPosition) {
        oldPosition = newPosition;
    
      const char* s = std::to_string(newPosition).c_str();
      M5Dial.Display.clear(BLACK);
      M5Dial.Display.drawFastHLine(0, LV_VER_RES_MAX/2, LV_HOR_RES_MAX, CYAN);
      M5Dial.Display.drawFastVLine(LV_VER_RES_MAX/2, 0 , LV_VER_RES_MAX, CYAN);


      
      M5Dial.Display.drawString(s, M5Dial.Display.width() / 2,
                              M5Dial.Display.height() / 2);
      Serial.println(s);
    }

    // Touch
    auto t = M5Dial.Touch.getDetail();
    
    if (prev_x != t.x || prev_y != t.y || prev_state != t.state) {
       prev_state  = t.state;

       static constexpr const char* state_name[16] = {
            "none", "touch", "touch_end", "touch_begin",
            "___",  "hold",  "hold_end",  "hold_begin",
            "___",  "flick", "flick_end", "flick_begin",
            "___",  "drag",  "drag_end",  "drag_begin"};

        if(t.state == 2){
          USBSerial.println("Finished Touch");
        }
        
        char buff[100] = "";

        sprintf(buff, "x: %d y: %d",  t.x, t.y);

        M5Dial.Display.clear(BLACK);
        M5Dial.Display.drawString(state_name[t.state], M5Dial.Display.width() / 2,
                              (M5Dial.Display.height() / 2)-16);

        M5Dial.Display.drawString(buff, M5Dial.Display.width() / 2,
                              (M5Dial.Display.height() / 2) + 16);

        USBSerial.println("X:" + String(t.x) + " / " + "Y:" + String(t.y));
        prev_x = t.x;
        prev_y = t.y;
    }
}