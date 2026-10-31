// Konfiguracja biblioteki EspCamWeb z domyślnymi wartościami dla ESP32-CAM AI Thinker.
#pragma once

#include <stdint.h>
#include "esp_camera.h"
#include "CameraPins.h"

namespace espcamweb {

struct CameraSettings {
  CameraPins pins = AI_THINKER_PINS;
  int xclkFreqHz = 20000000;
  framesize_t frameSize = FRAMESIZE_VGA;     // rozdzielczość startowa
  framesize_t maxFrameSize = FRAMESIZE_XGA;  // na ten rozmiar alokowane są bufory (z PSRAM)
  int jpegQuality = 12;                      // 4..63, mniej = lepsza jakość
  int fbCount = 2;                           // liczba buforów w PSRAM
  // Bez PSRAM: QVGA, jeden bufor w DRAM
  framesize_t noPsramFrameSize = FRAMESIZE_QVGA;
};

struct WifiSettings {
  const char *ssid = nullptr;  // nullptr = od razu punkt dostępowy
  const char *password = nullptr;
  const char *hostname = "esp32cam";  // mDNS: http://<hostname>.local, nullptr = bez mDNS
  uint32_t connectTimeoutMs = 15000;
  bool apFallback = true;  // gdy brak połączenia, uruchom AP
  const char *apSsid = "ESP32-CAM";
  const char *apPassword = "12345678";  // min. 8 znaków
  uint16_t mdnsHttpPort = 80;           // port ogłaszany w mDNS
};

struct LedSettings {
  int pin = AI_THINKER_LED_PIN;  // -1 = lampa wyłączona
  uint8_t channel = 2;           // kanał LEDC; kamera zajmuje kanał 0 / timer 0 (XCLK)
  uint32_t freq = 5000;
  uint8_t resolutionBits = 8;
  uint32_t dutyOn = 24;  // celowo słaba jasność (pełna moc oślepia i grzeje diodę)
};

struct ServerSettings {
  uint16_t httpPort = 80;    // strona, /control, /status, /capture
  uint16_t streamPort = 81;  // /stream - osobna instancja httpd
};

struct EspCamWebConfig {
  CameraSettings camera;
  WifiSettings wifi;
  LedSettings led;
  ServerSettings server;
  uint32_t fpsLogIntervalMs = 5000;  // co ile wypisywać FPS na Serial, 0 = wcale
};

}  // namespace espcamweb
