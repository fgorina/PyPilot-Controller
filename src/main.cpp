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

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Version

static const char *version = "v 0.0.4";

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

static bool redraw = true;

static String wifi_ssid = "mynetwork";      // Store the name of the wireless network.
static String wifi_password = "mypassword"; // Store the password of the wireless network.
static IPAddress pypilot_tcp_host = IPAddress(192, 168, 1, 3);
static int pypilot_tcp_port = 23322;

static int color = GREEN;
static int selectedColor = DARKGREEN;
static int emphasisColor = RED;

Preferences preferences;
void writePreferences();
void readPreferences();
void lookupPypilot();

void setStateCharacteristicRudder(float rudder_angle);
void setStateCharacteristicHeading(float heading);
void setStateCharacteristicCommand(float command);
void setStateCharacteristicEnabled(int state);
void setStateCharacteristicTackState(int tackState);
void setStateCharacteristicTackDirection(int tackDirection);
void setStateCharacteristicMode(int mode);

#include "net_mdns.h"

typedef struct _NetClient
{
  WiFiClient c = WiFiClient();
  unsigned long lastActivity = 0U;
} NetClient;

NetClient pypClient = NetClient();

#include "data_model.h"

static ship_data_t shipDataModel;

#include "keepalive.h"

using namespace reactesp;
ReactESP app;

boolean startWiFi();

#define MAX_RUDDER 30
static bool rudderMode = false;
static bool updateRudder = false;
void sendRudderCommand();

static bool detailMode = false; // Detail Mode is for special info for Autopilot

#include "pypilot_parse.h"
#include "net_pypilot.h"

static constexpr const char *modes[4] = {"compass", "gps", "wind", "true wind"};
static int edit_mode = 0;
static float edit_heading = 0.0;
static float edit_position = 0.0;       // Servo Position
static float last_drawn_position = 0.0; // Lasr Servo position drawn in screen

static int selectedOption = 0;

#include "ble_server.h"

long oldPosition = -999;
int prev_x = -1;
int prev_y = -1;

static unsigned long last_touched;
static m5::touch_state_t prev_state;

#define DISPLAY_ACTIVE 0
#define DISPLAY_SLEEPING 1
#define DISPLAY_WAKING 2

static int displaySaver = DISPLAY_ACTIVE;

#define ABOUT_TO_TACK_PORT -1
#define ABOUT_TO_TACK_STARBOARD 1
#define ABOUT_TO_TACK_NONE 0

static int aboutToTackState = ABOUT_TO_TACK_NONE;
static bool selectingMode = false;

#include "menu_encoder.h"

// Funcions per connectar-se per BLE
void setStateCharacteristicRudder(float rudder_angle)
{
  sprintf(buffer, "R%.0f", rudder_angle);

  stateCharacteristic->setValue((uint8_t *)buffer, strlen(buffer));
  stateCharacteristic->notify();
}
void setStateCharacteristicHeading(float heading)
{
  sprintf(buffer, "H%.0f", heading);

  stateCharacteristic->setValue((uint8_t *)buffer, strlen(buffer));
  stateCharacteristic->notify();
}

void setStateCharacteristicCommand(float command)
{
  sprintf(buffer, "C%.0f", command);

  stateCharacteristic->setValue((uint8_t *)buffer, strlen(buffer));
  stateCharacteristic->notify();
}

void setStateCharacteristicEnabled(int state)
{

  if (state == 1)
  {
    sprintf(buffer, "E");
  }
  else
  {
    sprintf(buffer, "D");
  }
  stateCharacteristic->setValue((uint8_t *)buffer, strlen(buffer));
  stateCharacteristic->notify();
}

void setStateCharacteristicTackState(int tackState)
{
  sprintf(buffer, "T%d", tackState);

  stateCharacteristic->setValue((uint8_t *)buffer, strlen(buffer));
  stateCharacteristic->notify();
}
void setStateCharacteristicTackDirection(int tackDirection)
{
  sprintf(buffer, "U%d", tackDirection);

  stateCharacteristic->setValue((uint8_t *)buffer, strlen(buffer));
  stateCharacteristic->notify();
}

