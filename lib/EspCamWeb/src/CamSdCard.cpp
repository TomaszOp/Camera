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
    Serial.println("SD card: no card or mount failed");
    mounted_ = false;
    return false;
  }

  if (!SD_MMC.exists(photoDir_) && !SD_MMC.mkdir(photoDir_)) {
    Serial.printf("SD card: cannot create directory %s\n", photoDir_.c_str());
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

  Serial.printf("SD card: %llu MB, %llu MB free, photos: %u\n", totalBytes() >> 20,
                freeBytes() >> 20, (unsigned)photoCount_);
  return true;
}

String CamSdCard::savePhoto(const uint8_t *data, size_t len) {
  if (!mounted_) return String();
  char path[64];
  snprintf(path, sizeof(path), "%s/IMG_%05u.jpg", photoDir_.c_str(), (unsigned)nextIndex_);

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    Serial.printf("SD card: cannot open %s\n", path);
    return String();
  }
  size_t written = f.write(data, len);
  f.close();
  if (written != len) {
    Serial.printf("SD card: wrote only %u of %u B\n", (unsigned)written, (unsigned)len);
    SD_MMC.remove(path);
    return String();
  }

  nextIndex_++;
  photoCount_++;
  lastPhoto_ = path;
  return lastPhoto_;
}

uint32_t CamSdCard::listPhotos(Print &out, uint32_t maxCount) const {
  uint32_t shown = 0;
  listPage(1, maxCount, [&](const String &name, uint32_t size) {
    out.printf("  %s/%s  %u KB\r\n", photoDir_.c_str(), name.c_str(), (unsigned)((size + 1023) / 1024));
    shown++;
  });
  return shown;
}

/**
 * @brief Checks the extension of a file name.
 * @param name File name.
 * @return true for .jpg or .jpeg (any case).
 */
static bool hasJpegExtension(const String &name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".jpg") || lower.endsWith(".jpeg");
}

bool CamSdCard::isValidName(const String &name) {
  if (name.length() < 1 || name.length() > 64) return false;
  if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) return false;
  for (unsigned i = 0; i < name.length(); i++) {
    if ((uint8_t)name[i] < 0x20) return false;
  }
  return hasJpegExtension(name);
}

uint32_t CamSdCard::listPage(uint32_t page, uint32_t perPage, const PhotoFn &fn) const {
  if (!mounted_ || !page || !perPage) return 0;
  /// One photo file found in the directory (12 bytes + name only for non-standard names).
  struct Entry {
    uint32_t n;     ///< Number from IMG_nnnnn.jpg, 0 for other names (listed last).
    uint32_t size;  ///< File size in bytes.
    String name;    ///< Name of a non-standard file; empty for IMG_nnnnn.jpg.
  };
  std::vector<Entry> photos;
  File dir = SD_MMC.open(photoDir_);
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) continue;
    String name = f.name();
    if (!hasJpegExtension(name)) continue;
    unsigned n;
    char check[24];
    bool standard = sscanf(name.c_str(), "IMG_%u.jpg", &n) == 1 &&
                    snprintf(check, sizeof(check), "IMG_%05u.jpg", n) > 0 && name == check;
    photos.push_back({standard ? (uint32_t)n : 0, (uint32_t)f.size(), standard ? String() : name});
  }
  std::sort(photos.begin(), photos.end(), [](const Entry &a, const Entry &b) {
    return a.n != b.n ? a.n > b.n : a.name < b.name;
  });

  uint32_t total = photos.size();
  uint32_t first = (page - 1) * perPage;
  for (uint32_t i = first; i < total && i < first + perPage; i++) {
    if (photos[i].name.length()) {
      fn(photos[i].name, photos[i].size);
    } else {
      char name[24];
      snprintf(name, sizeof(name), "IMG_%05u.jpg", (unsigned)photos[i].n);
      fn(String(name), photos[i].size);
    }
  }
  return total;
}

bool CamSdCard::readPhoto(const String &name, const std::function<bool(const uint8_t *, size_t)> &sink,
                          size_t *size) const {
  if (!mounted_ || !isValidName(name)) return false;
  File f = SD_MMC.open(photoDir_ + "/" + name, FILE_READ);
  if (!f || f.isDirectory()) return false;
  if (size) *size = f.size();
  const size_t CHUNK = 4096;
  uint8_t *buf = (uint8_t *)malloc(CHUNK);
  bool ok = buf != nullptr;
  while (ok) {
    size_t r = f.read(buf, CHUNK);
    if (r == 0) break;
    ok = sink(buf, r);
  }
  free(buf);
  f.close();
  return ok;
}

const char *CamSdCard::deletePhoto(const String &name) {
  if (!mounted_) return "no SD card";
  if (!isValidName(name)) return "invalid file name";
  String path = photoDir_ + "/" + name;
  if (!SD_MMC.exists(path)) return "file not found";
  if (!SD_MMC.remove(path)) return "delete failed";
  if (photoCount_) photoCount_--;
  if (lastPhoto_ == path) lastPhoto_ = "";
  Serial.printf("SD card: deleted %s\n", path.c_str());
  return nullptr;
}

uint64_t CamSdCard::totalBytes() const { return mounted_ ? SD_MMC.totalBytes() : 0; }

uint64_t CamSdCard::freeBytes() const {
  return mounted_ ? SD_MMC.totalBytes() - SD_MMC.usedBytes() : 0;
}

}  // namespace espcamweb

#endif  // SD_ON
