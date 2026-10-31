/**
 * @file EspCamera.h
 * @brief The camera - the hub of the library.
 *
 * Handles initialisation, frame capture, image settings, FPS and the optional services:
 * LED lamp, SD card and pan/tilt servos (enabled in EspCamFeatures.h). Works on its own,
 * without the web server. Methods of disabled services always exist and return false.
 */
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

/// Available services: enabled in EspCamFeatures.h and successfully initialised.
struct CameraFeatures {
  bool led;     ///< LED lamp attached and working.
  bool sd;      ///< SD card attached and mounted.
  bool servos;  ///< Servos attached and configured.
};

/**
 * @brief OV2640 camera with optional LED, SD card and servos attached.
 *
 * Front-ends (CamWebServer, CamComServer, CamTelnetServer) depend only on this class.
 * Thread-safe: methods that change state are protected by a recursive mutex, because they are
 * called from the httpd tasks and from loop() (COM/Telnet servers) at the same time.
 * capture() is not locked so the stream never waits for slow operations such as SD writes.
 */
class EspCamera {
 public:
  /// Creates the state mutex. The camera itself is started by begin().
  EspCamera() : mutex_(xSemaphoreCreateRecursiveMutex()) {}
  EspCamera(const EspCamera &) = delete;
  EspCamera &operator=(const EspCamera &) = delete;

  /**
   * @brief Initialises the sensor and frame buffers.
   *
   * With PSRAM: buffers in PSRAM sized for maxFrameSize, grab-latest mode, then switches to frameSize.
   * Without PSRAM: noPsramFrameSize, single buffer in DRAM. Errors are printed to Serial.
   * @param settings Pins, XCLK, resolutions, JPEG quality, buffer count.
   * @return true if the camera is ready.
   */
  bool begin(const CameraSettings &settings);

  /// @return true if PSRAM was found during begin().
  bool hasPsram() const { return hasPsram_; }

  /// @return Largest resolution the frame buffers can hold.
  framesize_t maxFrameSize() const { return maxFrameSize_; }

  /**
   * @brief Changes the resolution.
   * @param fs New frame size.
   * @return false if @p fs exceeds maxFrameSize() (the buffer would overflow) or the sensor rejects it.
   */
  bool setFrameSize(framesize_t fs);

  /**
   * @brief Sets the JPEG quality.
   * @param quality 4..63, lower = better quality and bigger frames (clamped).
   * @return true on success.
   */
  bool setQuality(int quality);

  /**
   * @brief Flips the image vertically.
   * @param on true = flipped.
   * @return true on success.
   */
  bool setVFlip(bool on);

  /**
   * @brief Mirrors the image horizontally.
   * @param on true = mirrored.
   * @return true on success.
   */
  bool setHMirror(bool on);

  /// @return Current frame size.
  framesize_t frameSize() const;
  /// @return Current JPEG quality.
  int quality() const;
  /// @return true if the image is flipped vertically.
  bool vflip() const;
  /// @return true if the image is mirrored horizontally.
  bool hmirror() const;

  /**
   * @brief Takes the latest JPEG frame and counts it for the FPS statistics.
   * @return Frame buffer (nullptr on error). Every frame must be returned with release().
   */
  camera_fb_t *capture();

  /**
   * @brief Returns a frame buffer obtained from capture() to the driver.
   * @param fb Frame buffer; nullptr is ignored.
   */
  void release(camera_fb_t *fb);

  /// @return Number of frames taken with capture() since start-up.
  uint32_t frameCount() const { return frames_; }

  /**
   * @brief Recalculates fps() once per second. Call often, e.g. from loop().
   * @param nowMs Current time, millis().
   */
  void updateFps(uint32_t nowMs);

  /// @return Frames per second measured over the last second.
  float fps() const { return fps_; }

  /// @return Which optional services are available right now.
  CameraFeatures features() const { return {hasLed(), hasSd(), hasServos()}; }

  // ---------- LED lamp (LED_ON) ----------
#if LED_ON
  /**
   * @brief Attaches the LED lamp to the camera. The lamp must outlive the camera.
   * @param led Initialised CamLed.
   */
  void attachLed(CamLed &led) { led_ = &led; }
#endif
  /// @return true if a working lamp is attached.
  bool hasLed() const;
  /**
   * @brief Switches the lamp.
   * @param on true = on.
   * @return false if no lamp is available.
   */
  bool setLed(bool on);
  /// @return true if the lamp is on (false without a lamp).
  bool ledOn() const;

