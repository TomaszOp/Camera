/**
 * @file CamWifi.h
 * @brief WiFi station connection, mDNS and fallback access point.
 */
#pragma once

#include <IPAddress.h>
#include "EspCamWebConfig.h"
#include "NvsSettings.h"

namespace espcamweb {

/**
 * @brief Brings up the network for the camera.
 *
 * Connects as a station with WiFi power saving disabled (it heavily reduces FPS),
 * falls back to an access point when the connection fails and announces the HTTP service over mDNS.
 * With WIFI_SETTINGS_ON, settings saved in NVS (NvsSettings) take precedence over the code defaults.
 * Independent of the rest of the library.
 */
class CamWifi {
 public:
  /**
   * @brief Connects to WiFi or starts the fallback access point, then starts mDNS. Blocks up to
   *        WifiSettings::connectTimeoutMs.
   * @param settings Credentials, hostname, timeout and access point parameters.
   * @return true when connected as a station or the access point is running.
   */
  bool begin(const WifiSettings &settings);

#if WIFI_SETTINGS_ON
  /**
   * @brief Like begin(settings), but uses the settings saved in NVS when present.
   *
   * Saved station: connect to the saved network, on failure start the default AP.
   * Saved access point: start the saved AP, on failure start the default AP.
   * Nothing saved: same as begin(settings). The outcome is recorded with NvsSettings::noteStart().
   * @param settings Code defaults (credentials, hostname, timeout, fallback AP).
   * @param saved    Loaded NVS settings.
   * @return true when connected as a station or an access point is running.
   */
  bool begin(const WifiSettings &settings, NvsSettings &saved);
#endif

  /// @return true in station mode with an active connection.
  bool isConnected() const;

  /// @return true if the fallback access point is running.
  bool isAp() const { return ap_; }

  /// @return The board's IP address: station IP, or access point IP in AP mode.
  IPAddress ip() const;

  /// @return Signal strength in dBm, 0 in access point mode or when disconnected.
  int rssi() const;

  /// @return How the network was started (saved/default settings, fallback).
  const WifiStartInfo &startInfo() const { return info_; }

 private:
  /**
   * @brief Connects as a station; blocks up to the timeout.
   * @param s        Settings (hostname, timeout).
   * @param ssid     Network name.
   * @param password Password, nullptr or empty = open network.
   * @return true if connected.
   */
  bool connectStation(const WifiSettings &s, const char *ssid, const char *password);

  /**
   * @brief Starts an access point.
   * @param ssid     AP name.
   * @param password Password, nullptr or empty = open AP.
   * @return true if the AP is running.
   */
  bool startAp(const char *ssid, const char *password);

  /**
   * @brief Starts the default access point from WifiSettings (if apFallback allows it).
   * @param s Settings.
   * @return true if the AP is running.
   */
  bool startDefaultAp(const WifiSettings &s);

  /**
   * @brief Common start-up path: optional saved settings first, then defaults; finally mDNS.
   * @param s     Code defaults.
   * @param saved Saved settings or nullptr.
   * @return true when connected as a station or an access point is running.
   */
  bool start(const WifiSettings &s, const StoredWifi *saved);

  /// @brief Starts mDNS and announces the HTTP service. @param s Settings (hostname, port).
  void startMdns(const WifiSettings &s);

  bool ap_ = false;      ///< true when running as access point.
  WifiStartInfo info_;   ///< How the network was started.
};

}  // namespace espcamweb
