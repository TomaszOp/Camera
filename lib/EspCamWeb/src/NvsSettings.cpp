/**
 * @file NvsSettings.cpp
 * @brief NvsSettings implementation: Preferences storage and validation.
 */
#include "NvsSettings.h"

#if WIFI_SETTINGS_ON

#include <Preferences.h>

namespace espcamweb {

static const char *KEY_ROLE = "role";  ///< NVS key: 0 = station, 1 = access point.
static const char *KEY_SSID = "ssid";  ///< NVS key: SSID.
static const char *KEY_PASS = "pass";  ///< NVS key: password.

bool NvsSettings::begin(const char *nvsNamespace) {
  ns_ = nvsNamespace;
  Preferences prefs;
  if (!prefs.begin(ns_.c_str(), true)) {  // read-only; fails when the namespace does not exist yet
    present_ = false;
    return true;
  }
  stored_.role = prefs.getUChar(KEY_ROLE, 0) ? WifiRole::AccessPoint : WifiRole::Station;
  stored_.ssid = prefs.getString(KEY_SSID, "");
  stored_.password = prefs.getString(KEY_PASS, "");
  prefs.end();
  present_ = validate(stored_.role, stored_.ssid, stored_.password) == nullptr;
  return true;
}

const char *NvsSettings::validate(WifiRole, const String &ssid, const String &password) {
  if (ssid.length() < 1 || ssid.length() > 32) return "SSID must be 1-32 characters";
  if (password.length() != 0 && (password.length() < 8 || password.length() > 63)) {
    return "password must be empty (open network) or 8-63 characters";
  }
  return nullptr;
}

const char *NvsSettings::save(WifiRole role, const String &ssid, const String &password) {
  const char *err = validate(role, ssid, password);
  if (err) return err;
  Preferences prefs;
  if (!prefs.begin(ns_.c_str(), false)) return "cannot open NVS";
  bool ok = prefs.putUChar(KEY_ROLE, role == WifiRole::AccessPoint ? 1 : 0) &&
            prefs.putString(KEY_SSID, ssid) == ssid.length() &&
            (password.length() ? prefs.putString(KEY_PASS, password) == password.length()
                               : (prefs.remove(KEY_PASS), true));
  prefs.end();
  if (!ok) return "NVS write failed";
  stored_ = {role, ssid, password};
  present_ = true;
  return nullptr;
}

bool NvsSettings::clear() {
  Preferences prefs;
  if (!prefs.begin(ns_.c_str(), false)) return false;
  bool ok = prefs.clear();
  prefs.end();
  if (ok) {
    present_ = false;
    stored_ = StoredWifi();
  }
  return ok;
}

}  // namespace espcamweb

#endif  // WIFI_SETTINGS_ON
