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
  if (settings.channel >= LEDC_SAFE_CHANNELS) {
    // Channels 8-15 would reuse the camera's XCLK timer (see LEDC_SAFE_CHANNELS)
    Serial.printf("LED: LEDC channel %u not allowed (use 0-%u)\n", settings.channel, LEDC_SAFE_CHANNELS - 1);
    pin_ = -1;
    return;
  }
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
