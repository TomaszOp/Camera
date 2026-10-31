/**
 * @file CamWifi.h
 * @brief WiFi station connection, mDNS and fallback access point.
 */
#pragma once

#include <IPAddress.h>
#include "EspCamWebConfig.h"

namespace espcamweb {

/**
 * @brief Brings up the network for the camera.
 *
 * Connects as a station with WiFi power saving disabled (it heavily reduces FPS),
 * falls back to an access point when the connection fails and announces the HTTP service over mDNS.
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

  /// @return true in station mode with an active connection.
  bool isConnected() const;

  /// @return true if the fallback access point is running.
  bool isAp() const { return ap_; }

  /// @return The board's IP address: station IP, or access point IP in AP mode.
  IPAddress ip() const;

  /// @return Signal strength in dBm, 0 in access point mode or when disconnected.
  int rssi() const;

 private:
  bool ap_ = false;  ///< true when running as access point.
};

}  // namespace espcamweb
