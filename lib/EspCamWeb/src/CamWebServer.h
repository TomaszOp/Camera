// Serwer WWW kamery na esp_http_server (ESP-IDF):
//  - httpPort:   strona, /control, /status, /capture
//  - streamPort: /stream (MJPEG) w osobnej instancji httpd, żeby strona nie blokowała strumienia
// Niezależny od reszty biblioteki: wymaga tylko obiektu EspCamera (i opcjonalnie CamLed).
#pragma once

#include <atomic>
#include "esp_http_server.h"
#include "EspCamWebConfig.h"

namespace espcamweb {

class EspCamera;
class CamLed;

class CamWebServer {
 public:
  explicit CamWebServer(EspCamera &camera, CamLed *led = nullptr) : camera_(camera), led_(led) {}
  ~CamWebServer() { end(); }

  bool begin(const ServerSettings &settings = ServerSettings());
  void end();

  uint8_t streamClients() const { return streamClients_; }

 private:
  static esp_err_t indexHandler(httpd_req_t *req);
  static esp_err_t statusHandler(httpd_req_t *req);
  static esp_err_t controlHandler(httpd_req_t *req);
  static esp_err_t captureHandler(httpd_req_t *req);
  static esp_err_t streamHandler(httpd_req_t *req);

  EspCamera &camera_;
  CamLed *led_;
  ServerSettings settings_;
  httpd_handle_t webHttpd_ = nullptr;
  httpd_handle_t streamHttpd_ = nullptr;
  std::atomic<uint8_t> streamClients_{0};
};

}  // namespace espcamweb
