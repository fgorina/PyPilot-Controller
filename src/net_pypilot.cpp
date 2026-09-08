#include "net_pypilot.h"
#include <ESPmDNS.h>
#include <lwip/sockets.h>

// Rudder travel limit for the manual jog (degrees); matches RudderScreen.
static constexpr float RUDDER_LIMIT = 30.0f;

// --- Low-level send helpers (all run inside netPylotTask on core 1) -----------

static void tcp_greet(WiFiClient &c) {
    c.println(F("watch={\"ap.heading\":0.5}"));
    c.println(F("watch={\"ap.heading_command\":true}"));
    c.println(F("watch={\"ap.enabled\":true}"));
    c.println(F("watch={\"ap.mode\":true}"));
    c.println(F("watch={\"servo.amp_hours\":1}"));
    c.println(F("watch={\"servo.position\":0.0}"));
    c.println(F("watch={\"servo.controller_temp\":1}"));
    c.println(F("watch={\"servo.voltage\":1}"));
    c.println(F("watch={\"ap.tack.state\":0.5}"));
    c.println(F("watch={\"ap.tack.direction\":1}"));
    c.println(F("watch={\"rudder.angle\":0}"));
}

static void tcp_engage   (WiFiClient &c) { 
    Serial.println("Engaging autopilot");
    c.println(F("ap.enabled=true")); 
}
static void tcp_disengage(WiFiClient &c) { 
    Serial.println("Disengaging autopilot");
    c.println(F("ap.enabled=false")); 
}

static void tcp_mode(WiFiClient &c, const char *mode) {
    c.print(F("ap.mode=\""));
    c.print(mode);
    c.println('"');
}

static void tcp_heading(WiFiClient &c, float h) {
    Serial.println("ap.heading_command=" + String(h, 1));
    c.print(F("ap.heading_command="));
    c.println(String(h, 1));
}

static void tcp_tack(WiFiClient &c, const char *dir) {
    c.print(F("ap.tack.direction=\""));
    c.print(dir);
    c.println('"');
    c.println(F("ap.tack.state=\"begin\""));
}

static void tcp_cancel_tack(WiFiClient &c) {
    c.println(F("ap.tack.state=\"none\""));
}

static void tcp_rudder_speed(WiFiClient &c, float speed) {
    c.print(F("servo.command="));
    c.println(String(speed, 3));
}

// Proportional rudder control step — call each time servoPosition updates.
// Sends a speed command proportional to position error; clears updateRudder
// when within deadband.
static void rudder_step(WiFiClient &c, State &state) {
    float delta = state.rudderTarget - state.servoPosition;
    if (fabsf(delta) < 0.5f) {
        tcp_rudder_speed(c, 0.0f);
        state.updateRudder = false;
        return;
    }
    float speed = delta / 60.0f;                        // proportional gain
    speed = constrain(speed, -1.0f, 1.0f);
    if (speed > 0.0f && speed <  0.001f) speed =  0.001f;
    if (speed < 0.0f && speed > -0.001f) speed = -0.001f;
    tcp_rudder_speed(c, speed);
}

// Set TCP keep-alive so dead connections are detected quickly.
static void set_keepalive(WiFiClient &c) {
    int v;
    v = 1;  c.setSocketOption(SOL_SOCKET,  SO_KEEPALIVE,   &v, sizeof(v));
    v = 10; c.setSocketOption(IPPROTO_TCP, TCP_KEEPIDLE,   &v, sizeof(v));
    v = 5;  c.setSocketOption(IPPROTO_TCP, TCP_KEEPCNT,    &v, sizeof(v));
    v = 5;  c.setSocketOption(IPPROTO_TCP, TCP_KEEPINTVL,  &v, sizeof(v));
}

