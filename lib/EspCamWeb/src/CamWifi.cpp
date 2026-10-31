/**
 * @file CamWifi.cpp
 * @brief CamWifi implementation: saved or default settings, station connection, AP fallback, mDNS.
 */
#include "CamWifi.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>

namespace espcamweb {

bool CamWifi::begin(const WifiSettings &s) { return start(s, nullptr); }

#if WIFI_SETTINGS_ON
bool CamWifi::begin(const WifiSettings &s, NvsSettings &saved) {
  bool ok = start(s, saved.hasSettings() ? &saved.stored() : nullptr);
  saved.noteStart(info_);
  return ok;
}
#endif

bool CamWifi::connectStation(const WifiSettings &s, const char *ssid, const char *password) {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // WiFi power saving heavily reduces FPS
  if (s.hostname) WiFi.setHostname(s.hostname);
  WiFi.begin(ssid, password && *password ? password : nullptr);
  Serial.printf("Connecting to WiFi \"%s\"", ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < s.connectTimeoutMs) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.printf("Connected, IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

bool CamWifi::startAp(const char *ssid, const char *password) {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  bool open = !password || !*password;
  if (!WiFi.softAP(ssid, open ? nullptr : password)) {
    Serial.printf("Failed to start the access point \"%s\"\n", ssid);
    return false;
  }
  ap_ = true;
  // The password is never printed
  Serial.printf("AP \"%s\" (%s), IP: %s\n", ssid, open ? "open" : "password protected",
                WiFi.softAPIP().toString().c_str());
  return true;
}

bool CamWifi::startDefaultAp(const WifiSettings &s) {
  if (!s.apFallback) {
    Serial.println("No WiFi connection");
    return false;
  }
  info_.fallback = true;
  Serial.println("Falling back to the default access point");
  return startAp(s.apSsid, s.apPassword);
}

bool CamWifi::start(const WifiSettings &s, const StoredWifi *saved) {
  ap_ = false;
  info_ = WifiStartInfo();
  bool ok;

  if (saved) {
    info_.fromSaved = true;
    Serial.printf("WiFi settings: saved (%s)\n", saved->role == WifiRole::AccessPoint ? "AP" : "STA");
    if (saved->role == WifiRole::AccessPoint) {
      ok = startAp(saved->ssid.c_str(), saved->password.c_str()) || startDefaultAp(s);
    } else {
      ok = connectStation(s, saved->ssid.c_str(), saved->password.c_str()) || startDefaultAp(s);
    }
  } else {
    Serial.println("WiFi settings: default");
    ok = (s.ssid && *s.ssid && connectStation(s, s.ssid, s.password)) || startDefaultAp(s);
  }
  if (!ok) return false;

  startMdns(s);
  return true;
}

void CamWifi::startMdns(const WifiSettings &s) {
  if (s.hostname && MDNS.begin(s.hostname)) {
    MDNS.addService("http", "tcp", s.mdnsHttpPort);
    Serial.printf("mDNS: http://%s.local\n", s.hostname);
  }
}

bool CamWifi::isConnected() const { return !ap_ && WiFi.status() == WL_CONNECTED; }

IPAddress CamWifi::ip() const { return ap_ ? WiFi.softAPIP() : WiFi.localIP(); }

int CamWifi::rssi() const { return isConnected() ? WiFi.RSSI() : 0; }

}  // namespace espcamweb
