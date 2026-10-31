#include "CamWebServer.h"

#include <Arduino.h>
#include <WiFi.h>

#include "CamLed.h"
#include "CamWebPage.h"
#include "EspCamera.h"

namespace espcamweb {

#define ECW_PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" ECW_PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" ECW_PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static CamWebServer *self(httpd_req_t *req) { return static_cast<CamWebServer *>(req->user_ctx); }

bool CamWebServer::begin(const ServerSettings &settings) {
  settings_ = settings;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = settings_.httpPort;
  config.max_uri_handlers = 8;
  config.lru_purge_enable = true;

  const httpd_uri_t uris[] = {
      {"/", HTTP_GET, indexHandler, this},
      {"/status", HTTP_GET, statusHandler, this},
      {"/control", HTTP_GET, controlHandler, this},
      {"/capture", HTTP_GET, captureHandler, this},
  };
  if (httpd_start(&webHttpd_, &config) != ESP_OK) {
    Serial.printf("Nie udało się uruchomić serwera na porcie %u\n", settings_.httpPort);
    return false;
  }
  for (const httpd_uri_t &uri : uris) httpd_register_uri_handler(webHttpd_, &uri);

  // Osobna instancja: strumień blokuje swoje zadanie, więc nie może dzielić go ze stroną
  config.server_port = settings_.streamPort;
  config.ctrl_port += 1;
  const httpd_uri_t streamUri = {"/stream", HTTP_GET, streamHandler, this};
  if (httpd_start(&streamHttpd_, &config) != ESP_OK) {
    Serial.printf("Nie udało się uruchomić serwera strumienia na porcie %u\n", settings_.streamPort);
    end();
    return false;
  }
  httpd_register_uri_handler(streamHttpd_, &streamUri);
  return true;
}

void CamWebServer::end() {
  if (streamHttpd_) httpd_stop(streamHttpd_);
  if (webHttpd_) httpd_stop(webHttpd_);
  streamHttpd_ = webHttpd_ = nullptr;
}

esp_err_t CamWebServer::indexHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, INDEX_HTML, INDEX_HTML_LEN);
}

esp_err_t CamWebServer::captureHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  camera_fb_t *fb = cam.capture();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  cam.release(fb);
  return res;
}

esp_err_t CamWebServer::statusHandler(httpd_req_t *req) {
  CamWebServer *srv = self(req);
  EspCamera &cam = srv->camera_;
  framesize_t fs = cam.frameSize();
  bool ap = WiFi.status() != WL_CONNECTED;
  String ip = ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  bool ledAvailable = srv->led_ && srv->led_->isAvailable();

  char json[400];
  snprintf(json, sizeof(json),
           "{\"fps\":%.1f,\"framesize\":%d,\"maxFramesize\":%d,\"name\":\"%s\",\"width\":%u,\"height\":%u,"
           "\"quality\":%d,\"ledAvailable\":%d,\"led\":%d,\"vflip\":%d,\"hmirror\":%d,\"rssi\":%d,"
           "\"psram\":%d,\"clients\":%u,\"streamPort\":%u,\"mode\":\"%s\",\"ip\":\"%s\"}",
           cam.fps(), (int)fs, (int)cam.maxFrameSize(), EspCamera::frameSizeName(fs),
           resolution[fs].width, resolution[fs].height, cam.quality(), ledAvailable,
           ledAvailable && srv->led_->isOn(), cam.vflip(), cam.hmirror(), ap ? 0 : (int)WiFi.RSSI(),
           cam.hasPsram(), (unsigned)srv->streamClients_, srv->settings_.streamPort, ap ? "AP" : "STA",
           ip.c_str());

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t CamWebServer::controlHandler(httpd_req_t *req) {
  CamWebServer *srv = self(req);
  EspCamera &cam = srv->camera_;
  char query[64], var[16], val[8];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "var", var, sizeof(var)) != ESP_OK ||
      httpd_query_key_value(query, "val", val, sizeof(val)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "brak var/val");
    return ESP_FAIL;
  }

  int v = atoi(val);
  bool ok;
  if (!strcmp(var, "framesize")) {
    ok = cam.setFrameSize((framesize_t)v);
  } else if (!strcmp(var, "quality")) {
    ok = cam.setQuality(v);
  } else if (!strcmp(var, "vflip")) {
    ok = cam.setVFlip(v != 0);
  } else if (!strcmp(var, "hmirror")) {
    ok = cam.setHMirror(v != 0);
  } else if (!strcmp(var, "led") && srv->led_ && srv->led_->isAvailable()) {
    srv->led_->set(v != 0);
    ok = true;
  } else {
    ok = false;
  }

  if (!ok) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "niepoprawna wartość");
    return ESP_FAIL;
  }
  return httpd_resp_send(req, nullptr, 0);
}

esp_err_t CamWebServer::streamHandler(httpd_req_t *req) {
  CamWebServer *srv = self(req);
  EspCamera &cam = srv->camera_;
  char part[64];
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");

  Serial.printf("Strumień: klient połączony (%u)\n", (unsigned)++srv->streamClients_);

  while (true) {
    camera_fb_t *fb = cam.capture();
    if (!fb) {
      Serial.println("Błąd pobrania klatki");
      res = ESP_FAIL;
      break;
    }
    size_t hlen = snprintf(part, sizeof(part), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, part, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    cam.release(fb);
    if (res != ESP_OK) break;
  }

  Serial.printf("Strumień: klient rozłączony (%u)\n", (unsigned)--srv->streamClients_);
  return res;
}

}  // namespace espcamweb
