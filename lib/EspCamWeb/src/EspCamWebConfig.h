// Konfiguracja biblioteki EspCamWeb z domyślnymi wartościami dla ESP32-CAM AI Thinker.
#pragma once

#include <stdint.h>
#include "esp_camera.h"
#include "CameraPins.h"
#include "EspCamFeatures.h"

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
  int pin = LED_PIN;  // -1 = lampa wyłączona w czasie działania (całkiem: LED_ON 0)
  uint8_t channel = 2;           // kanał LEDC; kamera zajmuje kanał 0 / timer 0 (XCLK)
  uint32_t freq = 5000;
  uint8_t resolutionBits = 8;
  uint32_t dutyOn = 24;  // celowo słaba jasność (pełna moc oślepia i grzeje diodę)
};

struct SdSettings {
  const char *mountPoint = "/sdcard";
  const char *photoDir = "/photos";  // katalog na zdjęcia (tworzony automatycznie)
  bool oneBitMode = true;  // 1-bit: tylko GPIO 2, 14, 15 - zostają wolne GPIO4 (lampa) i 12/13 (serwa)
  bool formatIfMountFailed = false;
};

struct ServoSettings {
  int panPin = SERVO_PAN_PIN;    // poziom
  int tiltPin = SERVO_TILT_PIN;  // pion
  // Kanały LEDC 4 i 5 = timer 2 (wspólny, 50 Hz); kamera: kanał 0 / timer 0, lampa: kanał 2 / timer 1
  uint8_t panChannel = 4;
  uint8_t tiltChannel = 5;
  uint16_t minPulseUs = 500;   // impuls dla 0°
  uint16_t maxPulseUs = 2400;  // impuls dla 180°
  int panMin = 0, panMax = 180;    // dopuszczalny zakres kątów
  int tiltMin = 0, tiltMax = 180;
  int panStart = 90, tiltStart = 90;  // pozycja po starcie i po center()
  bool invertPan = false, invertTilt = false;
};

struct ServerSettings {
  uint16_t httpPort = 80;    // strona, /control, /status, /capture
  uint16_t streamPort = 81;  // /stream - osobna instancja httpd
};

struct ComServerSettings {
  bool echo = true;           // odsyłaj wpisywane znaki (monitor zwykle nie pokazuje ich sam)
  const char *prompt = "> ";
};

struct TelnetSettings {
  uint16_t port = TELNET_PORT;
  const char *password = TELNET_PASSWORD;  // "" = bez hasła; telnet nie szyfruje niczego
};

struct EspCamWebConfig {
  CameraSettings camera;
  WifiSettings wifi;
  LedSettings led;
  SdSettings sd;
  ServoSettings servos;
  ServerSettings server;
  ComServerSettings com;
  TelnetSettings telnet;
  // Co ile wypisywać FPS na Serial, 0 = wcale. Przy serwerze COM domyślnie wyłączone, żeby
  // logi nie przerywały wpisywania poleceń (FPS pokazują polecenia cam i status).
  uint32_t fpsLogIntervalMs = COM_SERVER_ON ? 0 : 5000;
};

}  // namespace espcamweb
