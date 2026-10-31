/**
 * @file CamTelnetServer.h
 * @brief Telnet command server. Compiled only when TELNET_SERVER_ON is 1.
 *
 * The Arduino core has no Telnet server (only a raw WiFiServer), so this uses the ESPTelnet
 * library (lennarthennigs/ESP Telnet): one client at a time, line mode.
 * Telnet is not encrypted - password and commands travel as plain text; use only on trusted networks.
 */
#pragma once

#include <Arduino.h>
#include "CamCommands.h"
#include "EspCamWebConfig.h"

#if TELNET_SERVER_ON

#include <ESPTelnet.h>

namespace espcamweb {

/**
 * @brief Accepts a Telnet client and executes its lines with CamCommands (same commands as COM).
 *
 * A front-end on par with CamWebServer: depends only on EspCamera.
 * Serviced from loop() via handle(). Optional password, banner on connect, quit/exit to disconnect.
 */
class CamTelnetServer {
 public:
  /**
   * @brief Binds the server to a camera.
   * @param camera Camera the commands operate on; must outlive the server.
   */
  explicit CamTelnetServer(EspCamera &camera) : commands_(camera) {}

  /**
   * @brief Starts listening. Call after WiFi is up (station or AP). Only one instance may run at a time.
   * @param settings Port and optional password.
   * @return false if the server could not be started.
   */
  bool begin(const TelnetSettings &settings = TelnetSettings());

  /// @brief Accepts connections and processes input. Call often from loop().
  void handle();

  /// @return true if a client is connected.
  bool isClientConnected() { return telnet_.isConnected(); }

 private:
  // ESPTelnet accepts plain function pointers only, hence static callbacks using instance_.

  /// @brief Client connected: resets authorisation, prints the banner and prompt. @param ip Client IP.
  static void onConnect(String ip);
  /// @brief Client disconnected: logs it and clears authorisation. @param ip Client IP.
  static void onDisconnect(String ip);
  /// @brief Second client rejected while one is connected: logs it. @param ip Rejected IP.
  static void onConnectionAttempt(String ip);
  /**
   * @brief Full line received: checks the password first, then executes commands.
   * @param line Line without the line ending.
   */
  static void onInput(String line);
  /// @brief Prints "> " when logged in, otherwise the password prompt.
  void prompt();

  static CamTelnetServer *instance_;  ///< Running instance for the static callbacks.
  ESPTelnet telnet_;                  ///< Telnet server from the ESPTelnet library.
  CamCommands commands_;              ///< Shared command interpreter.
  TelnetSettings s_;                  ///< Port and password.
  bool authorized_ = false;           ///< Client passed the password check (or none is set).
};

}  // namespace espcamweb

#endif  // TELNET_SERVER_ON
