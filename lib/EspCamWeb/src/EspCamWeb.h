/**
 * @file EspCamWeb.h
 * @brief EspCamWeb - live ESP32-CAM camera preview over the web. Umbrella header.
 *
 * The classes can be used on their own:
 * - EspCamera       - the camera, the hub: capture, settings, FPS and optional services
 * - CamWebServer    - web server for an EspCamera object (front-end)
 * - CamWifi         - WiFi station + mDNS + fallback access point
 * - CamLed          - LED lamp (PWM)                       [LED_ON]
 * - CamSdCard       - microSD card for photos              [SD_ON]
 * - CamServos       - pan/tilt servos                      [SERVO_MOTORS_ON]
 * - CamComServer    - commands over the serial port (front-end) [COM_SERVER_ON]
 * - CamTelnetServer - commands over Telnet (front-end)     [TELNET_SERVER_ON]
 * - CamCommands     - command interpreter shared by COM and Telnet
 *
 * or all together through the optional EspCamWeb facade. Modules are enabled in EspCamFeatures.h.
 */
#pragma once

#include "CamComServer.h"
#include "CamCommands.h"
#include "CamLed.h"
#include "CamSdCard.h"
#include "CamServos.h"
#include "CamTelnetServer.h"
#include "CamWebServer.h"
#include "CamWifi.h"
#include "EspCamWebConfig.h"
#include "EspCamera.h"

namespace espcamweb {

/**
 * @brief Optional facade that creates, starts and wires all enabled components.
 *
 * Owns the camera, the enabled modules, WiFi and all front-ends. Modules are attached to the camera
 * automatically; a missing SD card or servo failure is not fatal - the service is just unavailable.
 */
class EspCamWeb {
 public:
  /// Constructs all components (nothing is started until begin()).
  EspCamWeb()
      : server_(camera_)
#if COM_SERVER_ON
        , com_(camera_)
#endif
#if TELNET_SERVER_ON
        , telnet_(camera_)
#endif
  {
  }

  /**
   * @brief Starts lamp, camera, SD card, servos, WiFi, web server, Telnet and COM, in this order.
   * @param config Settings of all components.
   * @return false if the camera, WiFi or web server failed (details on Serial).
   */
  bool begin(const EspCamWebConfig &config);

  /// @brief Call from loop(): updates FPS (logged every fpsLogIntervalMs), services COM and Telnet.
  void loop();

  /// @return The camera.
  EspCamera &camera() { return camera_; }
#if LED_ON
  /// @return The LED lamp.
  CamLed &led() { return led_; }
#endif
#if SD_ON
  /// @return The SD card storage.
  CamSdCard &sd() { return sd_; }
#endif
#if SERVO_MOTORS_ON
  /// @return The servos.
  CamServos &servos() { return servos_; }
#endif
  /// @return The WiFi manager.
  CamWifi &wifi() { return wifi_; }
  /// @return The web server.
  CamWebServer &server() { return server_; }
#if COM_SERVER_ON
  /// @return The serial command server.
  CamComServer &com() { return com_; }
#endif
#if TELNET_SERVER_ON
  /// @return The Telnet command server.
  CamTelnetServer &telnet() { return telnet_; }
#endif

 private:
  EspCamera camera_;
#if LED_ON
  CamLed led_;
#endif
#if SD_ON
  CamSdCard sd_;
#endif
#if SERVO_MOTORS_ON
  CamServos servos_;
#endif
  CamWifi wifi_;
  CamWebServer server_;
#if COM_SERVER_ON
  CamComServer com_;
#endif
#if TELNET_SERVER_ON
  CamTelnetServer telnet_;
#endif
  uint32_t fpsLogIntervalMs_ = 0;  ///< FPS log period, 0 = off.
  uint32_t logLastMs_ = 0;         ///< Time of the last FPS log.
  uint32_t logFrames_ = 0;         ///< Frame count at the last FPS log.
};

}  // namespace espcamweb
