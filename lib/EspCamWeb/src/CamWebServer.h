/**
 * @file CamWebServer.h
 * @brief Camera web server on esp_http_server (ESP-IDF).
 *
 * - httpPort:   page, /control, /status, /capture, /save (photo to SD card),
 *               /wifi pages and API (WiFi settings, with WIFI_SETTINGS_ON),
 *               /photos browser of the SD card (with SD_ON),
 *               /update firmware upload (with OTA_ON, HTTP Basic Auth with OTA_PASSWORD)
 * - streamPort: /stream (MJPEG) in a separate httpd instance, so the page never blocks the stream
 *
 * Independent of the rest of the library: needs only an EspCamera object
 * (lamp, SD card and servos are reached through the camera).
 */
#pragma once

#include <atomic>
#include <freertos/FreeRTOS.h>
#include "esp_http_server.h"
#include "EspCamWebConfig.h"

namespace espcamweb {

class EspCamera;

/**
 * @brief HTTP front-end of the camera: preview page, JSON status, controls, snapshots and MJPEG stream.
 *
 * Request handlers run in the httpd tasks, not in loop(). Each handler gets this object
 * through httpd_req_t::user_ctx.
 */
class CamWebServer {
 public:
  /**
   * @brief Binds the server to a camera. Nothing is started until begin().
   * @param camera Camera used by all handlers; must outlive the server.
   */
  explicit CamWebServer(EspCamera &camera) : camera_(camera) {}

  /// Stops both httpd instances.
  ~CamWebServer() { end(); }

  /**
   * @brief Starts both httpd instances and registers the URI handlers. Call after WiFi is up.
   * @param settings HTTP and stream ports.
   * @return false if either instance failed to start (both are stopped then).
   */
  bool begin(const ServerSettings &settings = ServerSettings());

  /// @brief Stops both httpd instances (safe to call repeatedly).
  void end();

  /// @return Number of clients currently watching /stream.
  uint8_t streamClients() const { return streamClients_; }

 private:
  /**
   * @brief GET / - serves the preview page from flash.
   * @param req Request.
   * @return ESP_OK or a send error.
   */
  static esp_err_t indexHandler(httpd_req_t *req);

  /**
   * @brief GET /status - JSON with available services, camera settings, FPS, SD and servo state,
   *        WiFi mode/IP/RSSI and the stream port.
   * @param req Request.
   * @return ESP_OK or a send error.
   */
  static esp_err_t statusHandler(httpd_req_t *req);

  /**
   * @brief GET /control?var=X&val=Y - changes a setting: framesize, quality, vflip, hmirror,
   *        led, pan, tilt, center.
   * @param req Request.
   * @return ESP_OK, or ESP_FAIL after sending 400 for a missing/invalid parameter or unavailable service.
   */
  static esp_err_t controlHandler(httpd_req_t *req);

  /**
   * @brief GET /capture - returns a single JPEG frame.
   * @param req Request.
   * @return ESP_OK, or ESP_FAIL after sending 500 when no frame could be taken.
   */
  static esp_err_t captureHandler(httpd_req_t *req);

  /**
   * @brief GET /save - takes a photo and saves it to the SD card; JSON reply with the file name.
   * @param req Request.
   * @return ESP_OK (503 without a card, 500 on write error, 200 with the file name otherwise).
   */
  static esp_err_t saveHandler(httpd_req_t *req);

  /**
   * @brief GET /stream - hands the request to its own FreeRTOS task (httpd async request), so the
   *        httpd task stays free and several clients are streamed at the same time. When
   *        ServerSettings::maxStreamClients streams are running, the oldest one is told to stop.
   * @param req Request.
   * @return ESP_OK when the stream task was started, otherwise an error.
   */
  static esp_err_t streamHandler(httpd_req_t *req);

  /**
   * @brief Task body of one stream client: endless multipart/x-mixed-replace MJPEG stream until the
   *        client disconnects, the stream is kicked out (newer client) or an OTA update starts.
   *        Allows CORS so the page on the HTTP port can read it with fetch().
   * @param arg Async copy of the request (httpd_req_t*); completed and the task deleted at the end.
   */
  static void streamTask(void *arg);

  /**
   * @brief Reserves a stream slot, evicting the oldest stream when all slots are taken.
   * @return Id of the new stream (never 0).
   */
  uint32_t claimStreamSlot();

