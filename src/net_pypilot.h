#ifndef NET_PYPILOT_H
#define NET_PYPILOT_H

#ifdef __cplusplus
extern "C" {
#endif

/*

TCP client on port 23322:

Just one line commands like this:

ap.heading_command=220
ap.enabled=true
ap.enabled=false
watch={"ap.heading":0.5}
watch={"ap.mode":true}
watch={"servo.amp_hours":1}
watch={"servo.controller_temp":1}
watch={"servo.voltage":1}

*/
#define PYPILOT_TIMEOUT 1000

static const char *PROGMEM AP_MODE_COMPASS = "MODE (Compass)";
static const char *PROGMEM AP_MODE_GPS = "MODE (GPS)";
static const char *PROGMEM AP_MODE_WIND = "MODE (Wind)";
static const char *PROGMEM AP_MODE_WIND_TRUE = "MODE (True Wind)";

#define TACK_PORT "port"
#define TACK_STARBOARD "starboard"

void pypilot_greet(WiFiClient &client) {
  if (client.connected()) {
    client.println(F("watch={\"ap.heading\":0.5}"));
    client.println(F("watch={\"ap.heading_command\":true}"));
    client.println(F("watch={\"ap.enabled\":true}"));
    client.println(F("watch={\"ap.mode\":true}"));
    client.println(F("watch={\"servo.amp_hours\":1}"));
    client.println(F("watch={\"servo.position\":0.0}"));
    client.println(F("watch={\"servo.controller_temp\":1}"));
    client.println(F("watch={\"servo.voltage\":1}"));
    client.println(F("watch={\"ap.tack.state\":0.5}"));
    client.println(F("watch={\"ap.tack.direction\":1}"));
    client.println(F("watch={\"rudder.angle\":0}"));
    // client.flush();
  }
}

void pypilot_send_engage(WiFiClient &client) {
  if (client.connected()) {
    client.println(F("ap.enabled=true"));
    // client.flush();
  }
}

void pypilot_send_disengage(WiFiClient &client) {
  if (client.connected()) {
    client.println(F("ap.enabled=false"));
    // client.flush();
  }
}

void pypilot_send_command(WiFiClient &client, float heading) {
  if (client.connected()) {
    Serial.print("Sending heading ");
    Serial.println(round(heading));
    client.print(F("ap.heading_command="));
    client.println(String(heading, 1));
    // client.flush();
  }
}

void pypilot_send_rudder_command(WiFiClient &client, float command) {

  if (client.connected()) {

    // Serial.print("Sending rudderCommand "); Serial.println(String(command,
    // 2));
    client.print(F("servo.command="));
    client.println(String(command, 2));
    // client.flush();
  }
}

void pypilot_send_rudder_position(WiFiClient &client, float pos) {
  if (client.connected()) {
    float lpos = min(max(pos, -float(MAX_RUDDER)), float(MAX_RUDDER));
    Serial.print("Sending rudder Position ");
    Serial.println(String(pos, 2));
    client.print(F("servo.position_command="));
    client.println(String(lpos, 2));
    // client.flush();
  }
}

void pypilot_send_tack(WiFiClient &client, const char *direction) {
  Serial.print("Tacking to ");
  Serial.println(direction);
  if (client.connected()) {
    client.print(F("ap.tack.direction=\""));
    client.print(String(direction));
    client.println("\"");
    client.println(F("ap.tack.state=\"begin\""));
    // client.flush();
  }
}

void pypilot_send_cancel_tack(WiFiClient &client) {
  if (client.connected()) {

    client.println(F("ap.tack.state=\"none\""));
    // client.flush();
  }
}

void pypilot_send_mode(WiFiClient &client, const char *mode) {

  Serial.print("Sending mode ");
  Serial.println(mode);
  if (client.connected()) {
    if (strcmp(AP_MODE_GPS, mode) == 0) {
      client.println(F("ap.mode=\"gps\""));
    } else if (strcmp(AP_MODE_WIND, mode) == 0) {
      client.println(F("ap.mode=\"wind\""));
    } else if (strcmp(AP_MODE_WIND_TRUE, mode) == 0) {
      client.println(F("ap.mode=\"true wind\""));
    } else {
      client.println(F("ap.mode=\"compass\""));
    }
    // client.flush();
  }
}
/*
void setup_pypilot_reconnect(NetClient &client, IPAddress host, int port) {
  app.onRepeat(7000, [&client, host, port]() {
    if (!client.c.connected()) {
      setKeepAlive(client.c);
      Serial.print("Reconnecting to pypilot ");
      Serial.print(host);
      Serial.print(" : ");
      Serial.println(port);
      if (client.c.connect(host, port, 200)) {
        Serial.println("Reconnected to pypilot");
        pypilot_greet(client.c);
      } else {
        {
          Serial.println("Failed to connect to pypilot in reconnect");
        }
      }
    }
    if (client.lastActivity > 0 && (millis() - client.lastActivity) > 20000) {
      client.c.stop();
      client.lastActivity = 0;
    }
  });
}

void pypilot_subscribe(NetClient &client) {
  app.onAvailable(client.c, [&client]() {
    while (client.c.connected() &&
           client.c.available() >
               12 ) {

      bool found = pypilot_parse(client.c);
      if (found) {
        client.lastActivity = millis();

        break; 
      }
    }
  });
}

void pypilot_begin(NetClient &pypClient, IPAddress pyp_host, int pyp_port) {
  Serial.print("PyPilot Host ");
  Serial.print(pyp_host);
  Serial.print(" : ");
  Serial.println(pyp_port);
  setKeepAlive(pypClient.c);
  setup_pypilot_reconnect(pypClient, pyp_host, pyp_port);
  pypilot_subscribe(pypClient);
  if (pypClient.c.connect(pyp_host, pyp_port, 300)) {
    Serial.println("Connected to PyPilot");
    pypilot_greet(pypClient.c);
  } else {
    Serial.println("Failed to connect to pypilot in begin");
  }
}
*/

volatile bool connecting = false;
void netPylotTask(void *param) {
  for (;;) {
    // Stage 1: WiFi

    if (WiFi.status() != WL_CONNECTED) {

      if (!connecting) {
        Serial.printf("Connecting to %s / %s\n", wifi_ssid.c_str(), wifi_password.c_str());
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
        connecting = true;
      }else{
        vTaskDelay(pdMS_TO_TICKS(500));
      }
    } else {      
      if(connecting){ 
         Serial.printf("Connected to WiFi %s\n", wifi_ssid.c_str());
        connecting = false;
    }
      if (!pypClient.c.connected()) {
        lookupPypilot();
        if (!pypClient.c.connect(pypilot_tcp_host, pypilot_tcp_port)) {
          vTaskDelay(pdMS_TO_TICKS(2000));
          continue;
        }
        Serial.printf("Connected to PyPilot %s:%d\n", pypilot_tcp_host, pypilot_tcp_port);
        pypClient.lastActivity = millis();
        pypilot_greet(pypClient.c);
      } else {
        if (pypClient.c.available() > 12) {
          String line = pypClient.c.readStringUntil('\n');
          pypilot_parse(line);
          pypClient.lastActivity = millis();
        }

        if (millis() - pypClient.lastActivity > PYPILOT_TIMEOUT) {
          pypClient.c.stop();
          continue;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
