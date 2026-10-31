// Wybór opcjonalnych modułów. Wartości bierze z config.h projektu (jeśli jest),
// pozostałe dostają domyślne. Wyłączony moduł nie jest kompilowany.
#pragma once

#if __has_include("config.h")
#include "config.h"
#endif

#ifndef LED_ON
#define LED_ON 1
#endif
#ifndef SD_ON
#define SD_ON 0
#endif
#ifndef SERVO_MOTORS_ON
#define SERVO_MOTORS_ON 0
#endif

#ifndef COM_SERVER_ON
#define COM_SERVER_ON 0
#endif
#ifndef TELNET_SERVER_ON
#define TELNET_SERVER_ON 0
#endif
#ifndef TELNET_PORT
#define TELNET_PORT 23
#endif
#ifndef TELNET_PASSWORD
#define TELNET_PASSWORD ""  // pusty = bez hasła
#endif

#define ESPCAMWEB_VERSION "1.1.0"

#ifndef LED_PIN
#define LED_PIN 4
#endif
#ifndef SERVO_PAN_PIN
#define SERVO_PAN_PIN 12
#endif
#ifndef SERVO_TILT_PIN
#define SERVO_TILT_PIN 13
#endif

// ---------- Zgodność konfiguracji ----------
// Biblioteka i program muszą widzieć ten sam config.h (inaczej klasy miałyby różny układ).
// Biblioteka definiuje symbol z nazwą zależną od konfiguracji, a konstruktor EspCamera
// (rozwijany w programie) się do niego odwołuje. Przy niezgodności linker zgłosi np.:
//   undefined reference to `espcamweb_config_led1_sd1_servo0'
// Rozwiązanie: build_flags = -Iinclude w platformio.ini.
#define ECW_CFG_CAT(l, s, v) espcamweb_config_led##l##_sd##s##_servo##v
#define ECW_CFG_SYM(l, s, v) ECW_CFG_CAT(l, s, v)
#define ECW_CONFIG_SYMBOL ECW_CFG_SYM(LED_ON, SD_ON, SERVO_MOTORS_ON)
extern "C" const int ECW_CONFIG_SYMBOL;

// ---------- Sprawdzenie kolizji pinów ----------
// Kamera AI Thinker: 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39; PSRAM: 16, 17
#define ECW_PIN_IS_CAMERA(p)                                                                   \
  ((p) == 0 || (p) == 5 || (p) == 16 || (p) == 17 || (p) == 18 || (p) == 19 || (p) == 21 || \
   (p) == 22 || (p) == 23 || (p) == 25 || (p) == 26 || (p) == 27 || (p) == 32 || (p) == 34 || \
   (p) == 35 || (p) == 36 || (p) == 39)
// Karta SD w trybie 1-bit: CLK 14, CMD 15, D0 2
#define ECW_PIN_IS_SD(p) ((p) == 2 || (p) == 14 || (p) == 15)

#if LED_ON && ECW_PIN_IS_CAMERA(LED_PIN)
#error "LED_PIN koliduje z pinami kamery"
#endif
#if LED_ON && SD_ON && ECW_PIN_IS_SD(LED_PIN)
#error "LED_PIN koliduje z kartą SD (GPIO 2, 14, 15)"
#endif
#if SERVO_MOTORS_ON
#if SERVO_PAN_PIN == SERVO_TILT_PIN
#error "SERVO_PAN_PIN i SERVO_TILT_PIN muszą być różne"
#endif
#if ECW_PIN_IS_CAMERA(SERVO_PAN_PIN) || ECW_PIN_IS_CAMERA(SERVO_TILT_PIN)
#error "Pin serwa koliduje z pinami kamery"
#endif
#if SERVO_PAN_PIN == 34 || SERVO_PAN_PIN == 35 || SERVO_PAN_PIN == 36 || SERVO_PAN_PIN == 39 || \
    SERVO_TILT_PIN == 34 || SERVO_TILT_PIN == 35 || SERVO_TILT_PIN == 36 || SERVO_TILT_PIN == 39
#error "GPIO 34-39 są tylko wejściami - nie nadają się do serw"
#endif
#if SD_ON && (ECW_PIN_IS_SD(SERVO_PAN_PIN) || ECW_PIN_IS_SD(SERVO_TILT_PIN))
#error "Pin serwa koliduje z kartą SD (GPIO 2, 14, 15)"
#endif
#if LED_ON && (LED_PIN == SERVO_PAN_PIN || LED_PIN == SERVO_TILT_PIN)
#error "LED_PIN koliduje z pinem serwa"
#endif
#endif
