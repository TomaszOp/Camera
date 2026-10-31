// ESP32-CAM AI Thinker - podgląd kamery na żywo przez WWW (biblioteka EspCamWeb).
// Jawne użycie klas: kamera (z lampą), WiFi i serwer WWW, który dostaje referencję do kamery.
// Krótsza wersja przez fasadę EspCamWeb: lib/EspCamWeb/examples/Basic/main.cpp

#include <Arduino.h>
#include <EspCamWeb.h>

#include "secrets.h"

using namespace espcamweb;

static EspCamera camera;
static CamLed led;
static CamWifi wifi;
static CamWebServer server(camera);  // lampą steruje przez kamerę

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-CAM start");

  led.begin(LedSettings());  // GPIO4, kanał LEDC 2
  camera.attachLed(led);

  CameraSettings cam;  // domyślnie: piny AI Thinker, VGA, jakość 12, 2 bufory, bufory na XGA
  if (!camera.begin(cam)) {
    Serial.println("Restart za 5 s...");
    delay(5000);
    ESP.restart();
  }

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
