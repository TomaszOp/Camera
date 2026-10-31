// Konfiguracja projektu: opcjonalne moduły kamery i ich piny.
// 1 = moduł włączony, 0 = wyłączony (jego kod w ogóle nie jest kompilowany).
// Biblioteka EspCamWeb czyta ten plik automatycznie; brakujące wpisy dostają wartości domyślne.
#pragma once

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
// (stan niski = 3,3 V, prawidłowo). Typowe serwo (SG90, MG90S) tego nie zakłóca, bo jego
// wejście nie podciąga linii do plusa. Gdyby płytka mimo to nie startowała z podłączonym
// serwem, dodaj rezystor 10 kΩ między GPIO12 a GND - wymusza stan niski przy starcie
// i nie przeszkadza w sterowaniu serwem.
#define SERVO_PAN_PIN  12  // poziom
#define SERVO_TILT_PIN 13  // pion
