/**
 * @file CamLed.cpp
 * @brief CamLed implementation: LEDC PWM for the flash LED.
 */
#include "CamLed.h"

#if LED_ON

#include <Arduino.h>

namespace espcamweb {

void CamLed::begin(const LedSettings &settings) {
  pin_ = settings.pin;
  dutyOn_ = settings.dutyOn;
  if (pin_ < 0) return;
  // Explicit channel: ledcAttach could pick channel 0, which generates the camera XCLK
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