void setStateCharacteristicMode(int mode)
{

  sprintf(buffer, "M%i", mode);

  stateCharacteristic->setValue((uint8_t *)buffer, strlen(buffer));
  stateCharacteristic->notify();
  USBSerial.print("Sending ");
  USBSerial.println(buffer);
}
// Fi BLE

int modeIndex(ap_mode_e mode)
{
  switch (mode)
  {
  case ap_mode_e::HEADING_MAG:
    return 0;
    break;

  case ap_mode_e::COG_TRUE:
    return 1;
    break;
  case ap_mode_e::APP_WIND:
    return 2;
    break;

  case ap_mode_e::TRUE_WIND:
    return 3;
    break;

  default:
    return -1;
  }
}

const char *modeCommand(int idx)
{
  switch (idx)
  {
  case 0:
    return AP_MODE_COMPASS;
    break;

  case 1:
    return AP_MODE_GPS;
    break;
  case 2:
    return AP_MODE_WIND;
    break;

  case 3:
    return AP_MODE_WIND_TRUE;
    break;

  default:
    return AP_MODE_COMPASS;
    break;
  }
}
const char *modeString(ap_mode_e mode)
{
  int idx = modeIndex(mode);

  if (idx < 0 || idx > 3)
  {
    return "error";
  }
  else
  {
    return modes[idx];
  }
}
// COLORS

void setDayColor()
{
  color = GREEN;
  selectedColor = DARKGREEN;
  emphasisColor = RED;
}

void setNightColor()
{
  color = RED;
  selectedColor = lgfx::v1::color565(128, 0, 0);
  emphasisColor = GREEN;
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

  M5Dial.Display.clear(BLACK);
  M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
  M5Dial.Display.drawString("Connecting to ", LV_HOR_RES_MAX / 2, LV_HOR_RES_MAX / 2 - 16);
  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
  M5Dial.Display.drawString(wifi_ssid, LV_HOR_RES_MAX / 2, LV_HOR_RES_MAX / 2 + 16);

  // WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  USBSerial.print(wifi_ssid);
  USBSerial.print(" ");
  USBSerial.println(wifi_password);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  if (checkConnection())
  {
    USBSerial.print("Connected to ");
    USBSerial.print(wifi_ssid);
    USBSerial.print(" IP ");
    USBSerial.println(WiFi.localIP());

    lookupPypilot();

    M5Dial.Display.clear(BLACK);
    M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
    M5Dial.Display.drawString("Connecting to ", LV_HOR_RES_MAX / 2, LV_HOR_RES_MAX / 2 - 16);

    M5Dial.Display.drawString("PyPilot", LV_HOR_RES_MAX / 2, LV_HOR_RES_MAX / 2 + 16);
    M5Dial.Display.drawString(pypilot_tcp_host.toString(), LV_HOR_RES_MAX / 2, LV_HOR_RES_MAX / 2 + 44);
    delay(1000);

    pypilot_begin(pypClient, pypilot_tcp_host, pypilot_tcp_port); // Connect to the PyPilot TCP server
    M5Dial.Display.setFont(&fonts::Orbitron_Light_32);

    return true;
  }
  return false;
}

