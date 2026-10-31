/**
 * @file main.cpp
 * @brief ESP32-CAM AI Thinker - live camera preview over the web (EspCamWeb library).
 *
 * EspCamera is the hub: the lamp, SD card and servos are attached to it, and the web server
 * gets only a reference to the camera. The COM and Telnet command servers are, like the web page,
 * front-ends using the camera. Modules and servers are enabled in lib/EspCamWeb/src/EspCamFeatures.h.
 * Shorter version using the EspCamWeb facade: lib/EspCamWeb/examples/Basic/main.cpp
 */

#include <Arduino.h>
#include <EspCamWeb.h>

#include "secrets.h"

using namespace espcamweb;

static EspCamera camera;             ///< The camera - hub of all services.
static CamWifi wifi;                 ///< WiFi station / fallback AP.
static CamWebServer server(camera);  ///< Web page, API and MJPEG stream.
#if COM_SERVER_ON
static CamComServer com(camera);  ///< Commands over Serial.
#endif
#if TELNET_SERVER_ON
static CamTelnetServer telnet(camera);  ///< Commands over Telnet.
#endif
#if LED_ON
static CamLed led;  ///< Flash LED.
#endif
#if SD_ON
static CamSdCard sd;  ///< SD card for photos.
#endif
#if SERVO_MOTORS_ON
static CamServos servos;  ///< Pan/tilt servos.
#endif
#if WIFI_SETTINGS_ON
static NvsSettings settings;  ///< WiFi settings saved in NVS (override secrets.h when present).
#endif

/**
 * @brief Starts the lamp, camera (restarts the board on failure), SD card, servos, WiFi,
 *        web server, Telnet and COM servers, and prints the addresses to Serial.
 */
void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-CAM start");
  // Hardware report: helps to recognise board variants (e.g. clones with 2 MB PSRAM or none)
  Serial.printf("CPU: %s rev %d, %d cores, %u MHz\n", ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getChipCores(), (unsigned)ESP.getCpuFreqMHz());
  if (psramFound()) {
    Serial.printf("PSRAM: yes, %u KB\n", (unsigned)(ESP.getPsramSize() / 1024));
  } else {
    Serial.println("PSRAM: no");
  }
  Serial.printf("Flash: %u MB, %u MHz\n", (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
                (unsigned)(ESP.getFlashChipSpeed() / 1000000));

#if LED_ON
  led.begin(LedSettings());  // LED_PIN, LEDC channel 2
  camera.attachLed(led);
#endif

  CameraSettings cam;  // defaults: AI Thinker pins, VGA, quality 12, 2 buffers sized for XGA
  if (!camera.begin(cam)) {
    Serial.println("Restarting in 5 s...");
    delay(5000);
    ESP.restart();
  }

#if SD_ON
  sd.begin(SdSettings());  // 1-bit mode, photos in /photos
  camera.attachSd(sd);
#endif
#if SERVO_MOTORS_ON
  servos.begin(ServoSettings());  // SERVO_PAN_PIN / SERVO_TILT_PIN, LEDC channels 4 and 5
  camera.attachServos(servos);
#endif

#if WIFI_SETTINGS_ON
  settings.begin();
  camera.attachSettings(settings);
#endif

  CameraFeatures f = camera.features();
  Serial.printf("Services: LED %s, SD %s, servos %s, WiFi settings %s\n", f.led ? "yes" : "no",
                f.sd ? "yes" : "no", f.servos ? "yes" : "no", f.settings ? "yes" : "no");

  WifiSettings net;
  net.ssid = WIFI_SSID;
  net.password = WIFI_PASS;
#if WIFI_SETTINGS_ON
  wifi.begin(net, settings);  // saved settings first; on failure the default AP "ESP32-CAM"
#else
  wifi.begin(net);  // no connection within 15 s -> AP "ESP32-CAM" / 12345678
#endif

  ServerSettings web;  // ports 80 (page) and 81 (stream)
  server.begin(web);
#if TELNET_SERVER_ON
  telnet.begin(TelnetSettings());  // TELNET_PORT, TELNET_PASSWORD
#endif

  String ip = wifi.ip().toString();
  Serial.printf("Page: http://%s/  stream: http://%s:%u/stream\n", ip.c_str(), ip.c_str(),
                web.streamPort);
#if COM_SERVER_ON
  com.begin(ComServerSettings());
#endif
}

/**
 * @brief Updates FPS, services the COM and Telnet servers and, without the COM server,
 *        prints FPS to Serial every 5 s.
 */
void loop() {
  static uint32_t lastLog = millis(), lastFrames = 0;
  uint32_t now = millis();
  camera.updateFps(now);
#if COM_SERVER_ON
  com.handle();
#endif
#if TELNET_SERVER_ON
  telnet.handle();
#endif

  // Without the COM server: FPS every 5 s on Serial (with it, the cam/status commands show FPS)
  if (!COM_SERVER_ON && now - lastLog >= 5000) {
    uint32_t total = camera.frameCount();
    Serial.printf("FPS: %.1f, clients: %u, free PSRAM: %u KB\n",
                  (total - lastFrames) * 1000.0f / (now - lastLog), server.streamClients(),
                  ESP.getFreePsram() / 1024);
    lastFrames = total;
    lastLog = now;
  }
  delay(10);  // short - the COM/Telnet servers are serviced in loop()
}
