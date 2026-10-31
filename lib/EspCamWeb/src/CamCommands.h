/**
 * @file CamCommands.h
 * @brief Text command interpreter for the camera, shared by the serial and Telnet servers.
 */
#pragma once

#include <Arduino.h>

namespace espcamweb {

class EspCamera;

/**
 * @brief Parses one text line and executes it on EspCamera.
 *
 * Shared by CamComServer and CamTelnetServer, so both offer identical commands.
 * Depends only on EspCamera. Keywords and replies are English; keywords are case-insensitive.
 * Commands of unavailable services (disabled in EspCamFeatures.h or not working) are hidden
 * in "help" and answer with a "service unavailable" message.
 */
class CamCommands {
 public:
  /// Outcome of a command line.
  enum class Result {
    Ok,   ///< Command handled (or empty line); keep the session.
    Quit  ///< Client asked to disconnect (quit/exit).
  };

  /**
   * @brief Binds the interpreter to a camera.
   * @param camera Camera the commands operate on; must outlive the interpreter.
   */
  explicit CamCommands(EspCamera &camera) : camera_(camera) {}

  /**
   * @brief Executes one command line and prints the reply.
   *
   * Leading non-letter characters are skipped (e.g. Telnet negotiation bytes in the first line).
   * Words are separated by spaces; "double quotes" group a word containing spaces (e.g. an SSID).
   * @param line Raw input line.
   * @param out  Where the reply is printed.
   * @return Result::Quit for quit/exit, otherwise Result::Ok.
   */
  Result execute(const String &line, Print &out);

 private:
  /// @brief "help" - lists the commands available right now. @param out Output.
  void help(Print &out);
  /// @brief "info" - version, uptime, memory, WiFi and the list of services. @param out Output.
  void info(Print &out);
  /// @brief "status" - one line per service: camera, lamp, servos, SD card. @param out Output.
  void status(Print &out);
  /// @brief Prints the current network state: mode, SSID, IP and RSSI (or AP clients). @param out Output.
  void wifiState(Print &out);
  /**
   * @brief "wifi [set sta|ap <ssid> [password] | clear yes]" - shows the network state and the saved
   *        settings (password masked), or changes/erases the saved settings.
   * @param args Lower-case arguments after "wifi".
   * @param raw  The same arguments in their original case (for SSID and password).
   * @param n    Number of arguments.
   * @param out  Output.
   */
  void wifi(const String *args, const String *raw, int n, Print &out);
  /**
   * @brief "led [on|off|toggle]" - switches the lamp or shows its state.
   * @param arg Sub-command, empty = show state.
   * @param out Output.
   */
  void led(const String &arg, Print &out);
  /**
   * @brief "servo [pan N | tilt N | N M | left|right|up|down [step] | center]" - moves the servos
   *        or shows their positions and ranges.
   * @param args Arguments after "servo".
   * @param n    Number of arguments.
   * @param out  Output.
   */
  void servo(const String *args, int n, Print &out);
  /// @brief "ota" - OTA state: enabled, password set, running/next partition, free slot size. @param out Output.
  void ota(Print &out);
  /// @brief "photo" - saves a photo to SD, or reports the frame size without a card. @param out Output.
  void photo(Print &out);
  /**
   * @brief "sd [list [N]]" - card state or the last N photos.
   * @param args Arguments after "sd".
   * @param n    Number of arguments.
   * @param out  Output.
   */
  void sd(const String *args, int n, Print &out);
  /**
   * @brief "cam [res X | quality N | flip on|off | mirror on|off]" - changes or shows image settings.
   * @param args Arguments after "cam".
   * @param n    Number of arguments.
   * @param out  Output.
   */
  void cam(const String *args, int n, Print &out);
  /**
   * @brief "restart yes" - restarts the board; without "yes" only asks for confirmation.
   * @param arg Confirmation word.
   * @param out Output.
   */
  void restart(const String &arg, Print &out);

  EspCamera &camera_;  ///< Camera the commands operate on.
};

}  // namespace espcamweb
