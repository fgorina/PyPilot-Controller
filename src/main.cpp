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
#include <ReactESP.h> // https://github.com/mairas/ReactESP

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

static constexpr const char *state_name[16] = {
    "none", "touch", "touch_end", "touch_begin",
    "___", "hold", "hold_end", "hold_begin",
    "___", "flick", "flick_end", "flick_begin",
    "___", "drag", "drag_end", "drag_begin"};

Preferences preferences;

static bool redraw = true;

static String wifi_ssid;     // Store the name of the wireless network.
static String wifi_password; // Store the password of the wireless network.

typedef struct _NetClient
{
  WiFiClient c = WiFiClient();
  unsigned long lastActivity = 0U;
} NetClient;

NetClient pypClient;

#include "data_model.h"

static ship_data_t shipDataModel;

#include "keepalive.h"

using namespace reactesp;
ReactESP app;

#include "pypilot_parse.h"
#include "net_pypilot.h"

long oldPosition = -999;
int prev_x = -1;
int prev_y = -1;

static unsigned long last_touched;
static m5::touch_state_t prev_state;

static constexpr const char *mode_names[6] = {
    "Standby", "Compass", "GPS", "Wind",
    "True Wind", "Rudder"};

String modeString(ap_mode_e mode)
{

  switch (mode)
  {
  case ap_mode_e::HEADING_MAG:
    return "compass";
    break;

  case ap_mode_e::COG_TRUE:
    return "gps";
    break;
  case ap_mode_e::APP_WIND:
    return "wind";
    break;

  case ap_mode_e::TRUE_WIND:
    return "true wind";
    break;

  default:
    return "not avilable";
  }
}

// WiFI
boolean checkConnection()
{                // Check wifi connection.
  int count = 0; // count.
  while (count < 30)
  { // If you fail to connect to wifi within 30*350ms (10.5s), return false; otherwise return true.
    if (WiFi.status() == WL_CONNECTED)
    {
      return true;
    }
    delay(350);
    count++;
  }
  return false;
}

boolean startWiFi()
{ // Check whether there is wifi configuration information storage, if there is return 1, if no return 0.
  wifi_ssid = "elrond";
  wifi_password = "ailataN1991";
  // WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  if (checkConnection())
  {
    USBSerial.print("Connected to ");
    USBSerial.println(wifi_ssid);
    String pypilot_tcp_host = "192.168.1.3";
    int pypilot_tcp_port = 23322;

    pypilot_begin(pypClient, pypilot_tcp_host.c_str(), pypilot_tcp_port); // Connect to the PyPilot TCP server

    return true;
  }
  return false;
}

