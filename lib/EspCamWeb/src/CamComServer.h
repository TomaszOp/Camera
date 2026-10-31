/**
 * @file CamComServer.h
 * @brief Command server on the serial port (COM). Compiled only when COM_SERVER_ON is 1.
 */
#pragma once

#include <Arduino.h>
#include "CamCommands.h"
#include "EspCamWebConfig.h"

#if COM_SERVER_ON

namespace espcamweb {

/**
 * @brief Reads command lines from a serial stream and executes them with CamCommands.
 *
 * A front-end on par with CamWebServer: depends only on EspCamera.
 * Non-blocking, serviced from loop() via handle() - commands are short, and sharing
 * the loop task with Telnet needs no extra stack and no synchronisation with loop().
 * Diagnostic logs may still be printed to the same port.
 */
class CamComServer {
 public:
  /**
   * @brief Binds the server to a camera and a stream.
   * @param camera Camera the commands operate on; must outlive the server.
   * @param io     Serial port (default Serial; the program calls Serial.begin()).
   */
  explicit CamComServer(EspCamera &camera, Stream &io = Serial) : commands_(camera), io_(io) {}

  /**
   * @brief Prints the greeting and the first prompt.
   * @param settings Echo and prompt.
   */
  void begin(const ComServerSettings &settings = ComServerSettings());

  /// @brief Reads available characters; executes a command on Enter. Call often from loop().
  void handle();

 private:
  /// @brief Prints the prompt (if configured).
  void prompt();

  CamCommands commands_;    ///< Shared command interpreter.
  Stream &io_;              ///< Serial port.
  ComServerSettings s_;     ///< Echo and prompt.
  String line_;             ///< Line being typed.
  bool lastWasCr_ = false;  ///< Previous character was CR (to treat CR LF as one Enter).
};

}  // namespace espcamweb

#endif  // COM_SERVER_ON
