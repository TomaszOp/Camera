// Strona HTML podglądu kamery (ciemna, responsywna). Port strumienia pobiera z /status.
#pragma once

#include <Arduino.h>

namespace espcamweb {

extern const char INDEX_HTML[] PROGMEM;
extern const size_t INDEX_HTML_LEN;

}  // namespace espcamweb
