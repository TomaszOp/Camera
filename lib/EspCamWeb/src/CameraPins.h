// Piny modułu kamery. Domyślnie ESP32-CAM AI Thinker (OV2640).
#pragma once

namespace espcamweb {

struct CameraPins {
  int pwdn;
  int reset;
  int xclk;
  int sda;  // SIOD
  int scl;  // SIOC
  int d0, d1, d2, d3, d4, d5, d6, d7;  // Y2..Y9
  int vsync;
  int href;
  int pclk;
};

inline constexpr CameraPins AI_THINKER_PINS = {
    /*pwdn*/ 32, /*reset*/ -1, /*xclk*/ 0, /*sda*/ 26, /*scl*/ 27,
    /*d0..d7*/ 5, 18, 19, 21, 36, 39, 34, 35,
    /*vsync*/ 25, /*href*/ 23, /*pclk*/ 22,
};

// Dioda LED błysku na ESP32-CAM AI Thinker
inline constexpr int AI_THINKER_LED_PIN = 4;

}  // namespace espcamweb
