// Lampa LED sterowana PWM (LEDC).
#pragma once

#include "EspCamWebConfig.h"

#if LED_ON

namespace espcamweb {

class CamLed {
 public:
  void begin(const LedSettings &settings);
  void set(bool on);
  bool isOn() const { return on_; }
  bool isAvailable() const { return pin_ >= 0; }

 private:
  int pin_ = -1;
  uint32_t dutyOn_ = 0;
  bool on_ = false;
};

}  // namespace espcamweb

#endif  // LED_ON