void drawStandbyScreen()
{
  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
  M5Dial.Display.setTextSize(0.5);
  M5Dial.Display.clear(BLACK);
  M5Dial.Display.setTextColor(color);

  if (selectedOption == 0)
  { // Compass
    M5Dial.Display.fillArc(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, LV_HOR_RES_MAX / 2, 180, 270, selectedColor);
  }
  else if (selectedOption == 1)
  { // GPS
    M5Dial.Display.fillArc(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, LV_HOR_RES_MAX / 2, 270, 0, selectedColor);
  }
  else if (selectedOption == 2)
  { // Wind
    M5Dial.Display.fillArc(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, LV_HOR_RES_MAX / 2, 0, 90, selectedColor);
  }
  else if (selectedOption == 3)
  { // True Wind
    M5Dial.Display.fillArc(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, LV_HOR_RES_MAX / 2, 90, 180, selectedColor);
  }

  M5Dial.Display.drawFastHLine(0, LV_VER_RES_MAX / 2, LV_HOR_RES_MAX, color);
  M5Dial.Display.drawFastVLine(LV_HOR_RES_MAX / 2, 0, LV_VER_RES_MAX, color);

  M5Dial.Display.drawString("Compass", M5Dial.Display.width() / 4, M5Dial.Display.height() / 4);
  M5Dial.Display.drawString("GPS", M5Dial.Display.width() / 4 * 3, M5Dial.Display.height() / 4);
  M5Dial.Display.drawString("T Wind", M5Dial.Display.width() / 4, M5Dial.Display.height() / 4 * 3);
  M5Dial.Display.drawString("Wind", M5Dial.Display.width() / 4 * 3, M5Dial.Display.height() / 4 * 3);

  M5Dial.Display.fillCircle(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, selectedOption == 4 ? selectedColor : BLACK);
  M5Dial.Display.drawCircle(LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2, 50, color);

  M5Dial.Display.drawString("Rudder", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
}

void drawNavigationScreen()
{
  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.clear(BLACK);

  M5Dial.Display.drawFloat(round(edit_heading), 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 4);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawFloat(round(shipDataModel.steering.autopilot.heading.deg), 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.setTextColor(selectingMode ? emphasisColor : color);
  M5Dial.Display.drawString(modes[edit_mode], M5Dial.Display.width() / 2, M5Dial.Display.height() / 4 * 3);
  M5Dial.Display.setTextColor(color);

  M5Dial.Display.fillTriangle(0, LV_VER_RES_MAX / 2, 30, LV_VER_RES_MAX / 2 - 17, 30, LV_VER_RES_MAX / 2 + 17,
                              aboutToTackState == ABOUT_TO_TACK_PORT ? ORANGE : ((shipDataModel.steering.autopilot.tack.st != ap_tack_state_e::TACK_NONE && shipDataModel.steering.autopilot.tack.direction == ap_tack_direction_e::TACKING_TO_PORT) ? emphasisColor : color));

  M5Dial.Display.fillTriangle(LV_HOR_RES_MAX - 30, LV_VER_RES_MAX / 2 - 17, LV_HOR_RES_MAX - 30, LV_VER_RES_MAX / 2 + 17, LV_HOR_RES_MAX, LV_VER_RES_MAX / 2,
                              aboutToTackState == ABOUT_TO_TACK_STARBOARD ? ORANGE : ((shipDataModel.steering.autopilot.tack.st != ap_tack_state_e::TACK_NONE && shipDataModel.steering.autopilot.tack.direction == ap_tack_direction_e::TACKING_TO_STARBOARD) ? emphasisColor : color));
}

void drawRudderScreen()
{

  M5Dial.Display.setTextSize(1);
  M5Dial.Display.clear(BLACK);

  M5Dial.Display.drawFloat(edit_position, 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 4);
  M5Dial.Display.setTextSize(2);
  M5Dial.Display.drawFloat(round(shipDataModel.steering.autopilot.heading.deg), 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawFloat(shipDataModel.steering.rudder_angle.deg, 0, M5Dial.Display.width() / 2, M5Dial.Display.height() / 4 * 3);
  last_drawn_position = shipDataModel.steering.rudder_angle.deg;
}

void drawDetailScreen()
{
  char buffer[128];

  M5Dial.Display.setTextSize(1);
  M5Dial.Display.clear(BLACK);

  sprintf(buffer, "V: %.1f", shipDataModel.steering.autopilot.ap_servo.voltage.volt);
  M5Dial.Display.drawString(buffer, M5Dial.Display.width() / 2, M5Dial.Display.height() / 4);

  sprintf(buffer, "Ah: %.1f", shipDataModel.steering.autopilot.ap_servo.amp_hr.amp_hr);
  M5Dial.Display.drawString(buffer, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);

  sprintf(buffer, "T: %.1f", shipDataModel.steering.autopilot.ap_servo.controller_temp.deg_C);
  M5Dial.Display.drawString(buffer, M5Dial.Display.width() / 2, M5Dial.Display.height() / 4 * 3);
}

void drawReconnectingScreen()
{
  M5Dial.Display.clear(BLACK);
  M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
  M5Dial.Display.setTextSize(1);

  M5Dial.Display.drawString("Reconnecting to:", M5Dial.Display.width() / 2, M5Dial.Display.height() / 2) - 32;
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawString(pypilot_tcp_host.toString(), M5Dial.Display.width() / 2, M5Dial.Display.height() / 2 + 32);
  M5Dial.Display.setTextSize(1);

  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
}

void drawScreen()
{
  M5Dial.Display.beginTransaction();

  if (!pypClient.c.connected())
  {
    drawReconnectingScreen();
  }
  else if (shipDataModel.steering.autopilot.ap_state.st == ap_state_e::STANDBY)
  {
    if (rudderMode)
    {
      drawRudderScreen();
    }
    else if (detailMode)
    {
      drawDetailScreen();
    }
    else
    {
      drawStandbyScreen();
    }
  }
  else
  {
    drawNavigationScreen();
  }

  M5Dial.Display.endTransaction();
  redraw = false;
}

void doUpdateRudder(long count)
{
  edit_position = shipDataModel.steering.rudder_angle.deg - (count * 1.0);

  if (edit_position < -MAX_RUDDER)
  {
    edit_position = -MAX_RUDDER;
  }

  if (edit_position > MAX_RUDDER)
  {
    edit_position = MAX_RUDDER;
  }
}

void sendRudderCommand()
{

  float delta = edit_position - shipDataModel.steering.rudder_angle.deg;

  // USBSerial.print("Delta: "); USBSerial.println(String(delta, 2));

  if (fabs(delta) < 0.5)
  {
    USBSerial.print("Servo position at ");
    USBSerial.println(String(shipDataModel.steering.autopilot.ap_servo.position.deg, 2));
    pypilot_send_rudder_command(pypClient.c, 0.0);
    updateRudder = false;
  }
  else
  {

    float sign = 1.0;
    if (delta > 0.0)
    {
      sign = 1.0;
    }
    else
    {
      sign = -1.0;
    }
    float command = fabs(delta) / 60.0;

    command = min(max(command, float(0.001)), float(1.0));
    command = command * sign;

    /* USBSerial.print(String(command, 2));
    USBSerial.print(";");
    USBSerial.println(String(shipDataModel.steering.autopilot.ap_servo.position.deg, 2));
    */

    pypilot_send_rudder_command(pypClient.c, command);
  }
}

void commitRudder(long count)
{

  // pypilot_send_rudder_position(pypClient.c, edit_position);
  // return;
  updateRudder = true;
  sendRudderCommand(); // Start without command
}

bool doRudder()
{

  if (M5Dial.BtnA.wasReleased())
  {
    last_touched = millis();
    rudderMode = false;
    return true;
  }

  auto t = M5Dial.Touch.getDetail();
  if (t.state == T_TOUCH_BEGIN)
  {
    last_touched = millis();
    M5Dial.Speaker.tone(2000, 100, 0, false);
  }
  if (t.state == T_TOUCH_END)
  {
    last_touched = millis();
    M5Dial.Speaker.tone(1000, 100, 0, false);
    edit_position = 0.0;
    updateRudder = true;
    sendRudderCommand();
    redraw = true;
  }

  redraw = redraw || menu_encoder_update(doUpdateRudder, commitRudder);
  redraw = redraw || (fabs(last_drawn_position - shipDataModel.steering.rudder_angle.deg) >= 1.0);

  return redraw;
}

bool doDetail()
{

  if (M5Dial.BtnA.wasReleased())
  {
    last_touched = millis();
    detailMode = false;
    return true;
  }
  return false;
}

void updateSteering(long count)
{
  edit_heading = shipDataModel.steering.autopilot.command.deg + (count * 1.0);

  if (shipDataModel.steering.autopilot.ap_mode.mode == ap_mode_e::HEADING_MAG ||
      shipDataModel.steering.autopilot.ap_mode.mode == ap_mode_e::COG_TRUE)
  {
    while (edit_heading < 0)
    {
      edit_heading += 360.0;
    }

    while (edit_heading >= 360.0)
    {
      edit_heading -= 360.0;
    }
  }
  else
  {
    if (edit_heading < -180)
    {
      edit_heading = 180.0;
    }

    if (edit_heading >= 179)
    {
      edit_heading = 179;
    }
  }
}

void commitSteering(long count)
{
  shipDataModel.steering.autopilot.command.deg = edit_heading;
  pypilot_send_command(pypClient.c, edit_heading);
}

void updateMenu(long count)
{
  edit_mode = modeIndex(shipDataModel.steering.autopilot.ap_mode.mode) + count;

  while (edit_mode < 0)
  {
    edit_mode = edit_mode + 4;
  }
  while (edit_mode >= 4)
  {
    edit_mode = edit_mode - 4;
  }

  USBSerial.print("Updating mode index to ");
  USBSerial.println(edit_mode);
}

void commitMenu(long count)
{

  pypilot_send_mode(pypClient.c, modeCommand(edit_mode));

  selectingMode = false;
  selectedOption = edit_mode >= 0 ? edit_mode : 4;

  USBSerial.print("A Commit ");
  USBSerial.println(selectedOption);

  USBSerial.print("Selected option ");
  USBSerial.println(selectedOption);
}

bool doNavigation()
{
  bool doRedraw = false;
  if (!selectingMode)
  {
    if (edit_mode != modeIndex(shipDataModel.steering.autopilot.ap_mode.mode))
    {
      edit_mode = modeIndex(shipDataModel.steering.autopilot.ap_mode.mode);
      doRedraw = true;
    }
  }
  if (menu_encoder_last_movement == 0)
  {
    if (edit_heading != shipDataModel.steering.autopilot.command.deg)
    {

      edit_heading = shipDataModel.steering.autopilot.command.deg;
      doRedraw = true;
    }
  }

  if (M5Dial.BtnA.wasPressed())
  {
    M5Dial.Speaker.tone(2000, 100, 0, false);
  }
  if (M5Dial.BtnA.wasReleased())
  {
    M5Dial.Speaker.tone(1000, 100, 0, false);
    // shipDataModel.steering.autopilot.ap_state.st = ap_state_e::STANDBY; // Send  data to pypilot
    last_touched = millis();
    oldPosition = M5Dial.Encoder.read();
    pypilot_send_disengage(pypClient.c);
    return true;
  }

  if (selectingMode)
  {
    doRedraw = doRedraw || menu_encoder_update(updateMenu, commitMenu);
  }
  else
  {
    doRedraw = doRedraw || menu_encoder_update(updateSteering, commitSteering);
  }

  auto t = M5Dial.Touch.getDetail();
  if (t.state == T_TOUCH_BEGIN)
  {
    M5Dial.Speaker.tone(2000, 100, 0, false);
  }
  if (t.state == T_TOUCH_END)
  {
    M5Dial.Speaker.tone(1000, 100, 0, false);
    if (shipDataModel.steering.autopilot.tack.st != ap_tack_state_e::TACK_NONE)
    {
      pypilot_send_cancel_tack(pypClient.c);
    }
    else if (abs(t.y - LV_VER_RES_MAX) < 60)
    {
      selectingMode = !selectingMode;
      doRedraw = true;
    }
    else
    {
      edit_heading = shipDataModel.steering.autopilot.heading.deg;
      commitSteering(0);
      doRedraw = true;
    }
  }

  if (t.state == T_HOLD_BEGIN)
  {

    if (t.x < 80 && abs(t.y - LV_VER_RES_MAX / 2) < 30)
    {
      aboutToTackState = ABOUT_TO_TACK_PORT;
      doRedraw = true;
    }
    else if (t.x > (LV_HOR_RES_MAX - 80) && abs(t.y - LV_VER_RES_MAX / 2) < 30)
    {
      aboutToTackState = ABOUT_TO_TACK_STARBOARD;
      doRedraw = true;
    }
  }
  else if (t.state == T_HOLD_END)
  {
    M5Dial.Speaker.tone(1000, 100, 0, false);
    if (t.x < 80 && abs(t.y - LV_VER_RES_MAX / 2) < 30)
    {
      pypilot_send_tack(pypClient.c, TACK_PORT);
      doRedraw = true;
    }
    else if (t.x > (LV_HOR_RES_MAX - 80) && abs(t.y - LV_VER_RES_MAX / 2) < 30)
    {
      pypilot_send_tack(pypClient.c, TACK_STARBOARD);
      doRedraw = true;
    }
    aboutToTackState = ABOUT_TO_TACK_NONE;
  }

  if (t.state != T_NONE)
  {
    last_touched = millis();
  }

  return doRedraw;
}

bool doStandby()
{
  bool doRedraw = false;

  if (M5Dial.BtnA.wasReleasedAfterHold())
  {
    if (color == GREEN)
    {
      setNightColor();
    }
    else
    {
      setDayColor();
    }
    doRedraw = true;
    return true;
  }

  long newPosition = M5Dial.Encoder.read();
  if (newPosition != oldPosition)
  {
    M5Dial.Speaker.tone(3000, 30, 0, false);
    last_touched = millis();
    int delta = newPosition - oldPosition;
    selectedOption = selectedOption += delta;

    while (selectedOption < 0)
    {
      selectedOption += 5;
    }
    while (selectedOption > 4)
    {
      selectedOption -= 5;
    }
    USBSerial.print("New Option ");
    USBSerial.println(selectedOption);
    doRedraw = true;
  }
  // Touch
  auto t = M5Dial.Touch.getDetail();
  oldPosition = M5Dial.Encoder.read();

  if (t.state == T_TOUCH_BEGIN)
  {
    M5Dial.Speaker.tone(2000, 100, 0, false);
  }
  if (t.state == T_HOLD_BEGIN)
  {
    M5Dial.Speaker.tone(3000, 100, 0, false);
  }
  if (t.state == T_TOUCH_END)
  {
    M5Dial.Speaker.tone(1000, 100, 0, false);
    // Check if in the center.

    if (abs(t.x - LV_HOR_RES_MAX / 2) < 20 && abs(t.y - LV_VER_RES_MAX / 2) < 20)
    {
      shipDataModel.steering.autopilot.ap_state.st = ap_state_e::STANDBY; // Send  data to pypilot
      rudderMode = true;
      updateRudder = false;
      edit_position = shipDataModel.steering.autopilot.ap_servo.position.deg;
      selectedOption = 4;
      doRedraw = true;
    }
    else if (t.y < LV_VER_RES_MAX / 2)
    {
      if (t.x < LV_VER_RES_MAX / 2)
      {
        USBSerial.println("Selected Compass");
        oldPosition = M5Dial.Encoder.read();
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_COMPASS);
        edit_heading = shipDataModel.steering.autopilot.heading.deg;
        commitSteering(0);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::HEADING_MAG;
        selectedOption = 0;
      }
      else
      {
        USBSerial.println("Selected GPS");
        oldPosition = M5Dial.Encoder.read();
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_GPS);
        edit_heading = shipDataModel.steering.autopilot.heading.deg;
        commitSteering(0);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::COG_TRUE;
        selectedOption = 1;
      }
    }
    else
    {
      if (t.x < LV_HOR_RES_MAX / 2)
      {
        USBSerial.println("Selected True Wind");
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_WIND_TRUE);
        edit_heading = shipDataModel.steering.autopilot.heading.deg;
        commitSteering(0);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::TRUE_WIND;
        selectedOption = 3;
      }
      else
      {
        USBSerial.println("Selected Wind");
        oldPosition = M5Dial.Encoder.read();
        pypilot_send_engage(pypClient.c);
        pypilot_send_mode(pypClient.c, AP_MODE_WIND);
        edit_heading = shipDataModel.steering.autopilot.heading.deg;
        commitSteering(0);
        shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
        shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::APP_WIND;
        selectedOption = 2;
      }
    }
    doRedraw = true;
  }
  else if (t.state == T_HOLD_END)
  {
    rudderMode = false;
    detailMode = true;
    doRedraw = true;
  }

  if (M5Dial.BtnA.wasPressed())
  {
    M5Dial.Speaker.tone(2000, 100, 0, false);
  }
  if (M5Dial.BtnA.wasReleased())
  {
    M5Dial.Speaker.tone(1000, 100, 0, false);
    // shipDataModel.steering.autopilot.ap_state.st = ap_state_e::STANDBY; // Send  data to pypilot
    last_touched = millis();
    oldPosition = M5Dial.Encoder.read();
    switch (selectedOption)
    {

    case 0: // Compass

      pypilot_send_engage(pypClient.c);
      pypilot_send_mode(pypClient.c, AP_MODE_COMPASS);
      edit_heading = shipDataModel.steering.autopilot.heading.deg;
      commitSteering(0);
      break;
      // shipDataModel.steering.autopilot.ap_state.st = ap_state_e::ENGAGED; // Send  data to pypilot
      // shipDataModel.steering.autopilot.ap_mode.mode = ap_mode_e::HEADING_MAG;

    case 1: // GPS
      pypilot_send_engage(pypClient.c);
      pypilot_send_mode(pypClient.c, AP_MODE_GPS);
      edit_heading = shipDataModel.steering.autopilot.heading.deg;
      commitSteering(0);
      break;

    case 2: // Wind
      pypilot_send_engage(pypClient.c);
      pypilot_send_mode(pypClient.c, AP_MODE_WIND);
      edit_heading = shipDataModel.steering.autopilot.heading.deg;
      commitSteering(0);
      break;

    case 3: // True Wind
      pypilot_send_engage(pypClient.c);
      pypilot_send_mode(pypClient.c, AP_MODE_WIND_TRUE);
      edit_heading = shipDataModel.steering.autopilot.heading.deg;
      commitSteering(0);
      break;

    case 4: // Rudder
      rudderMode = true;
      updateRudder = false;
      edit_position = shipDataModel.steering.autopilot.ap_servo.position.deg;
      break;

    default:
      break;
    }
    doRedraw = true;
  }

  if (t.state != T_NONE)
  {
    last_touched = millis();
  }
  return doRedraw;
}

