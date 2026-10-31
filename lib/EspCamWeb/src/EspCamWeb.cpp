#include "EspCamWeb.h"

#include <Arduino.h>

namespace espcamweb {

bool EspCamWeb::begin(const EspCamWebConfig &config) {
  fpsLogIntervalMs_ = config.fpsLogIntervalMs;
  led_.begin(config.led);
  if (!camera_.begin(config.camera)) return false;

  WifiSettings wifi = config.wifi;
  wifi.mdnsHttpPort = config.server.httpPort;
  if (!wifi_.begin(wifi)) return false;
  if (!server_.begin(config.server)) return false;

  String ip = wifi_.ip().toString();
  Serial.printf("Strona: http://%s:%u/  strumień: http://%s:%u/stream\n", ip.c_str(),
                config.server.httpPort, ip.c_str(), config.server.streamPort);
  logLastMs_ = millis();
  return true;
}

void EspCamWeb::loop() {
  uint32_t now = millis();
  camera_.updateFps(now);
  if (!fpsLogIntervalMs_ || now - logLastMs_ < fpsLogIntervalMs_) return;

  uint32_t total = camera_.frameCount();
  float fps = (total - logFrames_) * 1000.0f / (now - logLastMs_);
  logFrames_ = total;
  logLastMs_ = now;
  Serial.printf("FPS: %.1f, klienci: %u, wolna PSRAM: %u KB, heap: %u KB\n", fps,
                server_.streamClients(), ESP.getFreePsram() / 1024, ESP.getFreeHeap() / 1024);
}

}  // namespace espcamweb
