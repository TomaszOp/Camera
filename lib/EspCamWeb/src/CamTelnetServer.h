// Serwer poleceń Telnet. Logicznie na równi z CamWebServer: zależy wyłącznie od EspCamera,
// polecenia (te same co na COM) wykonuje CamCommands.
// Core Arduino nie ma serwera telnet (jest tylko surowy WiFiServer), dlatego używa biblioteki
// ESPTelnet (lennarthennigs/ESP Telnet): jeden klient naraz, tryb liniowy.
// Telnet nie szyfruje - hasło i polecenia idą jawnym tekstem; używać tylko w zaufanej sieci.
#pragma once

#include <Arduino.h>
#include "CamCommands.h"
#include "EspCamWebConfig.h"

#if TELNET_SERVER_ON

#include <ESPTelnet.h>

namespace espcamweb {

class CamTelnetServer {
 public:
  explicit CamTelnetServer(EspCamera &camera) : commands_(camera) {}

  // Wywołać po uruchomieniu WiFi (STA lub AP). Działa tylko jedna instancja naraz.
  bool begin(const TelnetSettings &settings = TelnetSettings());
  void handle();  // wywoływać często w loop()
  bool isClientConnected() { return telnet_.isConnected(); }

 private:
  // ESPTelnet przyjmuje zwykłe wskaźniki do funkcji, stąd statyczne callbacki
  static void onConnect(String ip);
  static void onDisconnect(String ip);
  static void onConnectionAttempt(String ip);
  static void onInput(String line);
  void prompt();

  static CamTelnetServer *instance_;
  ESPTelnet telnet_;
  CamCommands commands_;
  TelnetSettings s_;
  bool authorized_ = false;
};

}  // namespace espcamweb

#endif  // TELNET_SERVER_ON
