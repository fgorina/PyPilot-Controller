#include <Arduino.h>
#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <vector>

#include "AppConfig.h"
#include "ModeScreen.h"
#include "PilotScreen.h"
#include "RudderScreen.h"
#include "Screen.h"
#include "State.h"
#include "ble_server.h"
#include "net_pypilot.h"
#include "net_webserver.h"

// --- Physical button GPIO pins -----------------------------------------------
// Both wired with external pull-ups (INPUT_PULLUP); pressed = LOW.

static constexpr int PIN_STOP = 2;   // G2  — emergency stop
static constexpr int PIN_START = 27; // G27 — engage in Compass mode

// --- Globals -----------------------------------------------------------------

AppConfig config;
State state;
String myIp = "Connecting...";
QueueHandle_t gCmdQueue; // declared extern in net_pypilot.h

static Preferences prefs;
static NetWebServer *webServer = nullptr;
static BleServer *bleServer = nullptr;

static std::vector<Screen *> screens;
static Screen *currentScreen = nullptr;

static unsigned long lastTouched = 0;
static unsigned long lastBleNotify = 0;
static bool stopBtnWasLow = false;  // edge detection for STOP
static bool startBtnWasLow = false; // edge detection for START

static constexpr unsigned long SLEEP_TIMEOUT_MS = 600000ul; // 10 min
static constexpr unsigned long BLE_NOTIFY_MS = 500ul;       // BLE refresh rate

// --- Forward declarations ----------------------------------------------------
void writePreferences();
void switchTo(int idx);

// --- Preferences -------------------------------------------------------------

void writePreferences() {
  prefs.begin("pypilot", false);
  prefs.putString("SSID", config.wifiSsid);
  prefs.putString("PASSWD", config.wifiPassword);
  prefs.putString("PPSERVER", config.ppServer);
  prefs.putInt("PPPORT", config.ppPort);
  prefs.putString("DEVNAME", config.deviceName);
  prefs.end();
}

void readPreferences() {
  prefs.begin("pypilot", true);
  config.wifiSsid = prefs.getString("SSID", config.wifiSsid);
  config.wifiPassword = prefs.getString("PASSWD", config.wifiPassword);
  config.ppServer = prefs.getString("PPSERVER", config.ppServer);
  config.ppPort = prefs.getInt("PPPORT", config.ppPort);
  config.deviceName = prefs.getString("DEVNAME", "");
  prefs.end();

  if (config.deviceName.isEmpty()) {
    config.deviceName = "PYPILOT_" + String(random(100, 1000));
    prefs.begin("pypilot", false);
    prefs.putString("DEVNAME", config.deviceName);
    prefs.end();
  }

  Serial.println("=== Config ===");
  Serial.println("SSID:   " + config.wifiSsid);
  Serial.println("PP:     " + config.ppServer + ":" + String(config.ppPort));
  Serial.println("Device: " + config.deviceName);
  Serial.println("==============");
}

void resetNetwork() {
  config.wifiSsid = "";
  config.wifiPassword = "";
  config.ppServer = "";
  config.ppPort = 23322;
  String deviceName;
  writePreferences();
  ESP.restart();
}

// --- Screen switching --------------------------------------------------------

void switchTo(int idx) {
  if (idx < 0 || idx >= (int)screens.size())
    return;
  if (screens[idx] == currentScreen)
    return;

  if (state.displaySaver != DisplaySaverState::ACTIVE) {
    M5.Display.wakeup();
    M5.Display.setBrightness(128);
    state.displaySaver = DisplaySaverState::ACTIVE;
    lastTouched = millis();
  }
  if (currentScreen)
    currentScreen->exit(state);
  currentScreen = screens[idx];
  currentScreen->enter(state);
}

// --- WiFi --------------------------------------------------------------------
// --- WiFi
// ---------------------------------------------------------------------
bool checkConnection() { return WiFi.status() == WL_CONNECTED; }

bool startWiFi() {
  static bool servicesStarted = false;
  if (WiFi.status() == WL_CONNECTED)
    return true;

  Serial.println("Connecting to " + config.wifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 100) {
    vTaskDelay(pdMS_TO_TICKS(100));
    tries++;
  }
  if (WiFi.status() != WL_CONNECTED)
    return false;

  myIp = WiFi.localIP().toString();
  Serial.println("Connected, IP: " + myIp);

  if (!servicesStarted) {
    configTime(0, 0, "europe.pool.ntp.org");
    MDNS.begin(config.deviceName.c_str());
    webServer->begin();
    servicesStarted = true;
  }
  return true;
}

static bool startWiFiAP() {
  Serial.println("Starting AP: " + config.deviceName);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(config.deviceName.c_str(), "12345678");
  myIp = WiFi.softAPIP().toString();
  Serial.println("AP IP: " + myIp);
  MDNS.begin(config.deviceName.c_str());
  webServer->begin();
  return true;
}

// --- Splash ------------------------------------------------------------------