/* Encoder tas
  Associated to either an int or a float. Preferibly a float
  Must hava a max and a min
  An option is if they roll around
    - If true min goes to max and oposite
    - If false stops at max or min and rests there
  A scale that translates clicks to values
  A Timeout to make the value efective
  A Callback to call when changing value : onChange
  A Callback when timeout passed : onSubmit


  Variables

    - int value, oldvalue : Valor del encoder
    - timer0 value dels millis() from last value change. If 0 ,deactivated
    - timeout time not to touch the encoder to get a value
    -

*/

bool loopTask()
{

  if (displaySaver == DISPLAY_SLEEPING)
  {
    auto t = M5Dial.Touch.getDetail();
    if (M5Dial.BtnA.isPressed() || t.state == T_TOUCH_BEGIN)
    {
      displaySaver = DISPLAY_WAKING;
      return false;
    }
    else
    {
      last_touched = 0;
      return false;
    }
  }
  else if (displaySaver == DISPLAY_WAKING)
  {
    auto t = M5Dial.Touch.getDetail();
    if (!M5Dial.BtnA.isPressed() && t.state == T_NONE)
    {
      last_touched = millis();
      return true;
    }
    else
    {
      last_touched = 0;
      return false;
    }
  }
  else
  {
    if (shipDataModel.steering.autopilot.ap_state.st == ap_state_e::STANDBY)
    {
      if (rudderMode)
      {
        return doRudder();
      }
      else if (detailMode)
      {
        return doDetail();
      }
      else
      {
        return doStandby();
      }
    }
    else
    {
      return doNavigation();
    }
  }
}

