/**
 * @file EspCamera.cpp
 * @brief EspCamera implementation: sensor set-up, capture, settings, FPS and optional services.
 */
#include "EspCamera.h"

#include <Arduino.h>

namespace espcamweb {
/**
 * @brief RAII guard for EspCamera's recursive state mutex.
 *
 * Takes the mutex in the constructor and gives it back in the destructor, so a method body
 * is protected for its whole scope. Recursive, so locked methods may call each other.
 */
class EspCameraLock {
 public:
  /// @brief Waits for and takes the camera's mutex. @param cam Camera to lock.
  explicit EspCameraLock(const EspCamera &cam) : m_(cam.mutex_) { xSemaphoreTakeRecursive(m_, portMAX_DELAY); }
  /// @brief Releases the mutex.
  ~EspCameraLock() { xSemaphoreGiveRecursive(m_); }

 private:
  SemaphoreHandle_t m_;  ///< Locked mutex.
};
}  // namespace espcamweb

/// Locks the camera state until the end of the enclosing scope (use inside EspCamera methods).
#define ECW_LOCK EspCameraLock lock(*this)

namespace espcamweb {

bool EspCamera::begin(const CameraSettings &s) {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = s.pins.d0;
  config.pin_d1 = s.pins.d1;
  config.pin_d2 = s.pins.d2;
  config.pin_d3 = s.pins.d3;
  config.pin_d4 = s.pins.d4;
  config.pin_d5 = s.pins.d5;
  config.pin_d6 = s.pins.d6;
  config.pin_d7 = s.pins.d7;
  config.pin_xclk = s.pins.xclk;
  config.pin_pclk = s.pins.pclk;
  config.pin_vsync = s.pins.vsync;
  config.pin_href = s.pins.href;
  config.pin_sccb_sda = s.pins.sda;
  config.pin_sccb_scl = s.pins.scl;
  config.pin_pwdn = s.pins.pwdn;
  config.pin_reset = s.pins.reset;
  config.xclk_freq_hz = s.xclkFreqHz;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = s.jpegQuality;

  hasPsram_ = psramFound();
  if (hasPsram_) {
    // Buffers are allocated for the largest allowed resolution, then we switch to the start
    // resolution - this way changing the resolution later can never overflow the buffer.
    maxFrameSize_ = s.maxFrameSize > s.frameSize ? s.maxFrameSize : s.frameSize;
    config.frame_size = maxFrameSize_;
    config.fb_count = s.fbCount;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = s.fbCount > 1 ? CAMERA_GRAB_LATEST : CAMERA_GRAB_WHEN_EMPTY;
  } else {
    maxFrameSize_ = s.noPsramFrameSize;
    config.frame_size = maxFrameSize_;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x (%s)\n", err, esp_err_to_name(err));
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (hasPsram_ && s.frameSize != maxFrameSize_) sensor->set_framesize(sensor, s.frameSize);
  Serial.printf("Camera OK (PID 0x%x), PSRAM: %s, resolution: %s\n", sensor->id.PID,
                hasPsram_ ? "yes" : "no", frameSizeName(frameSize()));
  return true;
}

camera_fb_t *EspCamera::capture() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) frames_++;
  return fb;
}

void EspCamera::release(camera_fb_t *fb) {
  if (fb) esp_camera_fb_return(fb);
}

bool EspCamera::setFrameSize(framesize_t fs) {
  ECW_LOCK;
  if (fs < 0 || fs > maxFrameSize_) return false;
  sensor_t *s = esp_camera_sensor_get();
  return s->set_framesize(s, fs) == 0;
}

bool EspCamera::setQuality(int quality) {
  ECW_LOCK;
  sensor_t *s = esp_camera_sensor_get();
  return s->set_quality(s, constrain(quality, 4, 63)) == 0;
}

bool EspCamera::setVFlip(bool on) {
  ECW_LOCK;
  sensor_t *s = esp_camera_sensor_get();
  return s->set_vflip(s, on ? 1 : 0) == 0;
}

bool EspCamera::setHMirror(bool on) {
  ECW_LOCK;
  sensor_t *s = esp_camera_sensor_get();
  return s->set_hmirror(s, on ? 1 : 0) == 0;
}

framesize_t EspCamera::frameSize() const { return esp_camera_sensor_get()->status.framesize; }
int EspCamera::quality() const { return esp_camera_sensor_get()->status.quality; }
bool EspCamera::vflip() const { return esp_camera_sensor_get()->status.vflip; }
bool EspCamera::hmirror() const { return esp_camera_sensor_get()->status.hmirror; }

