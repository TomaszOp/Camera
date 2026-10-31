// Serwer poleceń na porcie szeregowym (COM). Logicznie na równi z CamWebServer:
// zależy wyłącznie od EspCamera, polecenia wykonuje CamCommands.
// Obsługa nieblokująca w loop() przez handle() - polecenia są krótkie, a jedno wspólne
// zadanie z Telnetem nie wymaga osobnego stosu ani synchronizacji z loop().
#pragma once

#include <Arduino.h>
#include "CamCommands.h"
#include "EspCamWebConfig.h"

#if COM_SERVER_ON

namespace espcamweb {

class CamComServer {
 public:
  // io: port szeregowy (domyślnie Serial; Serial.begin() woła program)
  explicit CamComServer(EspCamera &camera, Stream &io = Serial) : commands_(camera), io_(io) {}

  void begin(const ComServerSettings &settings = ComServerSettings());
  void handle();  // wywoływać często w loop()

 private:
  void prompt();

  CamCommands commands_;
  Stream &io_;
  ComServerSettings s_;
  String line_;
  bool lastWasCr_ = false;
};

}  // namespace espcamweb

#endif  // COM_SERVER_ON
