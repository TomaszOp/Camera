/**
 * @file CamWebServer.cpp
 * @brief CamWebServer implementation: httpd set-up and URI handlers.
 */
#include "CamWebServer.h"

#include <Arduino.h>
#include <WiFi.h>
#if OTA_ON
#include <mbedtls/base64.h>
#endif

#include "CamWebPage.h"
#include "EspCamera.h"
#if SD_ON
#include "CamSdCard.h"
#endif

namespace espcamweb {

/// Multipart boundary separating JPEG frames in the MJPEG stream.
#define ECW_PART_BOUNDARY "123456789000000000000987654321"
/// Content-Type of the MJPEG stream response.
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" ECW_PART_BOUNDARY;
/// Boundary line written before every frame.
static const char *STREAM_BOUNDARY = "\r\n--" ECW_PART_BOUNDARY "\r\n";
/// Per-frame part header; %u = JPEG length.
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/**
 * @brief Recovers the server object stored in the request's user context.
 * @param req Request.
 * @return The CamWebServer that registered the handler.
 */
static CamWebServer *self(httpd_req_t *req) { return static_cast<CamWebServer *>(req->user_ctx); }

bool CamWebServer::begin(const ServerSettings &settings) {
  settings_ = settings;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = settings_.httpPort;
  config.max_uri_handlers = 16;
  config.lru_purge_enable = true;

  const httpd_uri_t uris[] = {
      {"/", HTTP_GET, indexHandler, this},
      {"/status", HTTP_GET, statusHandler, this},
      {"/control", HTTP_GET, controlHandler, this},
      {"/capture", HTTP_GET, captureHandler, this},
      {"/save", HTTP_GET, saveHandler, this},
#if WIFI_SETTINGS_ON
      {"/wifi", HTTP_GET, wifiPageHandler, this},
      {"/wifi/settings", HTTP_GET, wifiSettingsHandler, this},
      {"/wifi/save", HTTP_POST, wifiSaveHandler, this},
      {"/wifi/clear", HTTP_POST, wifiClearHandler, this},
#endif
#if SD_ON
      {"/photos", HTTP_GET, photosPageHandler, this},
      {"/photos/list", HTTP_GET, photosListHandler, this},
      {"/photo", HTTP_GET, photoHandler, this},
      {"/photo/delete", HTTP_POST, photoDeleteHandler, this},
#endif
#if OTA_ON
      {"/update", HTTP_GET, updatePageHandler, this},
      {"/update", HTTP_POST, updateUploadHandler, this},
#endif
  };
  if (httpd_start(&webHttpd_, &config) != ESP_OK) {
    Serial.printf("Failed to start the web server on port %u\n", settings_.httpPort);
    return false;
  }
  for (const httpd_uri_t &uri : uris) httpd_register_uri_handler(webHttpd_, &uri);

  // Separate instance for /stream; each stream runs in its own task (see streamHandler)
  config.server_port = settings_.streamPort;
  config.ctrl_port += 1;
  config.max_uri_handlers = 2;
  config.send_wait_timeout = 2;  // drop a client that stopped reading within 2 s
  config.recv_wait_timeout = 2;
  const httpd_uri_t streamUri = {"/stream", HTTP_GET, streamHandler, this};
  if (httpd_start(&streamHttpd_, &config) != ESP_OK) {
    Serial.printf("Failed to start the stream server on port %u\n", settings_.streamPort);
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

esp_err_t CamWebServer::saveHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  if (!cam.hasSd()) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no SD card\"}");
  }
  String path = cam.savePhotoToSd();
  if (!path.length()) {
    httpd_resp_set_status(req, HTTPD_500);
    return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"write failed\"}");
  }
  char json[96];
  snprintf(json, sizeof(json), "{\"ok\":true,\"file\":\"%s\"}", path.c_str());
  return httpd_resp_sendstr(req, json);
}

