#include "CamTelnetServer.h"

#if TELNET_SERVER_ON

namespace espcamweb {

CamTelnetServer *CamTelnetServer::instance_ = nullptr;

// Print kierujący wyjście CamCommands do klienta telnet
class TelnetPrint : public Print {
 public:
  explicit TelnetPrint(ESPTelnet &telnet) : telnet_(telnet) {}
  size_t write(uint8_t c) override { return telnet_.write(c); }
  size_t write(const uint8_t *buf, size_t len) override { return telnet_.write(buf, len); }

 private:
  ESPTelnet &telnet_;
};

static bool hasPassword(const char *password) { return password && *password; }

bool CamTelnetServer::begin(const TelnetSettings &settings) {
  s_ = settings;
  instance_ = this;
  telnet_.onConnect(onConnect);
  telnet_.onDisconnect(onDisconnect);
  telnet_.onConnectionAttempt(onConnectionAttempt);
  telnet_.onInputReceived(onInput);
  // false: bez sprawdzania WiFi w bibliotece - działa też w trybie AP
  if (!telnet_.begin(s_.port, false)) {
    Serial.printf("Telnet: nie udało się uruchomić na porcie %u\n", s_.port);
    return false;
  }
  Serial.printf("Telnet: port %u%s\n", s_.port, hasPassword(s_.password) ? " (z hasłem)" : "");
  return true;
}

void CamTelnetServer::handle() { telnet_.loop(); }

void CamTelnetServer::prompt() { telnet_.print(authorized_ ? "> " : "Hasło: "); }

void CamTelnetServer::onConnect(String ip) {
  CamTelnetServer *self = instance_;
  Serial.printf("Telnet: połączono z %s\n", ip.c_str());
  self->authorized_ = !hasPassword(self->s_.password);
  self->telnet_.print("\r\nEspCamWeb " ESPCAMWEB_VERSION " - ESP32-CAM\r\n");
  if (self->authorized_) self->telnet_.print("Wpisz help, aby zobaczyć polecenia, quit - rozłącz.\r\n");
  self->prompt();
}

void CamTelnetServer::onDisconnect(String ip) {
  Serial.printf("Telnet: rozłączono %s\n", ip.c_str());
  instance_->authorized_ = false;
}

void CamTelnetServer::onConnectionAttempt(String ip) {
  Serial.printf("Telnet: odrzucono %s (obsługiwany jest jeden klient naraz)\n", ip.c_str());
}

void CamTelnetServer::onInput(String line) {
  CamTelnetServer *self = instance_;
  if (!self->authorized_) {
    line.trim();
    if (line == self->s_.password) {
      self->authorized_ = true;
      self->telnet_.print("Zalogowano. Wpisz help, aby zobaczyć polecenia.\r\n");
    } else {
      self->telnet_.print("Złe hasło.\r\n");
      Serial.println("Telnet: złe hasło");
      self->telnet_.disconnectClient();
      return;
    }
  } else {
    TelnetPrint out(self->telnet_);
    if (self->commands_.execute(line, out) == CamCommands::Result::Quit) {
      self->telnet_.print("Do widzenia.\r\n");
      self->telnet_.disconnectClient();
      return;
    }
  }
  self->prompt();
}

}  // namespace espcamweb

#endif  // TELNET_SERVER_ON