static void splash() {
  M5.Display.clear();
  M5.Display.setFont(&fonts::Orbitron_Light_24);
  M5.Display.setTextDatum(TC_DATUM);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.drawString("AUTOPILOT", M5.Display.width() / 2, 40);
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(CL_DATUM);
  M5.Display.drawString("WiFi: " + config.wifiSsid, 10, 100);
  M5.Display.drawString("PP:   " + config.ppServer, 10, 130);
  M5.Display.drawString("Dev:  " + config.deviceName, 10, 160);
  M5.Display.setTextDatum(CC_DATUM);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.drawString("Hold to reset WiFi", M5.Display.width() / 2, 210);

  // 5-second splash — long press resets stored WiFi
  unsigned long start = millis();
  long pressAt = -1;
  while (millis() - start < 10000) {
    M5.update();
    if (M5.Touch.getCount() > 0) {
      auto t = M5.Touch.getDetail(0);
      if (t.wasPressed())
        pressAt = millis();
      if (t.wasReleased() && pressAt >= 0) {
        if (millis() - pressAt > 1000)
          resetNetwork();
        pressAt = -1;
      }
    }
    delay(10);
  }
}

// --- Network task (core 1) ---------------------------------------------------

static NetPylotParams netParams;

static void networkTask(void *) {
  while (true) {
    if (!config.wifiSsid.isEmpty() && !checkConnection()) {
      startWiFi();
    }
    webServer->handleClient();
    vTaskDelay(10);
  }
}

// --- Physical button handling ------------------------------------------------

static void handlePhysicalButtons() {
  bool stopLow = (digitalRead(PIN_STOP) == LOW);
  bool startLow = (digitalRead(PIN_START) == LOW);

  // STOP: edge LOW → act; while held, keep re-sending disengage if AP
  //       gets re-engaged by another device (emergency stop behaviour).
  if (stopLow) {
    if (!stopBtnWasLow) {
      // Falling edge — immediate disengage
      pypilot_cmd_disengage();
      stopBtnWasLow = true;
    } else if (state.apState == ApState::ENGAGED) {
      // Still held and something re-engaged the AP → disengage again
      pypilot_cmd_disengage();
    }
  } else {
    stopBtnWasLow = false;
  }

  // START: rising-edge trigger; only acts when STOP is not held.
  if (startLow && !stopLow) {
    if (!startBtnWasLow) {
      startBtnWasLow = true;
    }
  } else if (!startLow && startBtnWasLow) {
    // Rising edge: engage in Compass mode with current heading
    startBtnWasLow = false;
    if (!stopLow) {
      pypilot_cmd_mode("compass");
      pypilot_cmd_engage();
      pypilot_cmd_heading(state.heading);
      switchTo(1); // go to PilotScreen
      lastTouched = millis();
    }
  }
}

// --- Setup -------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextSize(1.0f);

  // Physical buttons with pull-ups
  pinMode(PIN_STOP, INPUT_PULLUP);
  pinMode(PIN_START, INPUT_PULLUP);

  readPreferences();

  gCmdQueue = xQueueCreate(16, sizeof(PylotCmd));

  webServer = new NetWebServer(&config, 80, writePreferences);
  bleServer = new BleServer(&config, writePreferences);

  if (!config.wifiSsid.isEmpty()) {
    splash();
  } else {
    startWiFiAP();
  }

  bleServer->setup();

  // Screens: 0 = Mode, 1 = Pilot, 2 = Rudder
  screens.push_back(new ModeScreen(M5.Display.width(), M5.Display.height()));
  screens.push_back(new PilotScreen(M5.Display.width(), M5.Display.height()));
  screens.push_back(new RudderScreen(M5.Display.width(), M5.Display.height()));

  // Network tasks on core 1
  netParams = {&config, &state};
  xTaskCreatePinnedToCore(netPylotTask, "netPylot", 8192, &netParams, 5,
                          nullptr, 1);
  xTaskCreate(networkTask, "network", 4096, nullptr, 1, nullptr);

  lastTouched = millis();
  switchTo(0);
}

// --- Loop --------------------------------------------------------------------

void loop() {
  M5.update();

  handlePhysicalButtons();

  auto &t = M5.Touch.getDetail(0);

  // Display saver
  if (M5.Touch.getCount() > 0 && (t.wasPressed() || t.wasReleased()))
    lastTouched = millis();

  if (state.displaySaver == DisplaySaverState::SLEEPING) {
    if (t.wasPressed()) {
      M5.Display.wakeup();
      M5.Display.setBrightness(128);
      state.displaySaver = DisplaySaverState::WAKING;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    return;
  }
  if (state.displaySaver == DisplaySaverState::WAKING) {
    if (t.wasReleased()) {
      state.displaySaver = DisplaySaverState::ACTIVE;
      lastTouched = millis();
      if (currentScreen)
        currentScreen->draw(state);
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    return;
  }
  if (millis() - lastTouched > SLEEP_TIMEOUT_MS) {
    M5.Display.sleep();
    M5.Display.setBrightness(0);
    state.displaySaver = DisplaySaverState::SLEEPING;
    return;
  }

  // Run current screen
  if (currentScreen) {
    int next = currentScreen->run(t, state);
    if (next >= 0 && next < (int)screens.size())
      switchTo(next);
  }

  // BLE periodic notify
  unsigned long now = millis();
  if (bleServer && bleServer->isConnected() &&
      now - lastBleNotify > BLE_NOTIFY_MS) {
    bleServer->notifyAll(state);
    lastBleNotify = now;
  }

  vTaskDelay(pdMS_TO_TICKS(20));
}
