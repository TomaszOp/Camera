#include "EspCamera.h"

#include <Arduino.h>

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
    // Bufory alokowane dla największej dopuszczalnej rozdzielczości, potem przełączamy
    // na startową - dzięki temu zmiana rozdzielczości nie przepełni bufora.
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
    Serial.printf("Błąd inicjalizacji kamery: 0x%x (%s)\n", err, esp_err_to_name(err));
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (hasPsram_ && s.frameSize != maxFrameSize_) sensor->set_framesize(sensor, s.frameSize);
  Serial.printf("Kamera OK (PID 0x%x), PSRAM: %s, rozdzielczość: %s\n", sensor->id.PID,
                hasPsram_ ? "tak" : "nie", frameSizeName(frameSize()));
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
  if (fs < 0 || fs > maxFrameSize_) return false;
  sensor_t *s = esp_camera_sensor_get();
  return s->set_framesize(s, fs) == 0;
}

bool EspCamera::setQuality(int quality) {
  sensor_t *s = esp_camera_sensor_get();
  return s->set_quality(s, constrain(quality, 4, 63)) == 0;
}

bool EspCamera::setLed(bool on) {
  if (!hasLed()) return false;
  led_->set(on);
  return true;
}

bool EspCamera::setVFlip(bool on) {
  sensor_t *s = esp_camera_sensor_get();
  return s->set_vflip(s, on ? 1 : 0) == 0;
}

bool EspCamera::setHMirror(bool on) {
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
