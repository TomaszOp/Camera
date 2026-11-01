/**
 * @file CamWebPage.h
 * @brief Camera preview web page (dark, responsive), stored in flash (PROGMEM).
 *
 * The page reads the stream port and the list of available services from /status,
 * so it shows only the controls of services the camera actually provides.
 */
#pragma once

#include <Arduino.h>
#include "EspCamFeatures.h"

namespace espcamweb {

/// HTML of the preview page (with inline CSS and JavaScript).
extern const char INDEX_HTML[] PROGMEM;
/// Length of INDEX_HTML in bytes, without the terminating NUL.
extern const size_t INDEX_HTML_LEN;

#if SD_ON
/// HTML of the SD card photo browser (/photos).
extern const char PHOTOS_HTML[] PROGMEM;
/// Length of PHOTOS_HTML in bytes, without the terminating NUL.
extern const size_t PHOTOS_HTML_LEN;
#endif

#if OTA_ON
/// HTML of the firmware update page (/update).
extern const char UPDATE_HTML[] PROGMEM;
/// Length of UPDATE_HTML in bytes, without the terminating NUL.
extern const size_t UPDATE_HTML_LEN;
#endif

#if WIFI_SETTINGS_ON
/// HTML of the WiFi settings page (/wifi).
extern const char WIFI_HTML[] PROGMEM;
/// Length of WIFI_HTML in bytes, without the terminating NUL.
extern const size_t WIFI_HTML_LEN;
#endif

}  // namespace espcamweb
