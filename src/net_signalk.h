#ifndef NET_SIGNALK_H
#define NET_SIGNALK_H

// SignalK WebSocket client + FreeRTOS task for Calypso wind bridge.
//
// Globals required from main.cpp:
//   skClient (WebsocketsClient), sk_server, sk_port, sk_path, sk_token,
//   sk_me, sk_socket_state, sk_mdns_done, sk_big_buffer, preferences
//
// Requires ble_calypso.h for: calypsoDeviceInfo, calypsoService,
//   calypso_putUInt8, calypsoConnected, setup_calypso_ble()
// Requires net_mdns.h for: mdns_query_svc()

static const char* sk_update_message =
  "{ \"context\": \"%s\", \"updates\": [{"
  "\"source\": {\"label\": \"ULTRASONIC\"}, \"values\": ["
  "{\"path\": \"environment.wind.angleApparent\", \"value\": %s},"
  "{\"path\": \"environment.wind.speedApparent\",  \"value\": %s},"
  "{\"path\": \"electrical.batteries.99.name\",    \"value\": \"ULTRASONIC\"},"
  "{\"path\": \"electrical.batteries.99.capacity.stateOfCharge\", \"value\": %s}"
  "]}]}";

static const char* sk_subscribe_message =
  "{\"context\": \"%s\", \"subscribe\": ["
  "{\"path\": \"sensors.wind.speed\",   \"policy\": \"instant\"},"
  "{\"path\": \"sensors.wind.sensors\", \"policy\": \"instant\"}"
  "]}";

// Forward declaration — defined in main.cpp
void writeSkPreferences();

// ----- Data sender (called from Calypso BLE notification callback) ------

void sendWindData(int windSpeed, int windDirection, int batteryLevel) {
  if (!skClient.available()) return;
  char b0[14], b1[14], b2[14];
  char message[1024];
  double radians = double(windDirection) / 180.0 * PI;
  double speed   = double(windSpeed)    / 100.0;
  double level   = double(batteryLevel) / 100.0;
  sprintf(message, sk_update_message,
          sk_me.c_str(),
          dtostrf(radians, 8, 4, b0),
          dtostrf(speed,   8, 4, b1),
          dtostrf(level,   8, 4, b2));
  skClient.send(message);
}

// ----- WebSocket callbacks ----------------------------------------------

void sk_sendSubscribe() {
  char message[512];
  sprintf(message, sk_subscribe_message, sk_me.c_str());
  skClient.send(message);
}

void sk_onWsEvent(WebsocketsEvent event, String data) {
  if (event == WebsocketsEvent::ConnectionOpened) {
    sk_socket_state = 1;
    Serial.println("SignalK: WS opened");
  } else if (event == WebsocketsEvent::ConnectionClosed) {
    if (strlen(sk_server) == 0) sk_mdns_done = false;
    sk_socket_state = -2;
    Serial.println("SignalK: WS closed");
    vTaskDelay(1000);
  }
}

void sk_onWsMessage(WebsocketsMessage message) {
  if (sk_socket_state == 0 || sk_socket_state == 1) {
    sk_socket_state = 2;
    sk_sendSubscribe();
    return;
  }
  if (sk_socket_state != 2) return;

  // Process configuration updates from SignalK
  JsonDocument doc;
  deserializeJson(doc, message.c_str());
  const char* p = doc["updates"][0]["values"][0]["path"];
  if (p == nullptr) return;
  String path(p);

  if (path == "sensors.wind.speed") {
    uint8_t v = doc["updates"][0]["values"][0]["value"];
    if (calypsoDeviceInfo.dataRate != v && (v == 1 || v == 4 || v == 8)) {
      calypsoDeviceInfo.dataRate = v;
      if (calypsoService != nullptr)
        calypso_putUInt8(calypsoService, DATA_RATE_CHARACTERISTIC, v);
    }
  } else if (path == "sensors.wind.sensors") {
    uint8_t v = doc["updates"][0]["values"][0]["value"];
    if (calypsoDeviceInfo.sensorValue != v && (v == 0 || v == 1)) {
      calypsoDeviceInfo.sensorValue = v;
      if (calypsoService != nullptr)
        calypso_putUInt8(calypsoService, SENSORS_CHARACTERISTIC, v);
    }
  }
}

// ----- SignalK authentication -------------------------------------------

