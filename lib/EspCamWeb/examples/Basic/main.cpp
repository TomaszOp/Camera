// Minimalny przykład EspCamWeb: fasada uruchamia kamerę, lampę, WiFi i serwer WWW.
#include <Arduino.h>
#include <EspCamWeb.h>

espcamweb::EspCamWeb cam;

void setup() {
  Serial.begin(115200);

  espcamweb::EspCamWebConfig config;  // domyślnie ESP32-CAM AI Thinker, VGA, porty 80/81
  config.wifi.ssid = "TwojaSiec";
  config.wifi.password = "TwojeHaslo";

  if (!cam.begin(config)) {
    delay(5000);
    ESP.restart();
  }
}

void loop() {
  cam.loop();  // FPS na Serial co 5 s, serwery COM/Telnet (gdy włączone w EspCamFeatures.h)
  delay(10);
}
