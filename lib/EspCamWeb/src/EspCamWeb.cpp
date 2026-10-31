/**
 * @file EspCamWeb.cpp
 * @brief EspCamWeb facade implementation: start-up order and loop servicing.
 */
#include "EspCamWeb.h"

#include <Arduino.h>

namespace espcamweb {

bool EspCamWeb::begin(const EspCamWebConfig &config) {
  fpsLogIntervalMs_ = config.fpsLogIntervalMs;
#if LED_ON
  led_.begin(config.led);
  camera_.attachLed(led_);
#endif
  if (!camera_.begin(config.camera)) return false;
  // A missing card or servo failure is not fatal - the service is simply unavailable
#if SD_ON
  sd_.begin(config.sd);
  camera_.attachSd(sd_);
#endif
#if SERVO_MOTORS_ON
  servos_.begin(config.servos);
  camera_.attachServos(servos_);
#endif

  WifiSettings wifi = config.wifi;
  wifi.mdnsHttpPort = config.server.httpPort;
#if OTA_ON
  camera_.attachOta(ota_);
#endif
#if WIFI_SETTINGS_ON
  settings_.begin();
  camera_.attachSettings(settings_);
  if (!wifi_.begin(wifi, settings_)) return false;  // saved NVS settings take precedence
#else
  if (!wifi_.begin(wifi)) return false;
#endif
  if (!server_.begin(config.server)) return false;
#if TELNET_SERVER_ON
  telnet_.begin(config.telnet);
#endif
#if OTA_ON
  ota_.begin(config.ota);
#endif

  String ip = wifi_.ip().toString();
  Serial.printf("Page: http://%s:%u/  stream: http://%s:%u/stream\n", ip.c_str(),
                config.server.httpPort, ip.c_str(), config.server.streamPort);
  logLastMs_ = millis();
#if COM_SERVER_ON
  com_.begin(config.com);
#endif
  return true;
}

void EspCamWeb::loop() {
  uint32_t now = millis();
  camera_.updateFps(now);
#if COM_SERVER_ON
  com_.handle();
#endif
#if TELNET_SERVER_ON
  telnet_.handle();
#endif
#if OTA_ON
  ota_.handle();
#endif
  if (!fpsLogIntervalMs_ || now - logLastMs_ < fpsLogIntervalMs_) return;

  uint32_t total = camera_.frameCount();
  float fps = (total - logFrames_) * 1000.0f / (now - logLastMs_);
  logFrames_ = total;
  logLastMs_ = now;
  Serial.printf("FPS: %.1f, clients: %u, free PSRAM: %u KB, heap: %u KB\n", fps,
                server_.streamClients(), ESP.getFreePsram() / 1024, ESP.getFreeHeap() / 1024);
}

}  // namespace espcamweb
