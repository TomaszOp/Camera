/**
 * @file CamWifi.cpp
 * @brief CamWifi implementation: station connection, AP fallback, mDNS.
 */
#include "CamWifi.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

namespace espcamweb {

bool CamWifi::begin(const WifiSettings &s) {
  ap_ = false;
  if (s.ssid && *s.ssid) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // WiFi power saving heavily reduces FPS
    if (s.hostname) WiFi.setHostname(s.hostname);
    WiFi.begin(s.ssid, s.password);
    Serial.printf("Connecting to WiFi \"%s\"", s.ssid);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < s.connectTimeoutMs) {
      delay(500);
      Serial.print('.');
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected, IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else if (s.apFallback) {
    Serial.println("No connection - starting the access point");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    if (!WiFi.softAP(s.apSsid, s.apPassword)) {
      Serial.println("Failed to start the access point");
      return false;
    }
    ap_ = true;
    Serial.printf("AP \"%s\" (password %s), IP: %s\n", s.apSsid, s.apPassword,
                  WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("No WiFi connection");
    return false;
  }

  if (s.hostname && MDNS.begin(s.hostname)) {
    MDNS.addService("http", "tcp", s.mdnsHttpPort);
    Serial.printf("mDNS: http://%s.local\n", s.hostname);
  }
  return true;
}

bool CamWifi::isConnected() const { return !ap_ && WiFi.status() == WL_CONNECTED; }

IPAddress CamWifi::ip() const { return ap_ ? WiFi.softAPIP() : WiFi.localIP(); }

int CamWifi::rssi() const { return isConnected() ? WiFi.RSSI() : 0; }

}  // namespace espcamweb
