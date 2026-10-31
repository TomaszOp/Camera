/**
 * @file NvsSettings.h
 * @brief WiFi settings (role, SSID, password) persisted in NVS flash. Compiled only when WIFI_SETTINGS_ON is 1.
 */
#pragma once

#include <Arduino.h>
#include "EspCamWebConfig.h"

#if WIFI_SETTINGS_ON

namespace espcamweb {

/**
 * @brief Stores the WiFi role, SSID and password in NVS using the core's Preferences library.
 *
 * Also keeps the start-up information (saved or default settings, fallback) reported by CamWifi.
 * Attach to EspCamera with EspCamera::attachSettings(); front-ends change the settings only through
 * the camera, which never exposes the password. New settings take effect after a restart.
 */
class NvsSettings {
 public:
  /**
   * @brief Loads the settings from NVS.
   * @param nvsNamespace NVS namespace (max. 15 characters).
   * @return true if NVS could be opened (whether or not settings are saved).
   */
  bool begin(const char *nvsNamespace = "ecw_wifi");

  /// @return true if valid settings are saved.
  bool hasSettings() const { return present_; }

  /// @return The saved settings (meaningful only when hasSettings()). Contains the password.
  const StoredWifi &stored() const { return stored_; }

  /**
   * @brief Validates settings without saving them.
   * @param role     Station or access point.
   * @param ssid     1-32 bytes.
   * @param password Empty (open network) or 8-63 characters (WPA2 requirement).
   * @return nullptr if valid, otherwise an English error message.
   */
  static const char *validate(WifiRole role, const String &ssid, const String &password);

  /**
   * @brief Validates and saves new settings. They are used after the next restart.
   * @param role     Station or access point.
   * @param ssid     Network name.
   * @param password Password, empty = open network.
   * @return nullptr on success, otherwise an English error message.
   */
  const char *save(WifiRole role, const String &ssid, const String &password);

  /**
   * @brief Erases the saved settings; the defaults from code are used after the next restart.
   * @return true on success.
   */
  bool clear();

  /// @brief Records how the network was started (called by CamWifi). @param info Start-up information.
  void noteStart(const WifiStartInfo &info) { start_ = info; }

  /// @return How the network was started.
  const WifiStartInfo &startInfo() const { return start_; }

 private:
  String ns_;              ///< NVS namespace.
  bool present_ = false;   ///< Valid settings are saved.
  StoredWifi stored_;      ///< Saved settings.
  WifiStartInfo start_;    ///< Start-up information from CamWifi.
};

}  // namespace espcamweb

#endif  // WIFI_SETTINGS_ON
