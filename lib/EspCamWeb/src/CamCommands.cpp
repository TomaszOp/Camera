/**
 * @file CamCommands.cpp
 * @brief CamCommands implementation: tokenizer and handlers of all text commands.
 */
#include "CamCommands.h"

#include <WiFi.h>

#include "EspCamera.h"

namespace espcamweb {

static const int MAX_ARGS = 6;     ///< Maximum number of words parsed from a line (command + 5 arguments).
static const int SERVO_STEP = 10;  ///< Default step for servo left/right/up/down, degrees.

/**
 * @brief Prints the reply for a command whose service is disabled or not working.
 * @param out     Output.
 * @param service Service name shown to the user.
 */
static void unavailable(Print &out, const char *service) {
  out.printf("Service unavailable: %s (disabled in EspCamFeatures.h or not working)\r\n", service);
}

/**
 * @brief Formats a boolean state for replies.
 * @param v State.
 * @return "on" or "off".
 */
static const char *onOff(bool v) { return v ? "on" : "off"; }

CamCommands::Result CamCommands::execute(const String &line, Print &out) {
  // Skip garbage before the command (e.g. Telnet negotiation bytes in the first line)
  unsigned start = 0;
  while (start < line.length() && !isAlpha(line[start])) start++;

  // Split into words; "quoted text" is one word. raw keeps the case (SSID, password),
  // args is lower-case for keyword comparisons.
  String raw[MAX_ARGS], args[MAX_ARGS];
  int n = 0;
  unsigned i = start;
  while (n < MAX_ARGS) {
    while (i < line.length() && line[i] == ' ') i++;
    if (i >= line.length()) break;
    String word;
    if (line[i] == '"') {
      int end = line.indexOf('"', i + 1);
      if (end < 0) end = line.length();
      word = line.substring(i + 1, end);
      i = end + 1;
    } else {
      int end = line.indexOf(' ', i);
      if (end < 0) end = line.length();
      word = line.substring(i, end);
      i = end;
    }
    raw[n] = word;
    word.toLowerCase();
    args[n++] = word;
  }
  if (!n) return Result::Ok;

  const String &cmd = args[0];
  if (cmd == "help" || cmd == "?") help(out);
  else if (cmd == "info") info(out);
  else if (cmd == "status") status(out);
  else if (cmd == "wifi") wifi(args + 1, raw + 1, n - 1, out);
  else if (cmd == "led") led(args[1], out);
  else if (cmd == "servo") servo(args + 1, n - 1, out);
  else if (cmd == "photo") photo(out);
  else if (cmd == "ota") ota(out);
  else if (cmd == "sd") sd(args + 1, raw + 1, n - 1, out);
  else if (cmd == "cam") cam(args + 1, n - 1, out);
  else if (cmd == "restart") restart(args[1], out);
  else if (cmd == "quit" || cmd == "exit") return Result::Quit;
  else out.printf("Unknown command: %s (type help)\r\n", cmd.c_str());
  return Result::Ok;
}

void CamCommands::help(Print &out) {
  CameraFeatures f = camera_.features();
  out.print("Available commands:\r\n"
            "  help                      this list\r\n"
            "  info                      version, uptime, memory, network, services\r\n"
            "  status                    summary of all services\r\n"
            "  wifi                      mode, SSID, IP, RSSI, saved settings\r\n"
            "  cam                       resolution, quality, FPS\r\n"
            "  cam res qvga|vga|svga|xga change resolution\r\n"
            "  cam quality N             JPEG quality 4-63 (lower = better)\r\n"
            "  cam flip|mirror on|off    flip / mirror the image\r\n"
            "  photo                     take a photo");
  out.print(f.sd ? " and save it to the SD card\r\n" : " (frame size only; not saved without an SD card)\r\n");
  if (f.led) {
    out.print("  led                       lamp state\r\n"
              "  led on|off|toggle         switch the lamp on / off / toggle\r\n");
  }
  if (f.servos) {
    out.print("  servo                     servo positions and ranges\r\n"
              "  servo pan N | tilt N      set the pan / tilt angle\r\n"
              "  servo N M                 set both angles (pan tilt)\r\n"
              "  servo left|right|up|down [step]  move by a step (default 10)\r\n"
              "  servo center              center position\r\n");
  }
  if (f.sd) {
    out.print("  sd                        SD card state\r\n"
              "  sd list [N]               last N photos (default 10)\r\n"
              "  sd delete <name>          delete a photo, e.g. sd delete IMG_00012.jpg\r\n");
  }
  if (f.ota) {
    out.print("  ota                       firmware update state (OTA)\r\n");
  }
  if (f.settings) {
    out.print("  wifi set sta <ssid> [password]  save a network to join (restart to apply)\r\n"
              "  wifi set ap <ssid> [password]   save an own access point (restart to apply)\r\n"
              "  wifi clear yes            erase saved WiFi settings (use the defaults)\r\n"
              "                            use \"quotes\" for an SSID or password with spaces\r\n");
  }
  out.print("  restart yes               restart the board\r\n"
            "  quit                      disconnect (Telnet)\r\n");
}

void CamCommands::info(Print &out) {
  CameraFeatures f = camera_.features();
  uint32_t s = millis() / 1000;
  out.printf("EspCamWeb %s, %s rev %d, %u MHz\r\n", ESPCAMWEB_VERSION, ESP.getChipModel(),
             ESP.getChipRevision(), (unsigned)ESP.getCpuFreqMHz());
  out.printf("Uptime: %ud %02u:%02u:%02u\r\n", (unsigned)(s / 86400), (unsigned)(s / 3600 % 24),
             (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  out.printf("Memory: heap %u KB free, PSRAM %u KB free of %u KB\r\n", ESP.getFreeHeap() / 1024,
             ESP.getFreePsram() / 1024, ESP.getPsramSize() / 1024);
  wifiState(out);
  out.printf("Services: LED %s, SD card %s, servos %s, WiFi settings %s, OTA %s\r\n", f.led ? "yes" : "no",
             f.sd ? "yes" : "no", f.servos ? "yes" : "no", f.settings ? "yes" : "no", f.ota ? "yes" : "no");
}

void CamCommands::status(Print &out) {
  CameraFeatures f = camera_.features();
  framesize_t fs = camera_.frameSize();
  out.printf("Camera: %s, quality %d, %.1f fps\r\n", EspCamera::frameSizeName(fs), camera_.quality(),
             camera_.fps());
  if (f.led) out.printf("Lamp: %s\r\n", onOff(camera_.ledOn()));
  else out.print("Lamp: unavailable\r\n");
  if (f.servos) out.printf("Servos: pan %d°, tilt %d°\r\n", camera_.pan(), camera_.tilt());
  else out.print("Servos: unavailable\r\n");
  if (f.sd) {
    out.printf("SD card: %u photos, %u MB free\r\n", (unsigned)camera_.sdPhotoCount(),
               (unsigned)(camera_.sdFreeBytes() >> 20));
  } else {
    out.print("SD card: unavailable\r\n");
  }
}

void CamCommands::wifiState(Print &out) {
  if (WiFi.status() == WL_CONNECTED) {
    out.printf("WiFi: STA \"%s\", IP %s, RSSI %d dBm\r\n", WiFi.SSID().c_str(),
               WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
  } else if (WiFi.getMode() & WIFI_AP) {
    out.printf("WiFi: AP \"%s\", IP %s, %u clients\r\n", WiFi.softAPSSID().c_str(),
               WiFi.softAPIP().toString().c_str(), (unsigned)WiFi.softAPgetStationNum());
  } else {
    out.print("WiFi: not connected\r\n");
  }
}

void CamCommands::wifi(const String *args, const String *raw, int n, Print &out) {
  if (n == 0) {
    wifiState(out);
    WifiSettingsInfo info;
    if (!camera_.wifiSettings(info)) return;
    out.printf("Started with: %s settings%s\r\n", info.start.fromSaved ? "saved" : "default",
               info.start.fallback ? ", fallback to the default access point" : "");
    if (info.saved) {
      // The password itself is never shown
      out.printf("Saved: %s \"%s\", password %s\r\n", info.role == WifiRole::AccessPoint ? "AP" : "STA",
                 info.ssid.c_str(), info.hasPassword ? "********" : "none (open network)");
    } else {
      out.print("Saved: none (using the defaults from code)\r\n");
    }
    return;
  }
  if (!camera_.hasWifiSettings()) return unavailable(out, "WiFi settings");

  if (args[0] == "set" && n >= 3 && (args[1] == "sta" || args[1] == "ap")) {
    WifiRole role = args[1] == "ap" ? WifiRole::AccessPoint : WifiRole::Station;
    const char *err = camera_.saveWifiSettings(role, raw[2], n >= 4 ? raw[3] : String());
    if (err) out.printf("Not saved: %s\r\n", err);
    else out.print("Settings saved. Restart the device to apply (restart yes).\r\n");
  } else if (args[0] == "clear") {
    if (n < 2 || args[1] != "yes") return (void)out.print("Confirm with: wifi clear yes\r\n");
    if (camera_.clearWifiSettings()) out.print("Settings cleared. Restart the device to apply (restart yes).\r\n");
    else out.print("Failed to clear the settings\r\n");
  } else {
    out.print("Usage: wifi [set sta|ap <ssid> [password] | clear yes]  (use \"quotes\" for spaces)\r\n");
  }
}

void CamCommands::led(const String &arg, Print &out) {
  if (!camera_.hasLed()) return unavailable(out, "LED lamp");
  if (arg == "on") camera_.setLed(true);
  else if (arg == "off") camera_.setLed(false);
  else if (arg == "toggle") camera_.setLed(!camera_.ledOn());
  else if (arg.length()) return (void)out.print("Usage: led [on|off|toggle]\r\n");
  out.printf("Lamp: %s\r\n", onOff(camera_.ledOn()));
}

void CamCommands::servo(const String *args, int n, Print &out) {
  if (!camera_.hasServos()) return unavailable(out, "servos");
  const String &sub = args[0];
  bool ok = true;

  if (n == 0) {
    int pMin, pMax, tMin, tMax;
    camera_.servoLimits(pMin, pMax, tMin, tMax);
    out.printf("Servos: pan %d° (range %d-%d), tilt %d° (range %d-%d)\r\n", camera_.pan(), pMin, pMax,
               camera_.tilt(), tMin, tMax);
    return;
  }
  if (sub == "center") {
    camera_.centerServos();
  } else if ((sub == "pan" || sub == "tilt") && n >= 2 && isDigit(args[1][0])) {
    if (sub == "pan") camera_.setPan(args[1].toInt());
    else camera_.setTilt(args[1].toInt());
  } else if (sub == "left" || sub == "right" || sub == "up" || sub == "down") {
    int step = n >= 2 ? args[1].toInt() : SERVO_STEP;
    if (step <= 0) step = SERVO_STEP;
    if (sub == "left") camera_.setPan(camera_.pan() - step);
    else if (sub == "right") camera_.setPan(camera_.pan() + step);
    else if (sub == "up") camera_.setTilt(camera_.tilt() + step);
    else camera_.setTilt(camera_.tilt() - step);
  } else if (n >= 2 && isDigit(sub[0]) && isDigit(args[1][0])) {
    camera_.setPan(sub.toInt());
    camera_.setTilt(args[1].toInt());
  } else {
    ok = false;
  }

  if (!ok) {
    out.print("Usage: servo [pan N | tilt N | N M | left|right|up|down [step] | center]\r\n");
    return;
  }
  out.printf("Servos: pan %d°, tilt %d°\r\n", camera_.pan(), camera_.tilt());
}

void CamCommands::ota(Print &out) {
  OtaInfo info;
  if (!camera_.otaInfo(info)) return unavailable(out, "OTA");
  out.printf("OTA: enabled, password %s%s\r\n", info.passwordSet ? "set" : "NOT set (anyone on the LAN can flash)",
             info.updating ? ", update in progress" : "");
  out.printf("Running partition: %s, firmware %u KB\r\n", info.running.c_str(), (unsigned)(info.appSize / 1024));
  if (info.next.length()) {
    out.printf("Next update goes to: %s (%u KB)\r\n", info.next.c_str(), (unsigned)(info.slotSize / 1024));
  } else {
    out.print("No OTA slot - use a partition table with two app slots (min_spiffs.csv)\r\n");
  }
  out.print("Upload: pio run -e esp32cam_ota -t upload, or the /update web page\r\n");
}

void CamCommands::photo(Print &out) {
  if (camera_.hasSd()) {
    size_t bytes = 0;
    String path = camera_.savePhotoToSd(&bytes);
    if (path.length()) out.printf("Saved %s (%u KB)\r\n", path.c_str(), (unsigned)((bytes + 1023) / 1024));
    else out.print("Failed to save the photo to the SD card\r\n");
    return;
  }
  camera_fb_t *fb = camera_.capture();
  if (!fb) return (void)out.print("Failed to capture a frame\r\n");
  out.printf("Photo %ux%u, %u KB (no SD card - not saved; download via http://<IP>/capture)\r\n",
             (unsigned)fb->width, (unsigned)fb->height, (unsigned)((fb->len + 1023) / 1024));
  camera_.release(fb);
}

void CamCommands::sd(const String *args, const String *raw, int n, Print &out) {
  if (!camera_.hasSd()) return unavailable(out, "SD card");
  if (n == 0) {
    String last = camera_.sdLastPhoto();
    out.printf("SD card: %u photos, %u MB free%s%s\r\n", (unsigned)camera_.sdPhotoCount(),
               (unsigned)(camera_.sdFreeBytes() >> 20), last.length() ? ", last: " : "", last.c_str());
  } else if (args[0] == "list") {
    int count = n >= 2 ? args[1].toInt() : 10;
    if (count <= 0) count = 10;
    if (!camera_.sdListPhotos(out, count)) out.print("No photos on the card\r\n");
  } else if (args[0] == "delete" && n >= 2) {
    const char *err = camera_.sdDeletePhoto(raw[1]);
    if (err) out.printf("Not deleted: %s\r\n", err);
    else out.printf("Deleted %s\r\n", raw[1].c_str());
  } else {
    out.print("Usage: sd [list [N] | delete <name>]\r\n");
  }
}

void CamCommands::cam(const String *args, int n, Print &out) {
  if (n >= 2 && args[0] == "res") {
    static const struct {
      const char *name;
      framesize_t fs;
    } sizes[] = {{"qvga", FRAMESIZE_QVGA}, {"vga", FRAMESIZE_VGA}, {"svga", FRAMESIZE_SVGA}, {"xga", FRAMESIZE_XGA}};
    bool found = false;
    for (const auto &s : sizes) {
      if (args[1] == s.name) {
        found = true;
        if (!camera_.setFrameSize(s.fs)) {
          out.printf("Resolution %s unavailable (max. %s)\r\n", EspCamera::frameSizeName(s.fs),
                     EspCamera::frameSizeName(camera_.maxFrameSize()));
          return;
        }
      }
    }
    if (!found) return (void)out.print("Usage: cam res qvga|vga|svga|xga\r\n");
  } else if (n >= 2 && args[0] == "quality" && isDigit(args[1][0])) {
    camera_.setQuality(args[1].toInt());
  } else if (n >= 2 && (args[0] == "flip" || args[0] == "mirror") && (args[1] == "on" || args[1] == "off")) {
    if (args[0] == "flip") camera_.setVFlip(args[1] == "on");
    else camera_.setHMirror(args[1] == "on");
  } else if (n > 0) {
    return (void)out.print("Usage: cam [res X | quality N | flip on|off | mirror on|off]\r\n");
  }
  framesize_t fs = camera_.frameSize();
  out.printf("Camera: %s %ux%u, quality %d, %.1f fps, flip %s, mirror %s, PSRAM %s\r\n",
             EspCamera::frameSizeName(fs), resolution[fs].width, resolution[fs].height, camera_.quality(),
             camera_.fps(), onOff(camera_.vflip()), onOff(camera_.hmirror()), camera_.hasPsram() ? "yes" : "no");
}

void CamCommands::restart(const String &arg, Print &out) {
  if (arg != "yes") return (void)out.print("Confirm with: restart yes\r\n");
  out.print("Restarting...\r\n");
  delay(200);
  ESP.restart();
}

}  // namespace espcamweb
