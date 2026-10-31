#include "CamCommands.h"

#include <WiFi.h>

#include "EspCamera.h"

namespace espcamweb {

static const int MAX_ARGS = 4;
static const int SERVO_STEP = 10;  // domyślny krok dla servo left/right/up/down

static void unavailable(Print &out, const char *service) {
  out.printf("Usługa niedostępna: %s (wyłączona w config.h lub nie działa)\r\n", service);
}

static const char *onOff(bool v) { return v ? "wł." : "wył."; }

CamCommands::Result CamCommands::execute(const String &line, Print &out) {
  // Pomija śmieci przed poleceniem (np. bajty negocjacji telnetu w pierwszej linii)
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
  else out.printf("Nieznane polecenie: %s (wpisz help)\r\n", cmd.c_str());
  return Result::Ok;
}

void CamCommands::help(Print &out) {
  CameraFeatures f = camera_.features();
  out.print("Dostępne polecenia:\r\n"
            "  help                      ta lista\r\n"
            "  info                      wersja, czas pracy, pamięć, sieć, usługi\r\n"
            "  status                    skrót stanu wszystkich usług\r\n"
            "  wifi                      tryb, SSID, IP, RSSI\r\n"
            "  cam                       rozdzielczość, jakość, FPS\r\n"
            "  cam res qvga|vga|svga|xga zmiana rozdzielczości\r\n"
            "  cam quality N             jakość JPEG 4-63 (mniej = lepsza)\r\n"
            "  cam flip|mirror on|off    odwrócenie obrazu\r\n"
            "  photo                     zdjęcie");
  out.print(f.sd ? " na kartę SD\r\n" : " (rozmiar klatki; bez karty SD nie jest zapisywane)\r\n");
  if (f.led) {
    out.print("  led                       stan lampy\r\n"
              "  led on|off|toggle         włącz / wyłącz / przełącz lampę\r\n");
  }
  if (f.servos) {
    out.print("  servo                     pozycje i zakresy serw\r\n"
              "  servo pan N | tilt N      ustaw kąt poziomo / pionowo\r\n"
              "  servo N M                 ustaw oba kąty (pan tilt)\r\n"
              "  servo left|right|up|down [krok]  przesuń o krok (domyślnie 10)\r\n"
              "  servo center              pozycja środkowa\r\n");
  }
  if (f.sd) {
    out.print("  sd                        stan karty SD\r\n"
              "  sd list [N]               ostatnie N zdjęć (domyślnie 10)\r\n");
  }
  out.print("  restart yes               restart płytki\r\n"
            "  quit                      rozłącz (telnet)\r\n");
}

void CamCommands::info(Print &out) {
  CameraFeatures f = camera_.features();
  uint32_t s = millis() / 1000;
  out.printf("EspCamWeb %s, %s rev %d, %u MHz\r\n", ESPCAMWEB_VERSION, ESP.getChipModel(),
             ESP.getChipRevision(), (unsigned)ESP.getCpuFreqMHz());
  out.printf("Czas pracy: %ud %02u:%02u:%02u\r\n", (unsigned)(s / 86400), (unsigned)(s / 3600 % 24),
             (unsigned)(s / 60 % 60), (unsigned)(s % 60));
  out.printf("Pamięć: heap %u KB wolne, PSRAM %u KB wolne z %u KB\r\n", ESP.getFreeHeap() / 1024,
             ESP.getFreePsram() / 1024, ESP.getPsramSize() / 1024);
  wifi(out);
  out.printf("Usługi: LED %s, karta SD %s, serwa %s\r\n", f.led ? "tak" : "nie", f.sd ? "tak" : "nie",
             f.servos ? "tak" : "nie");
}

void CamCommands::status(Print &out) {
  CameraFeatures f = camera_.features();
  framesize_t fs = camera_.frameSize();
  out.printf("Kamera: %s, jakość %d, %.1f kl/s\r\n", EspCamera::frameSizeName(fs), camera_.quality(),
             camera_.fps());
  if (f.led) out.printf("Lampa: %s\r\n", onOff(camera_.ledOn()));
  else out.print("Lampa: niedostępna\r\n");
  if (f.servos) out.printf("Serwa: pan %d°, tilt %d°\r\n", camera_.pan(), camera_.tilt());
  else out.print("Serwa: niedostępne\r\n");
  if (f.sd) {
    out.printf("Karta SD: %u zdjęć, wolne %u MB\r\n", (unsigned)camera_.sdPhotoCount(),
               (unsigned)(camera_.sdFreeBytes() >> 20));
  } else {
    out.print("Karta SD: niedostępna\r\n");
  }
}

void CamCommands::wifi(Print &out) {
  if (WiFi.status() == WL_CONNECTED) {
    out.printf("WiFi: STA \"%s\", IP %s, RSSI %d dBm\r\n", WiFi.SSID().c_str(),
               WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
  } else if (WiFi.getMode() & WIFI_AP) {
    out.printf("WiFi: AP \"%s\", IP %s, klientów %u\r\n", WiFi.softAPSSID().c_str(),
               WiFi.softAPIP().toString().c_str(), (unsigned)WiFi.softAPgetStationNum());
  } else {
    out.print("WiFi: brak połączenia\r\n");
  }
}

void CamCommands::led(const String &arg, Print &out) {
  if (!camera_.hasLed()) return unavailable(out, "lampa LED");
  if (arg == "on") camera_.setLed(true);
  else if (arg == "off") camera_.setLed(false);
  else if (arg == "toggle") camera_.setLed(!camera_.ledOn());
  else if (arg.length()) return (void)out.print("Użycie: led [on|off|toggle]\r\n");
  out.printf("Lampa: %s\r\n", onOff(camera_.ledOn()));
}

void CamCommands::servo(const String *args, int n, Print &out) {
  if (!camera_.hasServos()) return unavailable(out, "serwa");
  const String &sub = args[0];
  bool ok = true;

  if (n == 0) {
    int pMin, pMax, tMin, tMax;
    camera_.servoLimits(pMin, pMax, tMin, tMax);
    out.printf("Serwa: pan %d° (zakres %d-%d), tilt %d° (zakres %d-%d)\r\n", camera_.pan(), pMin, pMax,
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
    out.print("Użycie: servo [pan N | tilt N | N M | left|right|up|down [krok] | center]\r\n");
    return;
  }
  out.printf("Serwa: pan %d°, tilt %d°\r\n", camera_.pan(), camera_.tilt());
}

void CamCommands::photo(Print &out) {
  if (camera_.hasSd()) {
    size_t bytes = 0;
    String path = camera_.savePhotoToSd(&bytes);
    if (path.length()) out.printf("Zapisano %s (%u KB)\r\n", path.c_str(), (unsigned)((bytes + 1023) / 1024));
    else out.print("Błąd zapisu zdjęcia na kartę SD\r\n");
    return;
  }
  camera_fb_t *fb = camera_.capture();
  if (!fb) return (void)out.print("Błąd pobrania klatki z kamery\r\n");
  out.printf("Zdjęcie %ux%u, %u KB (bez karty SD - nie zapisano; pobierz przez http://<IP>/capture)\r\n",
             (unsigned)fb->width, (unsigned)fb->height, (unsigned)((fb->len + 1023) / 1024));
  camera_.release(fb);
}

void CamCommands::sd(const String *args, int n, Print &out) {
  if (!camera_.hasSd()) return unavailable(out, "karta SD");
  if (n == 0) {
    String last = camera_.sdLastPhoto();
    out.printf("Karta SD: %u zdjęć, wolne %u MB%s%s\r\n", (unsigned)camera_.sdPhotoCount(),
               (unsigned)(camera_.sdFreeBytes() >> 20), last.length() ? ", ostatnie: " : "", last.c_str());
  } else if (args[0] == "list") {
    int count = n >= 2 ? args[1].toInt() : 10;
    if (count <= 0) count = 10;
    if (!camera_.sdListPhotos(out, count)) out.print("Brak zdjęć na karcie\r\n");
  } else {
    out.print("Użycie: sd [list [N]]\r\n");
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
          out.printf("Rozdzielczość %s niedostępna (maks. %s)\r\n", EspCamera::frameSizeName(s.fs),
                     EspCamera::frameSizeName(camera_.maxFrameSize()));
          return;
        }
      }
    }
    if (!found) return (void)out.print("Użycie: cam res qvga|vga|svga|xga\r\n");
  } else if (n >= 2 && args[0] == "quality" && isDigit(args[1][0])) {
    camera_.setQuality(args[1].toInt());
  } else if (n >= 2 && (args[0] == "flip" || args[0] == "mirror") && (args[1] == "on" || args[1] == "off")) {
    if (args[0] == "flip") camera_.setVFlip(args[1] == "on");
    else camera_.setHMirror(args[1] == "on");
  } else if (n > 0) {
    return (void)out.print("Użycie: cam [res X | quality N | flip on|off | mirror on|off]\r\n");
  }
  framesize_t fs = camera_.frameSize();
  out.printf("Kamera: %s %ux%u, jakość %d, %.1f kl/s, flip %s, lustro %s, PSRAM %s\r\n",
             EspCamera::frameSizeName(fs), resolution[fs].width, resolution[fs].height, camera_.quality(),
             camera_.fps(), onOff(camera_.vflip()), onOff(camera_.hmirror()), camera_.hasPsram() ? "tak" : "nie");
}

void CamCommands::restart(const String &arg, Print &out) {
  if (arg != "yes") return (void)out.print("Potwierdź: restart yes\r\n");
  out.print("Restart...\r\n");
  delay(200);
  ESP.restart();
}

}  // namespace espcamweb
