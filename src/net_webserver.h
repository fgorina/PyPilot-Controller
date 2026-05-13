#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "AppConfig.h"

class NetWebServer {
    ::WebServer server;
    AppConfig  *config;
    bool        started = false;
    void       (*onSave)();

    void handleMenu();
    void handlePreferences();
    void handleUpdatePreferences();
    void handleRestart();

    String getFullUri(const String &path) const;

public:
    NetWebServer(AppConfig *config, int port = 80, void (*onSave)() = nullptr);
    void begin();
    void handleClient();
};
