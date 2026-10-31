// Dwa serwa: poziom (pan) i pion (tilt), PWM 50 Hz przez LEDC.
// Bez biblioteki ESP32Servo - ta sama przydziela timery LEDC i koliduje z kamerą.
#pragma once

#include "EspCamWebConfig.h"

#if SERVO_MOTORS_ON

namespace espcamweb {

class CamServos {
 public:
  bool begin(const ServoSettings &settings);
  bool isAvailable() const { return ready_; }

  // Kąty w stopniach; wartości spoza zakresu z ServoSettings są przycinane
  void setPan(int deg);
  void setTilt(int deg);
  void center();
  int pan() const { return pan_; }
  int tilt() const { return tilt_; }

 private:
  void write(uint8_t pin, int deg, bool invert);

  ServoSettings s_;
  bool ready_ = false;
  int pan_ = 90;
  int tilt_ = 90;
};

}  // namespace espcamweb

#endif  // SERVO_MOTORS_ON