esp_err_t CamWebServer::statusHandler(httpd_req_t *req) {
  CamWebServer *srv = self(req);
  EspCamera &cam = srv->camera_;
  framesize_t fs = cam.frameSize();
  bool ap = WiFi.status() != WL_CONNECTED;
  String ip = ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

  CameraFeatures f = cam.features();
  String last = cam.sdLastPhoto();

  char json[640];
  snprintf(json, sizeof(json),
           "{\"features\":{\"led\":%d,\"sd\":%d,\"servos\":%d,\"settings\":%d,\"ota\":%d},"
           "\"sdPhotos\":%u,\"sdFreeMB\":%u,\"sdLast\":\"%s\",\"pan\":%d,\"tilt\":%d,"
           "\"fps\":%.1f,\"framesize\":%d,\"maxFramesize\":%d,\"name\":\"%s\",\"width\":%u,\"height\":%u,"
           "\"quality\":%d,\"led\":%d,\"vflip\":%d,\"hmirror\":%d,\"rssi\":%d,"
           "\"psram\":%d,\"clients\":%u,\"streamPort\":%u,\"mode\":\"%s\",\"ip\":\"%s\"}",
           f.led, f.sd, f.servos, f.settings, f.ota, (unsigned)cam.sdPhotoCount(), (unsigned)(cam.sdFreeBytes() >> 20),
           last.c_str(), cam.pan(), cam.tilt(), cam.fps(), (int)fs, (int)cam.maxFrameSize(), EspCamera::frameSizeName(fs),
           resolution[fs].width, resolution[fs].height, cam.quality(), cam.ledOn(), cam.vflip(), cam.hmirror(), ap ? 0 : (int)WiFi.RSSI(),
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
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing var/val");
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
  } else if (!strcmp(var, "led")) {
    ok = cam.setLed(v != 0);
  } else if (!strcmp(var, "pan")) {
    ok = cam.setPan(v);
  } else if (!strcmp(var, "tilt")) {
    ok = cam.setTilt(v);
  } else if (!strcmp(var, "center")) {
    ok = cam.centerServos();
  } else {
    ok = false;
  }

  if (!ok) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid value");
    return ESP_FAIL;
  }
  return httpd_resp_send(req, nullptr, 0);
}

uint32_t CamWebServer::claimStreamSlot() {
  uint8_t limit = settings_.maxStreamClients;
  if (limit < 1) limit = 1;
  if (limit > MAX_STREAM_SLOTS) limit = MAX_STREAM_SLOTS;

  portENTER_CRITICAL(&slotsLock_);
  uint32_t id = ++nextStreamId_;
  if (id == 0) id = ++nextStreamId_;  // 0 means "free"
  int freeSlot = -1, oldest = -1;
  for (int i = 0; i < limit; i++) {
    if (!streamSlots_[i]) {
      if (freeSlot < 0) freeSlot = i;
    } else if (oldest < 0 || streamSlots_[i] < streamSlots_[oldest]) {
      oldest = i;
    }
  }
  if (freeSlot < 0) {
    // All slots busy: ask the oldest stream to stop and take its slot over
    kickId_ = streamSlots_[oldest];
    freeSlot = oldest;
  }
  streamSlots_[freeSlot] = id;
  portEXIT_CRITICAL(&slotsLock_);
  return id;
}

void CamWebServer::releaseStreamSlot(uint32_t id) {
  portENTER_CRITICAL(&slotsLock_);
  for (uint8_t i = 0; i < MAX_STREAM_SLOTS; i++) {
    if (streamSlots_[i] == id) streamSlots_[i] = 0;
  }
  portEXIT_CRITICAL(&slotsLock_);
}

esp_err_t CamWebServer::streamHandler(httpd_req_t *req) {
  // Serve the stream in its own task so the httpd task is never blocked by a running stream
  httpd_req_t *copy = nullptr;
  esp_err_t err = httpd_req_async_handler_begin(req, &copy);
  if (err != ESP_OK) return err;
  if (xTaskCreate(streamTask, "cam_stream", 4096, copy, tskIDLE_PRIORITY + 5, nullptr) != pdPASS) {
    Serial.println("Stream: cannot start a task");
    httpd_resp_send_500(copy);
    httpd_req_async_handler_complete(copy);
    return ESP_FAIL;
  }
  return ESP_OK;
}