void drawStandbycreen()
{

  M5Dial.Display.setTextSize(0.5);
  M5Dial.Display.clear(BLACK);
  M5Dial.Display.setTextColor(GREEN);

  M5Dial.Display.drawFastHLine(0, LV_VER_RES_MAX / 2, LV_HOR_RES_MAX, CYAN);
  M5Dial.Display.drawFastVLine(LV_HOR_RES_MAX / 2, 0, LV_VER_RES_MAX, CYAN);

  M5Dial.Display.drawString("Compass", M5Dial.Display.width() / 4, M5Dial.Display.height() / 4);
  M5Dial.Display.drawString("GPS", M5Dial.Display.width() / 4 * 3, M5Dial.Display.height() / 4);
  M5Dial.Display.drawString("Wind", M5Dial.Display.width() / 4, M5Dial.Display.height() / 4 * 3);
  M5Dial.Display.drawString("T Wind", M5Dial.Display.width() / 4 * 3, M5Dial.Display.height() / 4 * 3);

  M5Dial.Display.fillCircle(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, BLACK);
  M5Dial.Display.drawCircle(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, CYAN);

  M5Dial.Display.drawString("Rudder", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
}

void drawNavigationScreen()
{
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.clear(BLACK);

  M5Dial.Display.drawFloat(round(shipDataModel.steering.autopilot.command.deg), 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 4);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawFloat(round(shipDataModel.steering.autopilot.heading.deg), 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawString(modeString(shipDataModel.steering.autopilot.ap_mode.mode), M5Dial.Display.width() / 2, M5Dial.Display.height() / 4 * 3);

  M5Dial.Display.fillTriangle(0, LV_VER_RES_MAX / 2, 30, LV_VER_RES_MAX / 2 - 17, 30, LV_VER_RES_MAX / 2 + 17, GREEN);
  M5Dial.Display.fillTriangle(LV_HOR_RES_MAX - 30, LV_VER_RES_MAX / 2 - 17, LV_HOR_RES_MAX - 30, LV_VER_RES_MAX / 2 + 17, LV_HOR_RES_MAX, LV_VER_RES_MAX / 2, GREEN);
}

void drawRudderScreen()
{

  M5Dial.Display.setTextSize(1);
  M5Dial.Display.clear(BLACK);

  M5Dial.Display.drawFloat(round(shipDataModel.steering.rudder_angle.deg), 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 4);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawFloat(round(shipDataModel.steering.autopilot.heading.deg), 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawString(modeString(shipDataModel.steering.autopilot.ap_mode.mode), M5Dial.Display.width() / 2, M5Dial.Display.height() / 4 * 3);
}

void drawScreen()
{
  M5Dial.Display.beginTransaction();

  if (shipDataModel.steering.autopilot.ap_state.st == ap_state_e::STANDBY)
  {
    drawStandbycreen();
  }
  else
  {
    drawNavigationScreen();
  }

  M5Dial.Display.endTransaction();
  redraw = false;
}

bool doRudder()
{

  if (M5Dial.BtnA.wasReleased())
  {
    shipDataModel.steering.autopilot.ap_state.st = ap_state_e::STANDBY; // Send  data to pypilot

    return true;
  }

  long newPosition = M5Dial.Encoder.read();
  if (newPosition != oldPosition)
  {
    last_touched = millis();
    long delta = newPosition - oldPosition;
    shipDataModel.steering.rudder_angle.deg += (delta * 1.0); // Send  data to pypilot
    oldPosition = newPosition;
    return true;
  }
  return false;
}

bool doNavigation()
{
  if (M5Dial.BtnA.wasReleased())
  {
    // shipDataModel.steering.autopilot.ap_state.st = ap_state_e::STANDBY; // Send  data to pypilot
    last_touched = millis();
    pypilot_send_disengage(pypClient.c);
    return true;
  }

  long newPosition = M5Dial.Encoder.read();
  if (newPosition != oldPosition)
  {

    long delta = newPosition - oldPosition;
    float newCommand = shipDataModel.steering.autopilot.command.deg + (delta * 1.0);

    pypilot_send_command(pypClient.c, newCommand);

    shipDataModel.steering.autopilot.command.deg = newCommand;
    oldPosition = newPosition;
    last_touched = millis();
    return true;
  }

  auto t = M5Dial.Touch.getDetail();
  if (t.state == T_HOLD_END)
  {
    if (t.x < 80 && abs(t.y - LV_VER_RES_MAX / 2) < 20)
    {
      USBSerial.println("Tacking Port");
    }
    else if (t.x > (LV_HOR_RES_MAX - 80) && abs(t.y - LV_VER_RES_MAX / 2) < 20)
    {
      USBSerial.println("Tacking Starboard");
    }
  }

  if (t.state != T_NONE)
  {
    last_touched = millis();
  }

  return false;
}

bool doStandby()
{
  long newPosition = M5Dial.Encoder.read();
  if (newPosition != oldPosition)
  {
    last_touched = millis();
  }
  // Touch
  auto t = M5Dial.Touch.getDetail();
  oldPosition = M5Dial.Encoder.read();
  if (t.state == T_TOUCH_END)
  {

    // Check if in the center.

    if (abs(t.x - LV_HOR_RES_MAX / 2) < 20 && abs(t.y - LV_VER_RES_MAX / 2) < 20)
    {
      shipDataModel.steering.autopilot.ap_state.st = ap_state_e::STANDBY; // Send  data to pypilot
    }
    else if (t.y < LV_VER_RES_MAX / 2)
    {
      if (t.x < LV_VER_RES_MAX / 2)
      {
        USBSerial.println("Selected Compass");
        oldPosition = M5Dial.Encoder.read();
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_COMPASS);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::HEADING_MAG;
      }
      else
      {
        USBSerial.println("Selected GPS");
        oldPosition = M5Dial.Encoder.read();
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_GPS);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::COG_TRUE;
      }
    }
    else
    {
      if (t.x < LV_VER_RES_MAX / 2)
      {
        USBSerial.println("Selected Wind");
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_WIND);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::APP_WIND;
      }
      else
      {
        USBSerial.println("Selected True Wind");
        oldPosition = M5Dial.Encoder.read();
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_WIND_TRUE);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::TRUE_WIND;
      }
    }
    return true;
  }
  if (t.state != T_NONE)
  {
    last_touched = millis();
  }
  return false;
}

bool loopTask()
{
  if (shipDataModel.steering.autopilot.ap_state.st == ap_state_e::STANDBY)
  {
    return doStandby();
  }
  else
  {
    return doNavigation();
  }
}

void setup()
{

  auto cfg = M5.config();

  M5Dial.begin(cfg, true, false);
  last_touched = millis();

  USBSerial.begin(115200);
  M5Dial.Display.setTextColor(GREEN);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
  M5Dial.Display.setTextSize(0.5);

  last_touched = 0;
}
// Encoder dona 64 / volta

#define GO_SLEEP_TIMEOUT 1800000ul
void loop()
{

  M5Dial.update();
  app.tick();

  if (last_touched > 0 && millis() - last_touched > GO_SLEEP_TIMEOUT)
  {
    // disconnect_clients();
    // save_page(page);
    // deep_sleep_with_touch_wakeup();

    M5Dial.Display.powerSaveOn();
  }
  else
  {
    if (!checkConnection())
    {
      startWiFi();
    }
  }
  redraw = redraw || loopTask();

  if (redraw)
  {
    drawScreen();
  }
}