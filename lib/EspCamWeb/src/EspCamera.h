// Kamera - centrum biblioteki: inicjalizacja, pobieranie klatek, ustawienia obrazu, FPS
// oraz opcjonalne usługi: lampa LED, karta SD, serwa pan/tilt (włączane w config.h).
// Działa samodzielnie, bez serwera WWW. Metody wyłączonych usług istnieją zawsze i zwracają false.
// Bezpieczna wątkowo: metody zmieniające stan są chronione muteksem (wołają je zadania httpd,
// loop() z serwerami COM/Telnet itd.).
#pragma once

#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "EspCamWebConfig.h"
#include "CamLed.h"
#include "CamSdCard.h"
#include "CamServos.h"

namespace espcamweb {

// Dostępne usługi: włączone w config.h i poprawnie zainicjalizowane
struct CameraFeatures {
  bool led;
  bool sd;
  bool servos;
};

class EspCamera {
 public:
  // Odwołanie do symbolu konfiguracji - wykrywa niezgodny config.h (patrz EspCamFeatures.h)
  EspCamera() : mutex_(xSemaphoreCreateRecursiveMutex()) { asm volatile("" ::"r"(&ECW_CONFIG_SYMBOL)); }
  EspCamera(const EspCamera &) = delete;
  EspCamera &operator=(const EspCamera &) = delete;

  bool begin(const CameraSettings &settings);

  bool hasPsram() const { return hasPsram_; }
  framesize_t maxFrameSize() const { return maxFrameSize_; }

  bool setFrameSize(framesize_t fs);  // false, gdy przekracza rozmiar buforów
  bool setQuality(int quality);
  bool setVFlip(bool on);
  bool setHMirror(bool on);

  framesize_t frameSize() const;
  int quality() const;
  bool vflip() const;
  bool hmirror() const;

  // Pobiera najnowszą klatkę JPEG (nullptr przy błędzie). Każdą zwrócić przez release().
  camera_fb_t *capture();
  void release(camera_fb_t *fb);

  // Licznik klatek pobranych przez capture()
  uint32_t frameCount() const { return frames_; }
  // Wywoływać często (np. z loop()); przelicza FPS co sekundę
  void updateFps(uint32_t nowMs);
  float fps() const { return fps_; }

  CameraFeatures features() const { return {hasLed(), hasSd(), hasServos()}; }

  // ---------- Lampa LED (LED_ON) ----------
#if LED_ON
  void attachLed(CamLed &led) { led_ = &led; }
#endif
  bool hasLed() const;
  bool setLed(bool on);
  bool ledOn() const;

  // ---------- Karta SD (SD_ON) ----------
#if SD_ON
  void attachSd(CamSdCard &sd) { sd_ = &sd; }
#endif
  bool hasSd() const;
  // Robi zdjęcie i zapisuje je na karcie; zwraca ścieżkę pliku albo pusty String
  String savePhotoToSd(size_t *bytes = nullptr);
  // Wypisuje ostatnie maxCount zdjęć z karty (nazwa i rozmiar); zwraca liczbę wypisanych
  uint32_t sdListPhotos(Print &out, uint32_t maxCount) const;
  uint32_t sdPhotoCount() const;
  uint64_t sdFreeBytes() const;
  String sdLastPhoto() const;

  // ---------- Serwa pan/tilt (SERVO_MOTORS_ON) ----------
#if SERVO_MOTORS_ON
  void attachServos(CamServos &servos) { servos_ = &servos; }
#endif
  bool hasServos() const;
  bool setPan(int deg);
  bool setTilt(int deg);
  bool centerServos();
  int pan() const;   // -1 bez serw
  int tilt() const;
  bool servoLimits(int &panMin, int &panMax, int &tiltMin, int &tiltMax) const;

  static const char *frameSizeName(framesize_t fs);

 private:
#if LED_ON
  CamLed *led_ = nullptr;
#endif
#if SD_ON
  CamSdCard *sd_ = nullptr;
#endif
#if SERVO_MOTORS_ON
  CamServos *servos_ = nullptr;
#endif
  bool hasPsram_ = false;
  framesize_t maxFrameSize_ = FRAMESIZE_QVGA;
  // Rekurencyjny mutex FreeRTOS (std::recursive_mutex dociąga ok. 60 KB kodu wyjątków C++)
  // dla operacji zmieniających stan: sensor, lampa, serwa, SD
  SemaphoreHandle_t mutex_;
  friend class EspCameraLock;
  std::atomic<uint32_t> frames_{0};
  uint32_t fpsFrames_ = 0;
  uint32_t fpsLastMs_ = 0;
  float fps_ = 0.0f;
};

}  // namespace espcamweb
