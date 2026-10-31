/**
 * @file CamOta.h
 * @brief Firmware update over WiFi (OTA). Compiled only when OTA_ON is 1.
 */
#pragma once

#include <Arduino.h>
#include <functional>
#include "EspCamWebConfig.h"

#if OTA_ON

namespace espcamweb {

/**
 * @brief Firmware update over the network, two ways:
 * - ArduinoOTA (espota protocol): PlatformIO `upload_protocol = espota`, Arduino IDE network port;
 * - web upload: CamWebServer streams the uploaded firmware.bin through webBegin()/webWrite()/webEnd().
 *
 * Attach to EspCamera with EspCamera::attachOta(); front-ends reach it only through the camera.
 * Requires a partition table with two app slots (e.g. min_spiffs.csv); the first upload after
 * switching the partition table must be done over USB/serial. The board restarts after a successful update.
 */
class CamOta {
 public:
  /**
   * @brief Starts the ArduinoOTA listener. Call after WiFi is up (station or access point).
   * @param settings Port, password ("" = none, prints a warning) and mDNS hostname.
   */
  void begin(const OtaSettings &settings = OtaSettings());

  /// @brief Processes ArduinoOTA requests; an upload runs inside this call. Call often from loop().
  void handle();

  /// @return true while an update (ArduinoOTA or web) is in progress.
  bool isUpdating() const { return updating_; }

  /// @return true if a password is configured.
  bool hasPassword() const { return password_.length() > 0; }

  /**
   * @brief Checks a password (used by the web upload's HTTP Basic Auth).
   * @param password Password supplied by the client.
   * @return true if it matches, or if no password is configured.
   */
  bool checkPassword(const String &password) const { return password_.isEmpty() || password == password_; }

  /**
   * @brief Sets a function called when an update starts (EspCamera uses it to stop the stream
   *        and switch the lamp off).
   * @param hook Function to call; may be empty.
   */
  void setStartHook(std::function<void()> hook) { startHook_ = hook; }

  /**
   * @brief Starts a web upload into the next OTA slot.
   * @param size Firmware size in bytes (Content-Length).
   * @return nullptr on success, otherwise an error message.
   */
  const char *webBegin(size_t size);

  /**
   * @brief Writes the next chunk of a web upload.
   * @param data Chunk.
   * @param len  Chunk length.
   * @return true if the whole chunk was written.
   */
  bool webWrite(const uint8_t *data, size_t len);

  /**
   * @brief Finishes (verifies and activates) or aborts a web upload.
   * @param ok false = abort (e.g. the connection dropped).
   * @return nullptr if the new firmware is verified and set to boot, otherwise an error message.
   */
  const char *webEnd(bool ok);

 private:
  /// @brief Marks the update as started and calls the start hook.
  void started();

  bool started_ = false;                ///< begin() was called.
  bool updating_ = false;               ///< Update in progress.
  String password_;                     ///< Configured password ("" = none).
  std::function<void()> startHook_;     ///< Called when an update starts.
};

}  // namespace espcamweb

#endif  // OTA_ON
