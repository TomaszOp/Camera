/**
 * @file CamOta.cpp
 * @brief CamOta implementation: ArduinoOTA set-up, web upload through the Update library, logging.
 */
#include "CamOta.h"

#if OTA_ON

#include <ArduinoOTA.h>
#include <Update.h>

namespace espcamweb {

void CamOta::begin(const OtaSettings &s) {
  password_ = s.password ? s.password : "";
  ArduinoOTA.setPort(s.port);
  if (s.hostname) ArduinoOTA.setHostname(s.hostname);
  if (hasPassword()) ArduinoOTA.setPassword(s.password);

  ArduinoOTA.onStart([this]() {
    started();
    Serial.printf("OTA: receiving %s\n", ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static unsigned lastPct = 101;
    unsigned pct = total ? done * 100 / total : 0;
    if (pct / 10 != lastPct / 10) {  // log every 10 %
      Serial.printf("OTA: %u%%\n", pct);
      lastPct = pct;
    }
  });
  ArduinoOTA.onEnd([this]() {
    updating_ = false;
    Serial.println("OTA: done, restarting");
  });
  ArduinoOTA.onError([this](ota_error_t err) {
    updating_ = false;
    static const char *const names[] = {"auth failed", "begin failed", "connect failed", "receive failed",
                                        "end failed"};
    Serial.printf("OTA: error %u (%s)\n", (unsigned)err, err <= OTA_END_ERROR ? names[err] : "unknown");
  });

  ArduinoOTA.begin();
  started_ = true;
  // The password itself is never printed
  Serial.printf("OTA: port %u%s\n", s.port, hasPassword() ? " (password protected)" : " - WARNING: no password");
}

void CamOta::handle() {
  if (started_) ArduinoOTA.handle();
}

void CamOta::started() {
  updating_ = true;
  if (startHook_) startHook_();
}

const char *CamOta::webBegin(size_t size) {
  if (updating_) return "another update is in progress";
  if (!Update.begin(size, U_FLASH)) return Update.errorString();
  started();
  Serial.printf("OTA (web): receiving %u bytes\n", (unsigned)size);
  return nullptr;
}

bool CamOta::webWrite(const uint8_t *data, size_t len) {
  return Update.write(const_cast<uint8_t *>(data), len) == len;
}

const char *CamOta::webEnd(bool ok) {
  updating_ = false;
  if (!ok) {
    Update.abort();
    Serial.println("OTA (web): aborted");
    return "upload aborted";
  }
  if (!Update.end(true)) {  // verifies the image and sets the boot partition
    Serial.printf("OTA (web): error: %s\n", Update.errorString());
    return Update.errorString();
  }
  Serial.println("OTA (web): done, restarting");
  return nullptr;
}

}  // namespace espcamweb

#endif  // OTA_ON
