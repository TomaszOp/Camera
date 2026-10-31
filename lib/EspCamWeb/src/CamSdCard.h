// Karta microSD do zapisu zdjęć. Pliki: <photoDir>/IMG_00001.jpg, IMG_00002.jpg...
// Interfejs SDMMC (biblioteka SD_MMC), nie SPI (SD.h). Piny są stałe w ESP32 (slot 1):
// CLK 14, CMD 15, D0 2 (+ D1 4, D2 12, D3 13 w trybie 4-bit) - nie konfiguruje się ich.
// Karta FAT32, wkładana przed włączeniem zasilania (wykrywana tylko w begin()).
// Numeracja zamiast daty (ESP32-CAM nie ma zegara RTC); po starcie szuka najwyższego numeru.
#pragma once

#include <Arduino.h>
#include "EspCamWebConfig.h"

#if SD_ON

namespace espcamweb {

class CamSdCard {
 public:
  bool begin(const SdSettings &settings);  // false = brak karty lub błąd montowania
  bool isAvailable() const { return mounted_; }

  // Zapisuje JPEG; zwraca ścieżkę pliku albo pusty String przy błędzie
  String savePhoto(const uint8_t *data, size_t len);

  uint32_t photoCount() const { return photoCount_; }
  const String &lastPhoto() const { return lastPhoto_; }
  uint64_t totalBytes() const;
  uint64_t freeBytes() const;

 private:
  bool mounted_ = false;
  String photoDir_;
  uint32_t nextIndex_ = 1;
  uint32_t photoCount_ = 0;
  String lastPhoto_;
};

}  // namespace espcamweb

#endif  // SD_ON
