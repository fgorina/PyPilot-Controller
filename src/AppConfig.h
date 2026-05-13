#pragma once
#include <Arduino.h>

struct AppConfig {
    String wifiSsid     = "";
    String wifiPassword = "";
    String ppServer     = "";   // IP or empty → mDNS lookup
    int    ppPort       = 23322;
    String deviceName;                      // auto-generated in readPreferences()
};