void CamWebServer::streamTask(void *arg) {
  httpd_req_t *req = static_cast<httpd_req_t *>(arg);
  CamWebServer *srv = self(req);
  EspCamera &cam = srv->camera_;
  uint32_t id = srv->claimStreamSlot();
  char part[64];

  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  Serial.printf("Stream: client connected (%u)\n", (unsigned)++srv->streamClients_);

  while (res == ESP_OK) {
    if (cam.otaInProgress() || srv->streamEvicted(id)) break;  // OTA update, or a newer client took the slot
    camera_fb_t *fb = cam.capture();
    if (!fb) {
      Serial.println("Frame capture failed");
      break;
    }
    size_t hlen = snprintf(part, sizeof(part), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, part, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    cam.release(fb);
  }

  srv->releaseStreamSlot(id);
  Serial.printf("Stream: client disconnected (%u)\n", (unsigned)--srv->streamClients_);
  // Close the socket (an evicted or broken stream must not linger) and finish the async request
  httpd_sess_trigger_close(srv->streamHttpd_, httpd_req_to_sockfd(req));
  httpd_req_async_handler_complete(req);
  vTaskDelete(nullptr);
}

#if WIFI_SETTINGS_ON || OTA_ON || SD_ON
/**
 * @brief Escapes a string for use inside a JSON string literal (quotes, backslash, control characters).
 * @param in Raw text (e.g. an SSID, which may contain any characters).
 * @return Escaped text without the surrounding quotes.
 */
static String jsonEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (unsigned i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      char buf[8];
      snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(uint8_t)c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out;
}

#if WIFI_SETTINGS_ON || SD_ON
/**
 * @brief Decodes an application/x-www-form-urlencoded value ("+" = space, %XX = byte).
 * @param in Encoded value.
 * @return Decoded value.
 */
static String urlDecode(const char *in) {
  String out;
  for (const char *p = in; *p; p++) {
    if (*p == '+') {
      out += ' ';
    } else if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
      char hex[3] = {p[1], p[2], 0};
      out += (char)strtol(hex, nullptr, 16);
      p += 2;
    } else {
      out += *p;
    }
  }
  return out;
}
#endif

/**
 * @brief Sends a small JSON reply {"ok":..,"message":".."} with an HTTP status.
 * @param req     Request.
 * @param status  HTTP status line, e.g. "200 OK".
 * @param ok      Value of the "ok" field.
 * @param message Message for the user (escaped here).
 * @return Result of httpd_resp_sendstr().
 */
static esp_err_t sendResult(httpd_req_t *req, const char *status, bool ok, const String &message) {
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  String json = String("{\"ok\":") + (ok ? "true" : "false") + ",\"message\":\"" + jsonEscape(message) + "\"}";
  return httpd_resp_sendstr(req, json.c_str());
}
#endif

#if WIFI_SETTINGS_ON
esp_err_t CamWebServer::wifiPageHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, WIFI_HTML, WIFI_HTML_LEN);
}

esp_err_t CamWebServer::wifiSettingsHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  WifiSettingsInfo info;
  cam.wifiSettings(info);

  bool sta = WiFi.status() == WL_CONNECTED;
  String curSsid = sta ? WiFi.SSID() : WiFi.softAPSSID();
  String curIp = sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString();

  // The password is never sent - only whether one is saved
  String json = "{\"current\":{\"mode\":\"";
  json += sta ? "STA" : "AP";
  json += "\",\"ssid\":\"" + jsonEscape(curSsid) + "\",\"ip\":\"" + curIp + "\",\"rssi\":";
  json += sta ? (int)WiFi.RSSI() : 0;
  json += "},\"source\":\"";
  json += info.start.fromSaved ? "saved" : "default";
  json += "\",\"fallback\":";
  json += info.start.fallback ? "true" : "false";
  json += ",\"saved\":{\"present\":";
  json += info.saved ? "true" : "false";
  if (info.saved) {
    json += ",\"mode\":\"";
    json += info.role == WifiRole::AccessPoint ? "AP" : "STA";
    json += "\",\"ssid\":\"" + jsonEscape(info.ssid) + "\",\"hasPassword\":";
    json += info.hasPassword ? "true" : "false";
  }
  json += "}}";

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_sendstr(req, json.c_str());
}

