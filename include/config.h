// Konfiguracja projektu: opcjonalne moduły kamery i ich piny.
// 1 = moduł włączony, 0 = wyłączony (jego kod w ogóle nie jest kompilowany).
// Biblioteka EspCamWeb czyta ten plik automatycznie; brakujące wpisy dostają wartości domyślne.
#pragma once

// ---------- Moduły ----------
#define LED_ON          1  // lampa LED błysku
#define SD_ON           1  // karta microSD do zapisu zdjęć (gniazdo na płytce, SD_MMC, tryb 1-bit)
#define SERVO_MOTORS_ON 0  // dwa serwa: poziom (pan) i pion (tilt)

// ---------- Lampa LED ----------
#define LED_PIN 4  // dioda błysku na ESP32-CAM AI Thinker

// ---------- Serwa ----------
// Karta SD w trybie 1-bit zajmuje GPIO 2, 14, 15, więc dla serw zostają GPIO 12 i 13.
// UWAGA: GPIO12 to pin strapping (napięcie flash). Jeśli serwo podciągnie go do stanu
// wysokiego przy starcie, płytka się nie uruchomi - wtedy zamień piny albo odłącz serwo
// na czas resetu.
#define SERVO_PAN_PIN  12  // poziom
#define SERVO_TILT_PIN 13  // pion
