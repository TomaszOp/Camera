// Konfiguracja biblioteki EspCamWeb: usługi opcjonalne i ich piny.
// 1 = usługa włączona, 0 = wyłączona (jej kod w ogóle nie jest kompilowany).
// To jedyne miejsce konfiguracji - czytają je zarówno biblioteka, jak i program.
#pragma once

#define ESPCAMWEB_VERSION "1.1.0"

// ---------- Moduły ----------
#define LED_ON          1  // lampa LED błysku
#define SD_ON           1  // karta microSD do zapisu zdjęć (gniazdo na płytce, SD_MMC, tryb 1-bit)
                           // piny SDMMC są stałe w ESP32: CLK 14, CMD 15, D0 2 - nie konfiguruje się ich
#define SERVO_MOTORS_ON 0  // dwa serwa: poziom (pan) i pion (tilt)

// ---------- Serwery poleceń (te same polecenia: help, info, led, servo, photo...) ----------
#define COM_SERVER_ON    1  // polecenia przez port szeregowy (pio device monitor)
#define TELNET_SERVER_ON 1  // polecenia przez Telnet: telnet esp32cam.local
#define TELNET_PORT      23
#define TELNET_PASSWORD  ""  // "" = bez hasła; telnet NIE szyfruje - tylko zaufana sieć

// ---------- Lampa LED ----------
#define LED_PIN 4  // dioda błysku na ESP32-CAM AI Thinker

// ---------- Serwa ----------
// Karta SD w trybie 1-bit zajmuje GPIO 2, 14, 15, więc dla serw zostają GPIO 12 i 13.
// UWAGA: GPIO12 to pin strapping - przy starcie wybiera napięcie pamięci flash
// (stan niski = 3,3 V, prawidłowo). Zwykle działa bez zmian: GPIO12 ma wewnętrzny rezystor
// ściągający, a wejście typowego serwa (SG90, MG90S) nie podciąga linii do plusa.
// Gdyby płytka mimo to nie startowała z podłączonym serwem, dodaj rezystor 10 kΩ między
// GPIO12 a GND - wymusza stan niski przy starcie i nie przeszkadza w sterowaniu serwem.
// Rozwiązanie trwałe (NIEODWRACALNE): espefuse.py set_flash_voltage 3.3V - napięcie flash
// zapisane w eFuse, GPIO12 przestaje być pinem strapping.
#define SERVO_PAN_PIN  12  // poziom
#define SERVO_TILT_PIN 13  // pion

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