esp_err_t CamWebServer::wifiSaveHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  char body[256];
  if (req->content_len >= sizeof(body)) return sendResult(req, "400 Bad Request", false, "request too long");
  int len = 0;
  while (len < (int)req->content_len) {
    int r = httpd_req_recv(req, body + len, req->content_len - len);
    if (r <= 0) return sendResult(req, "400 Bad Request", false, "cannot read the request");
    len += r;
  }
  body[len] = 0;

  char mode[8] = "", ssid[100] = "", pass[200] = "";
  httpd_query_key_value(body, "mode", mode, sizeof(mode));
  if (httpd_query_key_value(body, "ssid", ssid, sizeof(ssid)) != ESP_OK) {
    return sendResult(req, "400 Bad Request", false, "SSID is missing");
  }
  httpd_query_key_value(body, "password", pass, sizeof(pass));  // missing = open network
  if (strcmp(mode, "sta") && strcmp(mode, "ap")) return sendResult(req, "400 Bad Request", false, "mode must be sta or ap");

  WifiRole role = strcmp(mode, "ap") == 0 ? WifiRole::AccessPoint : WifiRole::Station;
  const char *err = cam.saveWifiSettings(role, urlDecode(ssid), urlDecode(pass));
  if (err) return sendResult(req, "400 Bad Request", false, err);
  return sendResult(req, "200 OK", true, "Settings saved. Restart the device to apply.");
}

esp_err_t CamWebServer::wifiClearHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  if (!cam.clearWifiSettings()) return sendResult(req, "500 Internal Server Error", false, "failed to clear the settings");
  return sendResult(req, "200 OK", true, "Settings cleared. Restart the device to apply.");
}
#endif

#if OTA_ON
/**
 * @brief Checks HTTP Basic Auth against the OTA password (any user name is accepted).
 *
 * Sends 401 with a WWW-Authenticate challenge when the check fails, so the browser asks for
 * the password and then repeats it for the following requests (page and upload).
 * @param req Request.
 * @param cam Camera (owns the OTA password check).
 * @return true if authorised (or no password is set); false after sending 401.
 */
static bool otaAuthorized(httpd_req_t *req, EspCamera &cam) {
  OtaInfo info;
  cam.otaInfo(info);
  if (!info.passwordSet) return true;

  char header[160];
  if (httpd_req_get_hdr_value_str(req, "Authorization", header, sizeof(header)) == ESP_OK &&
      strncmp(header, "Basic ", 6) == 0) {
    unsigned char decoded[120];
    size_t len = 0;
    const unsigned char *b64 = (const unsigned char *)header + 6;
    if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &len, b64, strlen(header + 6)) == 0) {
      decoded[len] = 0;
      const char *colon = strchr((const char *)decoded, ':');
      if (colon && cam.otaAuthorize(String(colon + 1))) return true;
    }
  }
  httpd_resp_set_status(req, "401 Unauthorized");
  httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"ESP32-CAM firmware update\"");
  httpd_resp_sendstr(req, "Authentication required");
  return false;
}

esp_err_t CamWebServer::updatePageHandler(httpd_req_t *req) {
  if (!otaAuthorized(req, self(req)->camera_)) return ESP_OK;
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, UPDATE_HTML, UPDATE_HTML_LEN);
}

esp_err_t CamWebServer::updateUploadHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  if (!otaAuthorized(req, cam)) return ESP_OK;
  if (req->content_len == 0) return sendResult(req, "400 Bad Request", false, "empty file");

  const char *err = cam.otaBegin(req->content_len);
  if (err) return sendResult(req, "400 Bad Request", false, String("cannot start: ") + err);

  // Stream the body in chunks straight into the OTA partition - the file is never buffered whole
  const size_t CHUNK = 4096;
  uint8_t *buf = (uint8_t *)malloc(CHUNK);
  size_t left = req->content_len;
  bool ok = buf != nullptr;
  while (ok && left > 0) {
    int r = httpd_req_recv(req, (char *)buf, left < CHUNK ? left : CHUNK);
    if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;  // slow client - wait again
    if (r <= 0 || !cam.otaWrite(buf, r)) {
      ok = false;
      break;
    }
    left -= r;
  }
  free(buf);

  err = cam.otaEnd(ok);
  if (err) return sendResult(req, "500 Internal Server Error", false, String("update failed: ") + err);
  sendResult(req, "200 OK", true, "Update successful. The device is restarting...");
  delay(500);  // let the reply reach the browser
  ESP.restart();
  return ESP_OK;
}
#endif

