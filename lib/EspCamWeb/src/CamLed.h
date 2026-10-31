/**
 * @file CamLed.h
 * @brief Flash LED lamp driven by PWM (LEDC). Compiled only when LED_ON is 1.
 */
#pragma once

#include "EspCamWebConfig.h"

#if LED_ON

namespace espcamweb {

/**
 * @brief PWM-dimmed flash LED.
 *
 * Uses an explicit LEDC channel (default 2) so it never takes channel 0,
 * which generates the camera XCLK. Attach to EspCamera with EspCamera::attachLed().
 */
class CamLed {
 public:
  /**
   * @brief Configures the PWM output and switches the lamp off.
   * @param settings Pin, LEDC channel, frequency, resolution and "on" duty. pin = -1 disables the lamp.
   */
  void begin(const LedSettings &settings);

  /**
   * @brief Switches the lamp on (with LedSettings::dutyOn) or off. No-op when unavailable.
   * @param on true = on, false = off.
   */
  void set(bool on);

  /// @return true if the lamp is currently on.
  bool isOn() const { return on_; }

  /// @return true if the lamp has a pin assigned (begin() called with pin >= 0).
  bool isAvailable() const { return pin_ >= 0; }

 private:
  int pin_ = -1;         ///< LED GPIO, -1 = unavailable.
  uint32_t dutyOn_ = 0;  ///< PWM duty used when on.
  bool on_ = false;      ///< Current state.
};

}  // namespace espcamweb

#endif  // LED_ON
