#include "CamLed.h"

#if LED_ON

#include <Arduino.h>

namespace espcamweb {

void CamLed::begin(const LedSettings &settings) {
  pin_ = settings.pin;
  dutyOn_ = settings.dutyOn;
  if (pin_ < 0) return;
  // Jawny kanał: ledcAttach mógłby zająć kanał 0, na którym kamera generuje XCLK
  ledcAttachChannel(pin_, settings.freq, settings.resolutionBits, settings.channel);
  set(false);
}

void CamLed::set(bool on) {
  if (pin_ < 0) return;
  on_ = on;
  ledcWrite(pin_, on ? dutyOn_ : 0);
}

}  // namespace espcamweb

#endif  // LED_ON