// Resolve the PyPilot host via mDNS if ppServer is not a valid IP.
// Updates config on success. Returns resolved IPAddress.
static IPAddress resolve_host(AppConfig &config) {
    IPAddress ip;
    if (ip.fromString(config.ppServer) && ip != IPAddress(0, 0, 0, 0))
        return ip;

    Serial.println("mDNS: searching for pypilot service...");
    if (MDNS.begin("pypilot_ctrl")) {
        int n = MDNS.queryService("pypilot", "tcp");
        if (n > 0) {
            ip              = MDNS.address(0);
            config.ppPort   = MDNS.port(0);
            config.ppServer = ip.toString();
            Serial.printf("mDNS found pypilot at %s:%d\n",
                          config.ppServer.c_str(), config.ppPort);
            MDNS.end();
            return ip;
        }
        MDNS.end();
    }
    // Fall back to default
    ip = IPAddress(192, 168, 1, 148);
    Serial.println("mDNS: not found, using default 192.168.1.148");
    return ip;
}

// --- Network task (pinned to core 1) -----------------------------------------

void netPylotTask(void *param) {
    auto     *p      = static_cast<NetPylotParams *>(param);
    AppConfig &config = *p->config;
    State     &state  = *p->state;

    WiFiClient client;
    unsigned long lastActivity = 0;

    static constexpr unsigned long RECONNECT_MS = 2000;
    static constexpr unsigned long TIMEOUT_MS   = 10000;

    for (;;) {
        // --- WiFi: networkTask owns the connection — just wait here ---
        if (WiFi.status() != WL_CONNECTED) {
            if (client.connected()) {
                client.stop();
                Serial.println("WiFi lost — PyPilot disconnected");
            }
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_MS));
            continue;
        }

        // --- TCP connection ---
        if (!client.connected()) {
            IPAddress host = resolve_host(config);
            Serial.printf("Connecting to PyPilot %s:%d...\n",
                          host.toString().c_str(), config.ppPort);
            if (!client.connect(host, config.ppPort)) {
                vTaskDelay(pdMS_TO_TICKS(RECONNECT_MS));
                continue;
            }
            set_keepalive(client);
            Serial.println("Connected to PyPilot");
            lastActivity = millis();
            tcp_greet(client);
        }

        // --- Drain pending commands from the queue ---
        PylotCmd cmd;
        while (xQueueReceive(gCmdQueue, &cmd, 0) == pdTRUE) {
            switch (cmd.type) {
                case PylotCmd::Type::ENGAGE:       tcp_engage(client);                 break;
                case PylotCmd::Type::DISENGAGE:    tcp_disengage(client);              break;
                case PylotCmd::Type::MODE:         tcp_mode(client, cmd.sval);         break;
                case PylotCmd::Type::HEADING:      tcp_heading(client, cmd.fval);      break;
                case PylotCmd::Type::TACK_PORT:    tcp_tack(client, "port");           break;
                case PylotCmd::Type::TACK_STBD:    tcp_tack(client, "starboard");      break;
                case PylotCmd::Type::CANCEL_TACK:  tcp_cancel_tack(client);            break;
                case PylotCmd::Type::RUDDER_TARGET:
                    state.rudderTarget = cmd.fval;
                    state.updateRudder = true;
                    state.rudderMode   = true;
                    break;
                case PylotCmd::Type::RUDDER_STOP:
                    tcp_rudder_speed(client, 0.0f);
                    state.updateRudder = false;
                    break;
                case PylotCmd::Type::RUDDER_JOG:
                    // Manual rudder move — only while the autopilot is disengaged,
                    // and only up to the travel limit in the requested direction.
                    if (state.apState != ApState::ENGAGED &&
                        ((cmd.fval > 0 && state.rudderAngle <  (RUDDER_LIMIT - 1.0f)) ||
                         (cmd.fval < 0 && state.rudderAngle > -(RUDDER_LIMIT - 1.0f))))
                        tcp_rudder_speed(client, cmd.fval);
                    break;
            }
        }

        // --- Read incoming data ---
        if (client.available() > 0) {
            String line = client.readStringUntil('\n');
            line.trim();
            state.parse(line);
            lastActivity = millis();

            // Run rudder control loop whenever servo position arrives
            if (state.servoPosDirty) {
                state.servoPosDirty = false;
                if (state.rudderMode && state.updateRudder && client.connected())
                    rudder_step(client, state);
            }
        }

        // --- Timeout detection ---
        if (millis() - lastActivity > TIMEOUT_MS) {
            Serial.println("PyPilot connection timed out");
            client.stop();
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
