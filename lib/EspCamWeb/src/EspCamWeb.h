// EspCamWeb - podgląd kamery ESP32-CAM na żywo przez WWW.
//
// Klasy można używać osobno:
//   EspCamera     - sama kamera (init, capture, ustawienia, FPS)
//   CamWebServer  - serwer WWW dla obiektu EspCamera
//   CamWifi       - WiFi STA + mDNS + awaryjny AP
//   CamLed        - lampa LED (PWM)
// albo razem przez opcjonalną fasadę EspCamWeb.
#pragma once

#include "CamLed.h"
#include "CamWebServer.h"
#include "CamWifi.h"
#include "EspCamWebConfig.h"
#include "EspCamera.h"

namespace espcamweb {

class EspCamWeb {
 public:
  EspCamWeb() : server_(camera_, &led_) {}

  // Kamera, lampa, WiFi i serwer. false = błąd (szczegóły na Serial).
  bool begin(const EspCamWebConfig &config);
  // Wywoływać w loop(): liczy FPS i wypisuje go co fpsLogIntervalMs
  void loop();

  EspCamera &camera() { return camera_; }
  CamLed &led() { return led_; }
  CamWifi &wifi() { return wifi_; }
  CamWebServer &server() { return server_; }

 private:
  EspCamera camera_;
  CamLed led_;
  CamWifi wifi_;
  CamWebServer server_;
  uint32_t fpsLogIntervalMs_ = 0;
  uint32_t logLastMs_ = 0;
  uint32_t logFrames_ = 0;
};

}  // namespace espcamweb