  // ---------- SD card (SD_ON) ----------
#if SD_ON
  /**
   * @brief Attaches the SD card storage. The object must outlive the camera.
   * @param sd CamSdCard after begin().
   */
  void attachSd(CamSdCard &sd) { sd_ = &sd; }
#endif
  /// @return true if a mounted SD card is attached.
  bool hasSd() const;
  /**
   * @brief Takes a photo and saves it to the SD card.
   * @param[out] bytes Optional: receives the JPEG size in bytes.
   * @return Path of the saved file, or an empty String (no card, capture or write error).
   */
  String savePhotoToSd(size_t *bytes = nullptr);
  /**
   * @brief Prints the most recent photos on the card (name and size).
   * @param out      Output.
   * @param maxCount Maximum number of entries.
   * @return Number of entries printed (0 without a card).
   */
  uint32_t sdListPhotos(Print &out, uint32_t maxCount) const;
  /// @return Number of photos on the card (0 without a card).
  uint32_t sdPhotoCount() const;
  /// @return Free space on the card in bytes (0 without a card).
  uint64_t sdFreeBytes() const;
  /// @return Path of the last photo saved since start-up, or an empty String.
  String sdLastPhoto() const;

  // ---------- Pan/tilt servos (SERVO_MOTORS_ON) ----------
#if SERVO_MOTORS_ON
  /**
   * @brief Attaches the servos. The object must outlive the camera.
   * @param servos CamServos after begin().
   */
  void attachServos(CamServos &servos) { servos_ = &servos; }
#endif
  /// @return true if working servos are attached.
  bool hasServos() const;
  /**
   * @brief Moves the pan (horizontal) servo.
   * @param deg Angle in degrees (clamped to the configured range).
   * @return false if no servos are available.
   */
  bool setPan(int deg);
  /**
   * @brief Moves the tilt (vertical) servo.
   * @param deg Angle in degrees (clamped to the configured range).
   * @return false if no servos are available.
   */
  bool setTilt(int deg);
  /**
   * @brief Moves both servos to the start position.
   * @return false if no servos are available.
   */
  bool centerServos();
  /// @return Pan angle in degrees, -1 without servos.
  int pan() const;
  /// @return Tilt angle in degrees, -1 without servos.
  int tilt() const;
  /**
   * @brief Reads the allowed angle ranges.
   * @param[out] panMin,panMax   Pan range in degrees.
   * @param[out] tiltMin,tiltMax Tilt range in degrees.
   * @return false (outputs unchanged) if no servos are available.
   */
  bool servoLimits(int &panMin, int &panMax, int &tiltMin, int &tiltMax) const;

  /**
   * @brief Human-readable name of a frame size.
   * @param fs Frame size.
   * @return E.g. "VGA"; "?" for sizes not listed.
   */
  static const char *frameSizeName(framesize_t fs);

 private:
#if LED_ON
  CamLed *led_ = nullptr;  ///< Attached lamp or nullptr.
#endif
#if SD_ON
  CamSdCard *sd_ = nullptr;  ///< Attached SD card or nullptr.
#endif
#if SERVO_MOTORS_ON
  CamServos *servos_ = nullptr;  ///< Attached servos or nullptr.
#endif
  bool hasPsram_ = false;                      ///< PSRAM found in begin().
  framesize_t maxFrameSize_ = FRAMESIZE_QVGA;  ///< Size the buffers were allocated for.
  /// Recursive FreeRTOS mutex for state changes: sensor, lamp, servos, SD
  /// (std::recursive_mutex would pull in ~60 KB of C++ exception code).
  SemaphoreHandle_t mutex_;
  friend class EspCameraLock;
  std::atomic<uint32_t> frames_{0};  ///< Frames taken with capture().
  uint32_t fpsFrames_ = 0;           ///< frames_ at the last FPS update.
  uint32_t fpsLastMs_ = 0;           ///< Time of the last FPS update.
  float fps_ = 0.0f;                 ///< Last measured FPS.
};

}  // namespace espcamweb
