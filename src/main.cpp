// ESP32-CAM AI Thinker - podgląd kamery na żywo przez WWW (biblioteka EspCamWeb).
// EspCamera jest centrum: lampa, karta SD i serwa są do niej dołączane, a serwer WWW
// dostaje tylko referencję do kamery. Moduły włącza się w include/config.h.
// Krótsza wersja przez fasadę EspCamWeb: lib/EspCamWeb/examples/Basic/main.cpp

#include <Arduino.h>
#include <EspCamWeb.h>

#include "secrets.h"

using namespace espcamweb;

static EspCamera camera;
static CamWifi wifi;
static CamWebServer server(camera);
#if LED_ON
static CamLed led;
#endif
#if SD_ON
static CamSdCard sd;
#endif
#if SERVO_MOTORS_ON
static CamServos servos;
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-CAM start");

#if LED_ON
  led.begin(LedSettings());  // LED_PIN, kanał LEDC 2
  camera.attachLed(led);
#endif

  CameraSettings cam;  // domyślnie: piny AI Thinker, VGA, jakość 12, 2 bufory, bufory na XGA
  if (!camera.begin(cam)) {
    Serial.println("Restart za 5 s...");
    delay(5000);
    ESP.restart();
  }

#if SD_ON
  sd.begin(SdSettings());  // tryb 1-bit, zdjęcia w /photos
  camera.attachSd(sd);
#endif
#if SERVO_MOTORS_ON
  servos.begin(ServoSettings());  // SERVO_PAN_PIN / SERVO_TILT_PIN, kanały LEDC 4 i 5
  camera.attachServos(servos);
#endif

  CameraFeatures f = camera.features();
  Serial.printf("Usługi: LED %s, SD %s, serwa %s\n", f.led ? "tak" : "nie", f.sd ? "tak" : "nie",
                f.servos ? "tak" : "nie");

  WifiSettings net;
  net.ssid = WIFI_SSID;
  net.password = WIFI_PASS;
  wifi.begin(net);  // brak połączenia w 15 s -> AP "ESP32-CAM" / 12345678

  ServerSettings web;  // porty 80 (strona) i 81 (strumień)
  server.begin(web);

  String ip = wifi.ip().toString();
  Serial.printf("Strona: http://%s/  strumień: http://%s:%u/stream\n", ip.c_str(), ip.c_str(),
                web.streamPort);
}

void loop() {
  static uint32_t lastLog = millis(), lastFrames = 0;
  uint32_t now = millis();
  camera.updateFps(now);

  if (now - lastLog >= 5000) {
    uint32_t total = camera.frameCount();
    Serial.printf("FPS: %.1f, klienci: %u, wolna PSRAM: %u KB\n",
                  (total - lastFrames) * 1000.0f / (now - lastLog), server.streamClients(),
                  ESP.getFreePsram() / 1024);
    lastFrames = total;
    lastLog = now;
  }
  delay(100);
}
