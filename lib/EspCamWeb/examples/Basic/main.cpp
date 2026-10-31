/**
 * @file main.cpp
 * @brief Minimal EspCamWeb example: the facade starts the camera, enabled modules, WiFi and web server.
 */
#include <Arduino.h>
#include <EspCamWeb.h>

espcamweb::EspCamWeb cam;  ///< Facade owning all components.

/// @brief Configures WiFi and starts everything; restarts the board if start-up fails.
void setup() {
  Serial.begin(115200);

  espcamweb::EspCamWebConfig config;  // defaults: ESP32-CAM AI Thinker, VGA, ports 80/81
  config.wifi.ssid = "TwojaSiec";
  config.wifi.password = "TwojeHaslo";

  if (!cam.begin(config)) {
    delay(5000);
    ESP.restart();
  }
}

/// @brief Services the facade (FPS, COM and Telnet servers).
void loop() {
  cam.loop();  // FPS on Serial every 5 s, COM/Telnet servers (when enabled in EspCamFeatures.h)
  delay(10);
}
