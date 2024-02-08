#ifndef NET_MDNS_H
#define NET_MDNS_H

//   from command line you can discover services:
//   avahi-browse -ar

#ifdef __cplusplus
extern "C" {
#endif

 
  static bool mdns_up = false;

  bool mdns_begin() {
    if (!MDNS.begin("ESP32_Browser")) {
      return false;
    } else {
      mdns_up = true;
      return true;
    }
  }

  void mdns_end() {
    MDNS.end();
    mdns_up = false;
  }

  int mdns_query_svc(const char* service, const char* proto) {
    bool fail = false;
    if (!mdns_up) {
      fail = !mdns_begin();
    }
    if (!fail) {
      delay(50);
      return MDNS.queryService(service, proto);
    } else {
      return 0;
    }
  }

  
#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
