/**
 * @file EspCamWebConfig.h
 * @brief Runtime settings of all EspCamWeb components, with defaults for ESP32-CAM AI Thinker.
 *
 * Each structure can be passed to the begin() of its component on its own,
 * or all of them together through EspCamWebConfig to the EspCamWeb facade.
 */
#pragma once

#include <stdint.h>
#include "esp_camera.h"
#include "CameraPins.h"
#include "EspCamFeatures.h"

namespace espcamweb {

/// Camera sensor and frame buffer settings (EspCamera::begin()).
struct CameraSettings {
  CameraPins pins = AI_THINKER_PINS;         ///< Sensor GPIO map.
  int xclkFreqHz = 20000000;                 ///< Sensor master clock (XCLK), Hz.
  framesize_t frameSize = FRAMESIZE_VGA;     ///< Resolution after start-up.
  framesize_t maxFrameSize = FRAMESIZE_XGA;  ///< Buffers are allocated for this size (with PSRAM).
  int jpegQuality = 12;                      ///< JPEG quality 4..63, lower = better.
  int fbCount = 2;                           ///< Number of frame buffers in PSRAM.
  /// Resolution used without PSRAM (single frame buffer in DRAM).
  framesize_t noPsramFrameSize = FRAMESIZE_QVGA;
};

/// WiFi station, mDNS and fallback access point settings (CamWifi::begin()).
struct WifiSettings {
  const char *ssid = nullptr;         ///< Network SSID; nullptr = start the access point right away.
  const char *password = nullptr;     ///< Network password.
  const char *hostname = "esp32cam";  ///< mDNS name: http://<hostname>.local; nullptr = no mDNS.
  uint32_t connectTimeoutMs = 15000;  ///< How long to wait for the station connection.
  bool apFallback = true;             ///< Start an access point when the connection fails.
  const char *apSsid = "ESP32-CAM";   ///< Fallback access point SSID.
  const char *apPassword = "12345678";  ///< Fallback access point password (min. 8 characters).
  uint16_t mdnsHttpPort = 80;           ///< HTTP port announced over mDNS.
};

/// Flash LED settings (CamLed::begin()).
struct LedSettings {
  int pin = LED_PIN;           ///< LED GPIO; -1 = lamp disabled at runtime (completely: LED_ON 0).
  uint8_t channel = 2;         ///< LEDC channel; the camera uses channel 0 / timer 0 (XCLK).
  uint32_t freq = 5000;        ///< PWM frequency, Hz.
  uint8_t resolutionBits = 8;  ///< PWM resolution, bits.
  uint32_t dutyOn = 24;        ///< Duty when on - deliberately dim (full power dazzles and heats the LED).
};

/// microSD card settings (CamSdCard::begin()).
struct SdSettings {
  const char *mountPoint = "/sdcard";  ///< VFS mount point.
  const char *photoDir = "/photos";    ///< Photo directory (created automatically).
  /// 1-bit mode uses only GPIO 2, 14, 15, leaving GPIO4 (lamp) and 12/13 (servos) free.
  bool oneBitMode = true;
  bool formatIfMountFailed = false;  ///< Format the card if it cannot be mounted (erases data!).
};

/// Pan/tilt servo settings (CamServos::begin()).
/// LEDC channels 4 and 5 share timer 2 (50 Hz); camera: channel 0 / timer 0, lamp: channel 2 / timer 1.
struct ServoSettings {
  int panPin = SERVO_PAN_PIN;         ///< Horizontal servo GPIO.
  int tiltPin = SERVO_TILT_PIN;       ///< Vertical servo GPIO.
  uint8_t panChannel = 4;             ///< LEDC channel of the pan servo.
  uint8_t tiltChannel = 5;            ///< LEDC channel of the tilt servo.
  uint16_t minPulseUs = 500;          ///< Pulse width for 0°, µs.
  uint16_t maxPulseUs = 2400;         ///< Pulse width for 180°, µs.
  int panMin = 0, panMax = 180;       ///< Allowed pan angle range, degrees.
  int tiltMin = 0, tiltMax = 180;     ///< Allowed tilt angle range, degrees.
  int panStart = 90, tiltStart = 90;  ///< Position after start-up and after center().
  bool invertPan = false, invertTilt = false;  ///< Reverse the direction of rotation.
};

/// Web server ports (CamWebServer::begin()).
struct ServerSettings {
  uint16_t httpPort = 80;    ///< Page, /control, /status, /capture, /save.
  uint16_t streamPort = 81;  ///< /stream - separate httpd instance.
};

/// Serial command server settings (CamComServer::begin()).
struct ComServerSettings {
  bool echo = true;           ///< Echo typed characters (serial monitors usually do not show them).
  const char *prompt = "> ";  ///< Prompt printed before each command; nullptr = none.
};

/// Telnet command server settings (CamTelnetServer::begin()).
struct TelnetSettings {
  uint16_t port = TELNET_PORT;             ///< TCP port.
  const char *password = TELNET_PASSWORD;  ///< "" = no password; Telnet encrypts nothing.
};

/// Complete configuration for the EspCamWeb facade.
struct EspCamWebConfig {
  CameraSettings camera;    ///< Camera sensor and buffers.
  WifiSettings wifi;        ///< WiFi, mDNS, fallback AP.
  LedSettings led;          ///< Flash LED (used when LED_ON).
  SdSettings sd;            ///< SD card (used when SD_ON).
  ServoSettings servos;     ///< Servos (used when SERVO_MOTORS_ON).
  ServerSettings server;    ///< Web server ports.
  ComServerSettings com;    ///< Serial commands (used when COM_SERVER_ON).
  TelnetSettings telnet;    ///< Telnet commands (used when TELNET_SERVER_ON).
  /// How often to print FPS to Serial, 0 = never. Disabled by default with the serial command
  /// server so logs do not interrupt typing (the cam and status commands show FPS).
  uint32_t fpsLogIntervalMs = COM_SERVER_ON ? 0 : 5000;
};

}  // namespace espcamweb
