/**
 * @file CamCommands.cpp
 * @brief CamCommands implementation: tokenizer and handlers of all text commands.
 */
#include "CamCommands.h"

#include <WiFi.h>

#include "EspCamera.h"

namespace espcamweb {

static const int MAX_ARGS = 4;     ///< Maximum number of words parsed from a line (command + 3 arguments).
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

  String args[MAX_ARGS];
  int n = 0;
  String rest = line.substring(start);
  rest.trim();
  rest.toLowerCase();
  while (rest.length() && n < MAX_ARGS) {
    int sp = rest.indexOf(' ');
    args[n++] = sp < 0 ? rest : rest.substring(0, sp);
    rest = sp < 0 ? String() : rest.substring(sp + 1);
    rest.trim();
  }
  if (!n) return Result::Ok;

  const String &cmd = args[0];
  if (cmd == "help" || cmd == "?") help(out);
  else if (cmd == "info") info(out);
  else if (cmd == "status") status(out);
  else if (cmd == "wifi") wifi(out);
  else if (cmd == "led") led(args[1], out);
  else if (cmd == "servo") servo(args + 1, n - 1, out);
  else if (cmd == "photo") photo(out);
  else if (cmd == "sd") sd(args + 1, n - 1, out);
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
            "  wifi                      mode, SSID, IP, RSSI\r\n"
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
              "  sd list [N]               last N photos (default 10)\r\n");
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
  wifi(out);
  out.printf("Services: LED %s, SD card %s, servos %s\r\n", f.led ? "yes" : "no", f.sd ? "yes" : "no",
             f.servos ? "yes" : "no");
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

void CamCommands::wifi(Print &out) {
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

void CamCommands::sd(const String *args, int n, Print &out) {
  if (!camera_.hasSd()) return unavailable(out, "SD card");
  if (n == 0) {
    String last = camera_.sdLastPhoto();
    out.printf("SD card: %u photos, %u MB free%s%s\r\n", (unsigned)camera_.sdPhotoCount(),
               (unsigned)(camera_.sdFreeBytes() >> 20), last.length() ? ", last: " : "", last.c_str());
  } else if (args[0] == "list") {
    int count = n >= 2 ? args[1].toInt() : 10;
    if (count <= 0) count = 10;
    if (!camera_.sdListPhotos(out, count)) out.print("No photos on the card\r\n");
  } else {
    out.print("Usage: sd [list [N]]\r\n");
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