void splash()
{
  M5Dial.Display.setTextColor(color);
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
  M5Dial.Display.setTextSize(1);
  M5Dial.Display.drawString("AUTOPILOT", LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2 - 16);
  M5Dial.Display.drawString("Paco Gorina", LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2 + 16);
  M5Dial.Display.drawString(version, LV_HOR_RES_MAX / 2, LV_VER_RES_MAX / 2 + 48);
  M5Dial.Display.setFont(&fonts::Orbitron_Light_32);
  delay(5000);
}

void writePreferences()
{
  preferences.begin("m5dial_pypilot", false);
  preferences.clear();
  /* preferences.remove("SSID");
   preferences.remove("PASSWD");
   preferences.remove("PPHOST");
   preferences.remove("PPPORT");
   */

  preferences.putString("SSID", wifi_ssid);
  preferences.putString("PASSWD", wifi_password);
  preferences.putUInt("PPHOST", pypilot_tcp_host);
  preferences.putInt("PPPORT", pypilot_tcp_port);
  preferences.end();

  USBSerial.print(" Written to Preferences ");
  USBSerial.println(pypilot_tcp_host.toString());
}

void lookupPypilot()
{
  M5Dial.Display.clear(BLACK);
  M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
  M5Dial.Display.drawString("Searching", LV_HOR_RES_MAX / 2, LV_HOR_RES_MAX / 2);

  if (pypilot_tcp_host.toString() == "0.0.0.0" || pypilot_tcp_port <= 0)
  {
    USBSerial.println("Cercant pypilot");
    mdns_begin();
    int n = mdns_query_svc("pypilot", "tcp");
    if (n > 0)
    {
      pypilot_tcp_host = MDNS.IP(0);
      pypilot_tcp_port = MDNS.port(0);
      USBSerial.print("Trobat Pyilot at ");
      USBSerial.print(pypilot_tcp_host.toString());
      USBSerial.print(" port ");
      USBSerial.println(pypilot_tcp_port);
      writePreferences();
    }
    else
    {
      pypilot_tcp_host = IPAddress(192, 168, 1, 148);
      pypilot_tcp_port = 23322;
      USBSerial.print("Trobat Pyilot at ");
      USBSerial.print(pypilot_tcp_host.toString());
      USBSerial.print(" port ");
      USBSerial.println(pypilot_tcp_port);
      writePreferences();

      USBSerial.println("No he trobat pypilot");
    }
    mdns_end();
  }
}

