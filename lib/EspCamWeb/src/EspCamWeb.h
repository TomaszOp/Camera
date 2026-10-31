// EspCamWeb - podgląd kamery ESP32-CAM na żywo przez WWW.
//
// Klasy można używać osobno:
//   EspCamera     - kamera, centrum: capture, ustawienia, FPS i usługi opcjonalne
//   CamWebServer  - serwer WWW dla obiektu EspCamera (dodatek)
//   CamWifi       - WiFi STA + mDNS + awaryjny AP
//   CamLed        - lampa LED (PWM)                       [LED_ON]
//   CamSdCard     - karta microSD na zdjęcia              [SD_ON]
//   CamServos     - serwa pan/tilt                        [SERVO_MOTORS_ON]
// albo razem przez opcjonalną fasadę EspCamWeb. Moduły włącza się w config.h projektu.
#pragma once

#include "CamLed.h"
#include "CamSdCard.h"
#include "CamServos.h"
#include "CamWebServer.h"
#include "CamWifi.h"
#include "EspCamWebConfig.h"
#include "EspCamera.h"

namespace espcamweb {

class EspCamWeb {
 public:
  EspCamWeb() : server_(camera_) {}

  // Kamera, lampa, WiFi i serwer. false = błąd (szczegóły na Serial).
  bool begin(const EspCamWebConfig &config);
  // Wywoływać w loop(): liczy FPS i wypisuje go co fpsLogIntervalMs
  void loop();

  EspCamera &camera() { return camera_; }
#if LED_ON
  CamLed &led() { return led_; }
#endif
#if SD_ON
  CamSdCard &sd() { return sd_; }
#endif
#if SERVO_MOTORS_ON
  CamServos &servos() { return servos_; }
#endif
  CamWifi &wifi() { return wifi_; }
  CamWebServer &server() { return server_; }

 private:
  EspCamera camera_;
#if LED_ON
  CamLed led_;
#endif
#if SD_ON
  CamSdCard sd_;
#endif
#if SERVO_MOTORS_ON
  CamServos servos_;
#endif
  CamWifi wifi_;
  CamWebServer server_;
  uint32_t fpsLogIntervalMs_ = 0;
  uint32_t logLastMs_ = 0;
  uint32_t logFrames_ = 0;
};

}  // namespace espcamweb
