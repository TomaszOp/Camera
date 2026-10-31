// Interpreter poleceń tekstowych dla kamery - wspólny dla serwera COM i Telnet,
// dzięki czemu oba mają identyczne polecenia. Zależy wyłącznie od EspCamera.
// Polecenia po angielsku, odpowiedzi po polsku, wielkość liter bez znaczenia.
// Polecenia usług niedostępnych (wyłączonych w EspCamFeatures.h lub niedziałających) są
// ukryte w "help" i zwracają komunikat o braku usługi.
#pragma once

#include <Arduino.h>

namespace espcamweb {

class EspCamera;

class CamCommands {
 public:
  enum class Result { Ok, Quit };

  explicit CamCommands(EspCamera &camera) : camera_(camera) {}

  // Wykonuje jedną linię i wypisuje odpowiedź. Quit = klient chce się rozłączyć (quit/exit).
  Result execute(const String &line, Print &out);

 private:
  void help(Print &out);
  void info(Print &out);
  void status(Print &out);
  void wifi(Print &out);
  void led(const String &arg, Print &out);
  void servo(const String *args, int n, Print &out);
  void photo(Print &out);
  void sd(const String *args, int n, Print &out);
  void cam(const String *args, int n, Print &out);
  void restart(const String &arg, Print &out);

  EspCamera &camera_;
};

}  // namespace espcamweb
