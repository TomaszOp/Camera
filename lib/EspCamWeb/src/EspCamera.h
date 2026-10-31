// Kamera: inicjalizacja, pobieranie klatek, ustawienia obrazu i licznik FPS.
// Działa samodzielnie, bez serwera WWW. Opcjonalnie steruje lampą LED (attachLed).
#pragma once

#include <atomic>
#include "EspCamWebConfig.h"
#include "CamLed.h"

namespace espcamweb {

class EspCamera {
 public:
  bool begin(const CameraSettings &settings);

  bool hasPsram() const { return hasPsram_; }
  framesize_t maxFrameSize() const { return maxFrameSize_; }

  bool setFrameSize(framesize_t fs);  // false, gdy przekracza rozmiar buforów
  bool setQuality(int quality);
  // Lampa LED (opcjonalna): bez attachLed() metody setLed() zwracają false
  void attachLed(CamLed &led) { led_ = &led; }
  bool hasLed() const { return led_ && led_->isAvailable(); }
  bool setLed(bool on);
  bool ledOn() const { return hasLed() && led_->isOn(); }

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

  static const char *frameSizeName(framesize_t fs);

 private:
  CamLed *led_ = nullptr;
  bool hasPsram_ = false;
  framesize_t maxFrameSize_ = FRAMESIZE_QVGA;
  std::atomic<uint32_t> frames_{0};
  uint32_t fpsFrames_ = 0;
  uint32_t fpsLastMs_ = 0;
  float fps_ = 0.0f;
};

}  // namespace espcamweb