void EspCamera::updateFps(uint32_t nowMs) {
  if (nowMs - fpsLastMs_ < 1000) return;
  uint32_t total = frames_;
  fps_ = (total - fpsFrames_) * 1000.0f / (nowMs - fpsLastMs_);
  fpsFrames_ = total;
  fpsLastMs_ = nowMs;
}

// ---------- LED lamp ----------
#if LED_ON
bool EspCamera::hasLed() const { return led_ && led_->isAvailable(); }
bool EspCamera::ledOn() const { return hasLed() && led_->isOn(); }
bool EspCamera::setLed(bool on) {
  ECW_LOCK;
  if (!hasLed()) return false;
  led_->set(on);
  return true;
}
#else
bool EspCamera::hasLed() const { return false; }
bool EspCamera::ledOn() const { return false; }
bool EspCamera::setLed(bool) { return false; }
#endif

// ---------- SD card ----------
#if SD_ON
bool EspCamera::hasSd() const { return sd_ && sd_->isAvailable(); }
uint32_t EspCamera::sdPhotoCount() const { return hasSd() ? sd_->photoCount() : 0; }
uint64_t EspCamera::sdFreeBytes() const { return hasSd() ? sd_->freeBytes() : 0; }
String EspCamera::sdLastPhoto() const { return hasSd() ? sd_->lastPhoto() : String(); }
uint32_t EspCamera::sdListPhotos(Print &out, uint32_t maxCount) const {
  return hasSd() ? sd_->listPhotos(out, maxCount) : 0;
}
String EspCamera::savePhotoToSd(size_t *bytes) {
  ECW_LOCK;
  if (!hasSd()) return String();
  camera_fb_t *fb = capture();
  if (!fb) return String();
  String path = sd_->savePhoto(fb->buf, fb->len);
  if (bytes) *bytes = fb->len;
  release(fb);
  if (path.length()) Serial.printf("Photo saved: %s\n", path.c_str());
  return path;
}
#else
bool EspCamera::hasSd() const { return false; }
uint32_t EspCamera::sdPhotoCount() const { return 0; }
uint64_t EspCamera::sdFreeBytes() const { return 0; }
String EspCamera::sdLastPhoto() const { return String(); }
String EspCamera::savePhotoToSd(size_t *) { return String(); }
uint32_t EspCamera::sdListPhotos(Print &, uint32_t) const { return 0; }
#endif

// ---------- Servos ----------
#if SERVO_MOTORS_ON
bool EspCamera::hasServos() const { return servos_ && servos_->isAvailable(); }
int EspCamera::pan() const { return hasServos() ? servos_->pan() : -1; }
int EspCamera::tilt() const { return hasServos() ? servos_->tilt() : -1; }
bool EspCamera::servoLimits(int &panMin, int &panMax, int &tiltMin, int &tiltMax) const {
  if (!hasServos()) return false;
  const ServoSettings &s = servos_->settings();
  panMin = s.panMin;
  panMax = s.panMax;
  tiltMin = s.tiltMin;
  tiltMax = s.tiltMax;
  return true;
}
bool EspCamera::setPan(int deg) {
  ECW_LOCK;
  if (!hasServos()) return false;
  servos_->setPan(deg);
  return true;
}
bool EspCamera::setTilt(int deg) {
  ECW_LOCK;
  if (!hasServos()) return false;
  servos_->setTilt(deg);
  return true;
}
bool EspCamera::centerServos() {
  ECW_LOCK;
  if (!hasServos()) return false;
  servos_->center();
  return true;
}
#else
bool EspCamera::hasServos() const { return false; }
int EspCamera::pan() const { return -1; }
int EspCamera::tilt() const { return -1; }
bool EspCamera::servoLimits(int &, int &, int &, int &) const { return false; }
bool EspCamera::setPan(int) { return false; }
bool EspCamera::setTilt(int) { return false; }
bool EspCamera::centerServos() {
  ECW_LOCK; return false; }
#endif

const char *EspCamera::frameSizeName(framesize_t fs) {
  switch (fs) {
    case FRAMESIZE_QVGA: return "QVGA";
    case FRAMESIZE_CIF:  return "CIF";
    case FRAMESIZE_VGA:  return "VGA";
    case FRAMESIZE_SVGA: return "SVGA";
    case FRAMESIZE_XGA:  return "XGA";
    case FRAMESIZE_HD:   return "HD";
    case FRAMESIZE_SXGA: return "SXGA";
    case FRAMESIZE_UXGA: return "UXGA";
    default:             return "?";
  }
}

}  // namespace espcamweb
