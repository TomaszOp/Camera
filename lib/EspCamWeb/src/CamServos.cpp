/**
 * @file CamServos.cpp
 * @brief CamServos implementation: angle to pulse width conversion on LEDC.
 */
#include "CamServos.h"

#if SERVO_MOTORS_ON

#include <Arduino.h>

namespace espcamweb {

static const uint32_t SERVO_FREQ = 50;                         ///< Servo PWM frequency, Hz (20 ms period).
static const uint8_t SERVO_RES_BITS = 14;                      ///< PWM resolution (~1.2 µs per step).
static const uint32_t SERVO_PERIOD_US = 1000000 / SERVO_FREQ;  ///< PWM period, µs.

bool CamServos::begin(const ServoSettings &settings) {
  s_ = settings;
  ready_ = ledcAttachChannel(s_.panPin, SERVO_FREQ, SERVO_RES_BITS, s_.panChannel) &&
           ledcAttachChannel(s_.tiltPin, SERVO_FREQ, SERVO_RES_BITS, s_.tiltChannel);
  if (!ready_) {
    Serial.println("Serwa: błąd konfiguracji PWM");
    return false;
  }
  center();
  Serial.printf("Serwa: pan GPIO%d, tilt GPIO%d\n", s_.panPin, s_.tiltPin);
  return true;
}

void CamServos::setPan(int deg) {
  if (!ready_) return;
  pan_ = constrain(deg, s_.panMin, s_.panMax);
  write(s_.panPin, pan_, s_.invertPan);
}

void CamServos::setTilt(int deg) {
  if (!ready_) return;
  tilt_ = constrain(deg, s_.tiltMin, s_.tiltMax);
  write(s_.tiltPin, tilt_, s_.invertTilt);
}

void CamServos::center() {
  setPan(s_.panStart);
  setTilt(s_.tiltStart);
}

void CamServos::write(uint8_t pin, int deg, bool invert) {
  if (invert) deg = 180 - deg;
  uint32_t us = s_.minPulseUs + (uint32_t)(s_.maxPulseUs - s_.minPulseUs) * deg / 180;
  ledcWrite(pin, us * ((1u << SERVO_RES_BITS) - 1) / SERVO_PERIOD_US);
}

}  // namespace espcamweb

#endif  // SERVO_MOTORS_ON