  /// @brief Frees the slot of a finished stream. @param id Stream id from claimStreamSlot().
  void releaseStreamSlot(uint32_t id);

  /// @return true if the stream @p id must stop (evicted by a newer client). @param id Stream id.
  bool streamEvicted(uint32_t id) const { return kickId_ == id; }

#if WIFI_SETTINGS_ON
  /**
   * @brief GET /wifi - serves the WiFi settings page.
   * @param req Request.
   * @return ESP_OK or a send error.
   */
  static esp_err_t wifiPageHandler(httpd_req_t *req);

  /**
   * @brief GET /wifi/settings - JSON with the current network, the saved settings (never the
   *        password, only whether one is set) and how the network was started.
   * @param req Request.
   * @return ESP_OK or a send error.
   */
  static esp_err_t wifiSettingsHandler(httpd_req_t *req);

  /**
   * @brief POST /wifi/save - form fields mode (sta|ap), ssid, password; validates and saves them.
   *        Takes effect after a manual restart.
   * @param req Request.
   * @return ESP_OK (400 with the error message for invalid input).
   */
  static esp_err_t wifiSaveHandler(httpd_req_t *req);

  /**
   * @brief POST /wifi/clear - erases the saved settings (defaults apply after a restart).
   * @param req Request.
   * @return ESP_OK (500 if NVS could not be cleared).
   */
  static esp_err_t wifiClearHandler(httpd_req_t *req);
#endif

#if SD_ON
  /**
   * @brief GET /photos - page listing the photos on the SD card (pages of 100).
   * @param req Request.
   * @return ESP_OK or a send error.
   */
  static esp_err_t photosPageHandler(httpd_req_t *req);

  /**
   * @brief GET /photos/list?page=N - JSON page of photos, newest first:
   *        {"items":[{"name","size"}...],"total","page","pages","perPage"}. Sent in chunks while listing.
   * @param req Request.
   * @return ESP_OK (503 JSON without a card).
   */
  static esp_err_t photosListHandler(httpd_req_t *req);

  /**
   * @brief GET /photo?name=X - streams a photo (image/jpeg) from the card in chunks.
   *        The name must be a plain .jpg/.jpeg file name in the photo directory.
   * @param req Request.
   * @return ESP_OK, ESP_FAIL after 400/404, or when the client disconnects.
   */
  static esp_err_t photoHandler(httpd_req_t *req);

  /**
   * @brief POST /photo/delete?name=X - deletes a photo (name validated).
   * @param req Request.
   * @return ESP_OK (JSON {"ok","message"}; 400 for an invalid/missing file, 503 without a card).
   */
  static esp_err_t photoDeleteHandler(httpd_req_t *req);
#endif

#if OTA_ON
  /**
   * @brief GET /update - firmware upload page (HTTP Basic Auth with OTA_PASSWORD).
   * @param req Request.
   * @return ESP_OK or a send error.
   */
  static esp_err_t updatePageHandler(httpd_req_t *req);

  /**
   * @brief POST /update - raw firmware.bin body, streamed in 4 KB chunks into the next OTA slot,
   *        verified, then the board restarts (HTTP Basic Auth with OTA_PASSWORD).
   * @param req Request.
   * @return ESP_OK (400/500 JSON with the error message on failure).
   */
  static esp_err_t updateUploadHandler(httpd_req_t *req);
#endif

  EspCamera &camera_;                      ///< Camera served by all handlers.
  ServerSettings settings_;                ///< Ports in use.
  httpd_handle_t webHttpd_ = nullptr;      ///< httpd instance for the page and API.
  httpd_handle_t streamHttpd_ = nullptr;   ///< httpd instance for /stream.
  std::atomic<uint8_t> streamClients_{0};  ///< Active /stream connections.
  static const uint8_t MAX_STREAM_SLOTS = 4;   ///< Upper bound for ServerSettings::maxStreamClients.
  uint32_t streamSlots_[MAX_STREAM_SLOTS] = {};  ///< Ids of running streams, 0 = free slot.
  uint32_t nextStreamId_ = 0;                 ///< Last assigned stream id.
  std::atomic<uint32_t> kickId_{0};           ///< Id of the stream asked to stop (oldest, when full).
  portMUX_TYPE slotsLock_ = portMUX_INITIALIZER_UNLOCKED;  ///< Guards streamSlots_ and nextStreamId_.
};

}  // namespace espcamweb
