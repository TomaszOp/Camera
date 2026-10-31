/**
 * @file CamServos.h
 * @brief Two pan/tilt servos driven by 50 Hz LEDC PWM. Compiled only when SERVO_MOTORS_ON is 1.
 *
 * Does not use the ESP32Servo library - it allocates LEDC timers on its own and collides with the camera.
 */
#pragma once

#include "EspCamWebConfig.h"

#if SERVO_MOTORS_ON

namespace espcamweb {

/**
 * @brief Horizontal (pan) and vertical (tilt) servo pair.
 *
 * Angles are in degrees and clamped to the ranges from ServoSettings.
 * Attach to EspCamera with EspCamera::attachServos().
 */
class CamServos {
 public:
  /**
   * @brief Configures both PWM channels and moves the servos to the start position.
   * @param settings Pins, LEDC channels, pulse widths, angle limits, start position, inversion.
   * @return true if both PWM channels were configured; false also for a channel outside 0-7.
   */
  bool begin(const ServoSettings &settings);

  /// @return true after a successful begin().
  bool isAvailable() const { return ready_; }

  /**
   * @brief Moves the pan (horizontal) servo.
   * @param deg Target angle in degrees, clamped to [panMin, panMax].
   */
  void setPan(int deg);

  /**
   * @brief Moves the tilt (vertical) servo.
   * @param deg Target angle in degrees, clamped to [tiltMin, tiltMax].
   */
  void setTilt(int deg);

  /// @brief Moves both servos to the start position (panStart, tiltStart).
  void center();

  /// @return Current pan angle in degrees.
  int pan() const { return pan_; }

  /// @return Current tilt angle in degrees.
  int tilt() const { return tilt_; }

  /// @return Settings passed to begin() (e.g. to read the angle limits).
  const ServoSettings &settings() const { return s_; }

 private:
  /**
   * @brief Converts an angle to a pulse width and writes the PWM duty.
   * @param pin    Servo GPIO (LEDC is addressed by pin in Arduino core 3.x).
   * @param deg    Angle 0..180.
   * @param invert Mirror the angle (180 - deg).
   */
  void write(uint8_t pin, int deg, bool invert);

  ServoSettings s_;     ///< Active settings.
  bool ready_ = false;  ///< PWM configured.
  int pan_ = 90;        ///< Current pan angle.
  int tilt_ = 90;       ///< Current tilt angle.
};

}  // namespace espcamweb

#endif  // SERVO_MOTORS_ON