String sk_requestAuth() {
  HTTPClient http;
  WiFiClient wc;
  sprintf(sk_big_buffer, "http://%s:%d/signalk/v1/access/requests", sk_server, sk_port);
  http.begin(wc, String(sk_big_buffer));
  http.addHeader("Content-Type", "application/json");
  String body =
    "{\"clientId\": \"pypilot_controller\","
    " \"description\": \"PyPilot Wind Bridge\","
    " \"permissions\": \"readwrite\"}";
  int rc = http.POST(body);
  vTaskDelay(500);
  if (rc <= 0) { http.end(); return ""; }
  String payload = http.getString();
  http.end();
  Serial.printf("SK auth request: %s\n", payload.c_str());
  JsonDocument doc;
  deserializeJson(doc, payload.c_str());
  return String((const char*)doc["href"]);
}

bool sk_checkAuth(String path) {
  HTTPClient http;
  WiFiClient wc;
  sprintf(sk_big_buffer, "http://%s:%d%s", sk_server, sk_port, path.c_str());
  http.begin(wc, String(sk_big_buffer));
  int rc = http.GET();
  String payload = http.getString();
  http.end();
  Serial.printf("SK auth check: %s\n", payload.c_str());
  JsonDocument doc;
  deserializeJson(doc, payload.c_str());
  if (String((const char*)doc["accessRequest"]["permission"]) == "APPROVED") {
    sprintf(sk_token, "Bearer %s", (const char*)doc["accessRequest"]["token"]);
    writeSkPreferences();
    Serial.println("SignalK: token obtained and saved");
    return true;
  }
  return false;
}

bool sk_connectWs() {
  // If we already have a token, try to connect directly
  if (strlen(sk_token) > 0) {
    skClient.onMessage(sk_onWsMessage);
    skClient.onEvent(sk_onWsEvent);
    skClient.addHeader("Authorization", sk_token);
    bool ok = skClient.connect(sk_server, sk_port, sk_path);
    if (ok) { sk_socket_state = 0; return true; }
    // Token may be stale; clear and re-auth next time
    sk_token[0] = 0;
    return false;
  }

  // Request a new token — user must approve in SignalK admin UI
  String href = sk_requestAuth();
  if (href.length() == 0) return false;

  Serial.println("SignalK: waiting for approval in server admin UI...");
  while (!sk_checkAuth(href)) {
    vTaskDelay(5000);
  }

  if (strlen(sk_token) > 0) {
    skClient.onMessage(sk_onWsMessage);
    skClient.onEvent(sk_onWsEvent);
    skClient.addHeader("Authorization", sk_token);
    bool ok = skClient.connect(sk_server, sk_port, sk_path);
    if (ok) sk_socket_state = 0;
    return ok;
  }
  return false;
}

// ----- mDNS discovery ---------------------------------------------------

void sk_discoverServer() {
  Serial.println("SignalK: mDNS discovery...");
  int n = mdns_query_svc("signalk-ws", "tcp");
  if (n > 0) {
    char found[40];
    IPAddress ip = MDNS.address(0);
    strncpy(found, ip.toString().c_str(), sizeof(found) - 1);
    found[sizeof(found) - 1] = '\0';
    if (strcmp(sk_server, found) != 0) {
      strcpy(sk_server, found);
      sk_port    = MDNS.port(0);
      sk_token[0] = 0; // server changed, token is invalid
      writeSkPreferences();
      Serial.printf("SignalK: found at %s:%d\n", sk_server, sk_port);
    }
    sk_mdns_done = true;
  } else {
    Serial.println("SignalK: not found via mDNS");
  }
}

// ----- Task -------------------------------------------------------------

void calypsoTask(void* parameter) {
  int oldState = -99;
  for (;;) {
    if (oldState != sk_socket_state) {
      Serial.printf("SK state: %d -> %d\n", oldState, sk_socket_state);
      oldState = sk_socket_state;
    }

    switch (sk_socket_state) {

      case -4: // Waiting for WiFi (managed by PyPilot's netPylotTask)
        if (WiFi.status() == WL_CONNECTED) {
          sk_socket_state = -2;
        } else {
          vTaskDelay(1000);
        }
        break;

      case -2: // WiFi up — discover SignalK and connect
        if (WiFi.status() != WL_CONNECTED) {
          sk_socket_state = -4;
          break;
        }
        if (!sk_mdns_done) {
          sk_discoverServer();
        }
        if (strlen(sk_server) > 0 && sk_port > 0) {
          if (!sk_connectWs()) vTaskDelay(5000);
        } else {
          vTaskDelay(5000);
        }
        break;

      default: // 0, 1, 2 — connected states; poll WebSocket
        if (WiFi.status() != WL_CONNECTED) {
          sk_socket_state = -4;
          break;
        }
        skClient.poll();
        break;
    }

    // Maintain Calypso BLE connection (blocks ~5 s when not connected)
    if (!calypsoConnected) setup_calypso_ble();

    vTaskDelay(10);
  }
}

#endif // NET_SIGNALK_H
