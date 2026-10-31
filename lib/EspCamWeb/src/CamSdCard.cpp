/**
 * @file CamSdCard.cpp
 * @brief CamSdCard implementation: SD_MMC mounting, numbered JPEG files, listing.
 */
#include "CamSdCard.h"

#if SD_ON

#include <SD_MMC.h>
#include <algorithm>
#include <vector>

namespace espcamweb {

bool CamSdCard::begin(const SdSettings &s) {
  photoDir_ = s.photoDir;
  mounted_ = SD_MMC.begin(s.mountPoint, s.oneBitMode, s.formatIfMountFailed);
  if (!mounted_ || SD_MMC.cardType() == CARD_NONE) {
    Serial.println("Karta SD: brak karty lub błąd montowania");
    mounted_ = false;
    return false;
  }

  if (!SD_MMC.exists(photoDir_) && !SD_MMC.mkdir(photoDir_)) {
    Serial.printf("Karta SD: nie można utworzyć katalogu %s\n", photoDir_.c_str());
    mounted_ = false;
    return false;
  }

  // Highest existing photo number -> next free number
  File dir = SD_MMC.open(photoDir_);
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    unsigned n;
    if (!f.isDirectory() && sscanf(f.name(), "IMG_%u.jpg", &n) == 1) {
      photoCount_++;
      if (n >= nextIndex_) nextIndex_ = n + 1;
    }
  }

  Serial.printf("Karta SD: %llu MB, wolne %llu MB, zdjęć: %u\n", totalBytes() >> 20,
                freeBytes() >> 20, (unsigned)photoCount_);
  return true;
}

String CamSdCard::savePhoto(const uint8_t *data, size_t len) {
  if (!mounted_) return String();
  char path[64];
  snprintf(path, sizeof(path), "%s/IMG_%05u.jpg", photoDir_.c_str(), (unsigned)nextIndex_);

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    Serial.printf("Karta SD: nie można otworzyć %s\n", path);
    return String();
  }
  size_t written = f.write(data, len);
  f.close();
  if (written != len) {
    Serial.printf("Karta SD: zapisano tylko %u z %u B\n", (unsigned)written, (unsigned)len);
    SD_MMC.remove(path);
    return String();
  }

  nextIndex_++;
  photoCount_++;
  lastPhoto_ = path;
  return lastPhoto_;
}

uint32_t CamSdCard::listPhotos(Print &out, uint32_t maxCount) const {
  if (!mounted_ || !maxCount) return 0;
  /// One photo file found in the directory.
  struct Entry {
    unsigned n;   ///< Number from the IMG_nnnnn.jpg name.
    size_t size;  ///< File size in bytes.
  };
  std::vector<Entry> photos;
  File dir = SD_MMC.open(photoDir_);
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    unsigned n;
    if (!f.isDirectory() && sscanf(f.name(), "IMG_%u.jpg", &n) == 1) photos.push_back({n, f.size()});
  }
  std::sort(photos.begin(), photos.end(), [](const Entry &a, const Entry &b) { return a.n > b.n; });
  uint32_t shown = std::min<uint32_t>(maxCount, photos.size());
  for (uint32_t i = 0; i < shown; i++) {
    out.printf("  %s/IMG_%05u.jpg  %u KB\r\n", photoDir_.c_str(), photos[i].n,
               (unsigned)((photos[i].size + 1023) / 1024));
  }
  return shown;
}

uint64_t CamSdCard::totalBytes() const { return mounted_ ? SD_MMC.totalBytes() : 0; }

uint64_t CamSdCard::freeBytes() const {
  return mounted_ ? SD_MMC.totalBytes() - SD_MMC.usedBytes() : 0;
}

}  // namespace espcamweb

#endif  // SD_ON
