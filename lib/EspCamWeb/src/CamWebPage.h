/**
 * @file CamWebPage.h
 * @brief Camera preview web page (dark, responsive), stored in flash (PROGMEM).
 *
 * The page reads the stream port and the list of available services from /status,
 * so it shows only the controls of services the camera actually provides.
 */
#pragma once

#include <Arduino.h>

namespace espcamweb {

/// HTML of the preview page (with inline CSS and JavaScript).
extern const char INDEX_HTML[] PROGMEM;
/// Length of INDEX_HTML in bytes, without the terminating NUL.
extern const size_t INDEX_HTML_LEN;

}  // namespace espcamweb
