// Połączenie WiFi (STA), mDNS i awaryjny punkt dostępowy.
#pragma once

#include <IPAddress.h>
#include "EspCamWebConfig.h"

namespace espcamweb {

class CamWifi {
 public:
  // Zwraca true, gdy jest połączenie z siecią lub działa AP
  bool begin(const WifiSettings &settings);

  bool isConnected() const;  // tryb STA i połączono
  bool isAp() const { return ap_; }
  IPAddress ip() const;
  int rssi() const;  // 0 w trybie AP

 private:
  bool ap_ = false;
};

}  // namespace espcamweb
