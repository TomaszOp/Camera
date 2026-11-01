/**
 * @file CamSdCard.h
 * @brief microSD card storage for photos. Compiled only when SD_ON is 1.
 *
 * Uses the SDMMC host (SD_MMC library), not SPI (SD.h). On the classic ESP32 the SDMMC slot 1
 * pins are fixed in silicon: CLK 14, CMD 15, D0 2 (+ D1 4, D2 12, D3 13 in 4-bit mode) and are
 * not configurable. The card must be FAT32 and inserted before power-up (detected only in begin()).
 * Files are numbered instead of dated (the ESP32-CAM has no RTC): <photoDir>/IMG_00001.jpg, ...
 * After start-up the numbering continues from the highest existing number.
 */
#pragma once

#include <Arduino.h>
#include <functional>
#include "EspCamWebConfig.h"

#if SD_ON

namespace espcamweb {

/**
 * @brief Mounts the SD card and stores JPEG photos with sequential file names.
 *
 * Attach to EspCamera with EspCamera::attachSd(); photos are then taken with EspCamera::savePhotoToSd().
 */
class CamSdCard {
 public:
  /**
   * @brief Mounts the card, creates the photo directory and scans it for the next free number.
   * @param settings Mount point, photo directory, bus width, format-on-failure flag.
   * @return true if the card is mounted and ready; false if there is no card or mounting failed.
   */
  bool begin(const SdSettings &settings);

  /// @return true if the card was mounted successfully.
  bool isAvailable() const { return mounted_; }

  /**
   * @brief Writes a JPEG image to the next numbered file. A partially written file is removed.
   * @param data JPEG data.
   * @param len  Data length in bytes.
   * @return Full path of the written file, or an empty String on error.
   */
  String savePhoto(const uint8_t *data, size_t len);

  /// @return Number of photos in the photo directory (scanned at start-up plus those saved since).
  uint32_t photoCount() const { return photoCount_; }

  /**
   * @brief Prints the most recent (highest numbered) photos, one per line with their size.
   * @param out      Output (Serial, Telnet...).
   * @param maxCount Maximum number of entries to print.
   * @return Number of entries printed.
   */
  uint32_t listPhotos(Print &out, uint32_t maxCount) const;

  /// Callback receiving one photo of a listing page: file name (without directory) and size in bytes.
  using PhotoFn = std::function<void(const String &name, uint32_t size)>;

  /**
   * @brief Lists one page of .jpg/.jpeg files in the photo directory, newest (highest number) first.
   *
   * One pass over the directory keeps only the number and size of each file (plus the name of files
   * not following the IMG_nnnnn.jpg pattern), then reports just the requested page.
   * @param page    Page number, 1-based.
   * @param perPage Entries per page.
   * @param fn      Called for each entry of the page.
   * @return Total number of photo files.
   */
  uint32_t listPage(uint32_t page, uint32_t perPage, const PhotoFn &fn) const;

  /**
   * @brief Checks that a name is a plain photo file name: 1-64 characters, no path separators,
   *        no "..", ending with .jpg or .jpeg.
   * @param name File name from the client.
   * @return true if safe to use inside the photo directory.
   */
  static bool isValidName(const String &name);

  /**
   * @brief Reads a photo in chunks and passes them to @p sink (the file is never loaded whole).
   * @param name File name (validated).
   * @param sink Receives each chunk; return false to stop.
   * @param[out] size Optional: file size in bytes.
   * @return false if the name is invalid, the file is missing or the sink stopped.
   */
  bool readPhoto(const String &name, const std::function<bool(const uint8_t *, size_t)> &sink,
                 size_t *size = nullptr) const;

  /**
   * @brief Deletes a photo.
   * @param name File name (validated).
   * @return nullptr on success, otherwise an English error message.
   */
  const char *deletePhoto(const String &name);

  /// @return Path of the last photo saved since start-up, or an empty String.
  const String &lastPhoto() const { return lastPhoto_; }

  /// @return Card capacity in bytes, 0 when not mounted.
  uint64_t totalBytes() const;

  /// @return Free space in bytes, 0 when not mounted.
  uint64_t freeBytes() const;

 private:
  bool mounted_ = false;     ///< Card mounted and photo directory ready.
  String photoDir_;          ///< Photo directory, relative to the mount point.
  uint32_t nextIndex_ = 1;   ///< Number of the next photo file.
  uint32_t photoCount_ = 0;  ///< Number of photos in the directory.
  String lastPhoto_;         ///< Path of the last saved photo.
};

}  // namespace espcamweb

#endif  // SD_ON