#if SD_ON
/// Photos per page in /photos and /photos/list.
static const uint32_t PHOTOS_PER_PAGE = 100;

/**
 * @brief Reads one query parameter, URL-decoded.
 * @param req  Request.
 * @param key  Parameter name.
 * @param[out] value Decoded value.
 * @return true if the parameter is present.
 */
static bool queryParam(httpd_req_t *req, const char *key, String &value) {
  char query[200], raw[150];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) return false;
  if (httpd_query_key_value(query, key, raw, sizeof(raw)) != ESP_OK) return false;
  value = urlDecode(raw);
  return true;
}

esp_err_t CamWebServer::photosPageHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, PHOTOS_HTML, PHOTOS_HTML_LEN);
}

esp_err_t CamWebServer::photosListHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  if (!cam.hasSd()) return sendResult(req, "503 Service Unavailable", false, "no SD card");
  String p;
  long page = queryParam(req, "page", p) ? p.toInt() : 1;
  if (page < 1) page = 1;

  // Send the JSON in chunks while listing - only one page (max. 100 entries) is ever in memory
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_sendstr_chunk(req, "{\"items\":[");
  bool first = true;
  uint32_t total = cam.sdListPhotoPage(page, PHOTOS_PER_PAGE, [&](const String &name, uint32_t size) {
    String item = first ? "{" : ",{";
    first = false;
    item += "\"name\":\"" + jsonEscape(name) + "\",\"size\":" + String(size) + "}";
    httpd_resp_sendstr_chunk(req, item.c_str());
  });
  uint32_t pages = (total + PHOTOS_PER_PAGE - 1) / PHOTOS_PER_PAGE;
  char tail[96];
  snprintf(tail, sizeof(tail), "],\"total\":%u,\"page\":%ld,\"pages\":%u,\"perPage\":%u}", (unsigned)total,
           page, (unsigned)pages, (unsigned)PHOTOS_PER_PAGE);
  httpd_resp_sendstr_chunk(req, tail);
  return httpd_resp_sendstr_chunk(req, nullptr);
}

esp_err_t CamWebServer::photoHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  if (!cam.hasSd()) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    return httpd_resp_sendstr(req, "No SD card");
  }
  String name;
  if (!queryParam(req, "name", name) || !CamSdCard::isValidName(name)) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file name");
    return ESP_FAIL;
  }
  bool headerSent = false;
  // Stream from the card in 4 KB chunks; the file is never loaded whole into RAM
  bool ok = cam.sdReadPhoto(name, [&](const uint8_t *data, size_t len) {
    if (!headerSent) {
      httpd_resp_set_type(req, "image/jpeg");
      httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
      headerSent = true;
    }
    return httpd_resp_send_chunk(req, (const char *)data, len) == ESP_OK;
  });
  if (!headerSent) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "photo not found");
    return ESP_FAIL;
  }
  if (!ok) return ESP_FAIL;  // connection dropped mid-file
  return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t CamWebServer::photoDeleteHandler(httpd_req_t *req) {
  EspCamera &cam = self(req)->camera_;
  if (!cam.hasSd()) return sendResult(req, "503 Service Unavailable", false, "no SD card");
  String name;
  if (!queryParam(req, "name", name)) return sendResult(req, "400 Bad Request", false, "missing file name");
  const char *err = cam.sdDeletePhoto(name);
  if (err) return sendResult(req, "400 Bad Request", false, err);
  return sendResult(req, "200 OK", true, "Deleted " + name);
}
#endif

}  // namespace espcamweb