void readPreferences()
{

  preferences.begin("m5dial_pypilot", true);
  wifi_ssid = preferences.getString("SSID", wifi_ssid);
  wifi_password = preferences.getString("PASSWD", wifi_password);
  pypilot_tcp_host = IPAddress(preferences.getUInt("PPHOST"));
  pypilot_tcp_port = preferences.getInt("PPPORT", pypilot_tcp_port);
  preferences.end();

  USBSerial.print(" Read from Preferences ");
  USBSerial.println(pypilot_tcp_host.toString());

  if (M5Dial.BtnA.isPressed())
  {
    USBSerial.println("Resetting PyPilot Host");

    pypilot_tcp_port = 0;
    M5Dial.Display.clear(BLACK);
    M5Dial.Display.setFont(&fonts::Orbitron_Light_24);
    M5Dial.Display.drawString("Cleared Host", LV_HOR_RES_MAX / 2, LV_HOR_RES_MAX / 2);
    delay(5000);
  }
  else
  {
    USBSerial.println(pypilot_tcp_host.toString());
  }
}

void setup()
{

  auto cfg = M5.config();
  M5Dial.begin(cfg, true, false);
  USBSerial.begin(115200);

  splash();
  M5Dial.update();
  readPreferences();

  M5Dial.Speaker.setVolume(128);
  last_touched = millis();

  setup_ble();
}
// Encoder dona 64 / volta

#define GO_SLEEP_TIMEOUT 600000ul // 50 '

void loop()
{

  M5Dial.update();
  app.tick();

  if (!checkConnection())
  {
    startWiFi();
  }

  redraw = redraw || loopTask();

  if (last_touched > 0 && (millis() - last_touched > GO_SLEEP_TIMEOUT))
  {
    // disconnect_clients();
    // save_page(page);
    // deep_sleep_with_touch_wakeup();
    USBSerial.println("Going to sleep");
    last_touched = 0;
    displaySaver = DISPLAY_SLEEPING;
    M5Dial.Display.sleep(); // powerSaveOn();
  }
  else if (last_touched != 0 && displaySaver == DISPLAY_WAKING)
  {
    USBSerial.println("Waking Up");
    displaySaver = DISPLAY_ACTIVE;
    M5Dial.Display.wakeup(); // powerSaveOff();
    redraw = true;
  }

  if (redraw)
  {
    drawScreen();
  }
}