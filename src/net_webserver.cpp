#include "net_webserver.h"
#include <Update.h>

NetWebServer::NetWebServer(AppConfig *cfg, int port, void (*onSave)())
    : server(port), config(cfg), onSave(onSave) {}

String NetWebServer::getFullUri(const String &path) const {
    return "http://" + config->deviceName + ".local/" + path;
}

void NetWebServer::handleMenu() {
    String out = "<html><head>"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>" + config->deviceName + "</title></head><body>"
                 "<h1>PyPilot Controller</h1><ul>"
                 "<li><a href=\"/prefs\">Preferences</a></li>"
                 "<li><a href=\"/restart\">Restart</a></li><hr>"
                "<li><a href=\"/update\">Firmware update</a></li>"
 
                 "</ul></body></html>";
    server.send(200, "text/html", out);
}

void NetWebServer::handlePreferences() {
    String out = "<html><head>"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>Preferences</title></head><body>"
                 "<h1><a href=\"/\">PyPilot Controller</a> / Preferences</h1>"
                 "<form action=\"/updatePrefs\" method=\"post\"><table>"
                 "<tr><td>WiFi SSID:</td><td><input name=\"ssid\" value=\""     + config->wifiSsid  + "\"></td></tr>"
                 "<tr><td>Password:</td><td><input type=\"password\" name=\"password\" value=\"" + config->wifiPassword + "\"></td></tr>"
                 "<tr><td>PyPilot host:</td><td><input name=\"ppserver\" value=\"" + config->ppServer + "\"></td></tr>"
                 "<tr><td>PyPilot port:</td><td><input type=\"number\" name=\"ppport\" value=\"" + String(config->ppPort) + "\"></td></tr>"
                 "<tr><td colspan=2 align=center><input type=\"submit\" value=\"Save\"></td></tr>"
                 "</table></form></body></html>";
    server.send(200, "text/html", out);
}

void NetWebServer::handleUpdatePreferences() {
    if (server.hasArg("ssid"))     config->wifiSsid     = server.arg("ssid");
    if (server.hasArg("password")) config->wifiPassword = server.arg("password");
    if (server.hasArg("ppserver")) config->ppServer     = server.arg("ppserver");
    if (server.hasArg("ppport"))   config->ppPort       = server.arg("ppport").toInt();
    if (onSave) onSave();
    server.send(200, "text/html",
        "<html><body><h2>Saved. Restarting...</h2>"
        "<p>Reconnect to the new network if needed.</p></body></html>");
    delay(500);
    ESP.restart();
}

void NetWebServer::handleFirmwarePage() {
    String out = "<html><head>"
                 "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                 "<title>Firmware update</title></head><body>"
                 "<h1><a href=\"/\">PyPilot Controller</a> / Firmware update</h1>"
                 "<p>Upload <code>firmware.bin</code>. The device reboots when done.</p>"
                 "<form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\">"
                 "<input type=\"file\" name=\"firmware\" accept=\".bin\"> "
                 "<input type=\"submit\" value=\"Update\">"
                 "</form></body></html>";
    server.send(200, "text/html", out);
}

void NetWebServer::handleFirmwareUpload() {
    HTTPUpload &up = server.upload();
    if (up.status == UPLOAD_FILE_START) {
        Serial.printf("OTA: receiving %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            Update.printError(Serial);
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize)
            Update.printError(Serial);
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true))
            Serial.printf("OTA: done, %u bytes\n", up.totalSize);
        else
            Update.printError(Serial);
    }
}

void NetWebServer::handleRestart() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    delay(100);
    ESP.restart();
}

void NetWebServer::begin() {
    if (started) return;
    server.on("/",            HTTP_GET,  [this]() { handleMenu(); });
    server.on("/index.html",  HTTP_GET,  [this]() { handleMenu(); });
    server.on("/prefs",       HTTP_GET,  [this]() { handlePreferences(); });
    server.on("/updatePrefs", HTTP_POST, [this]() { handleUpdatePreferences(); });
    server.on("/update",      HTTP_GET,  [this]() { handleFirmwarePage(); });
    server.on("/update",      HTTP_POST,
        [this]() {
            bool ok = !Update.hasError();
            server.sendHeader("Connection", "close");
            server.send(200, "text/html", ok
                ? "<html><body><h2>Update OK. Restarting...</h2></body></html>"
                : "<html><body><h2>Update FAILED.</h2><a href=\"/update\">Retry</a></body></html>");
            delay(500);
            if (ok) ESP.restart();
        },
        [this]() { handleFirmwareUpload(); });
    server.on("/restart",     HTTP_GET,  [this]() { handleRestart(); });
    server.onNotFound([this]() { server.send(404, "text/plain", "Not Found"); });
    server.begin();
    started = true;
    Serial.println("HTTP server started");
}

void NetWebServer::handleClient() {
    if (started) server.handleClient();
}
