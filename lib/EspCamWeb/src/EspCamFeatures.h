/**
 * @file EspCamFeatures.h
 * @brief EspCamWeb configuration: optional services and their pins.
 *
 * 1 = service enabled, 0 = disabled (its code is not compiled at all).
 * This is the single configuration point - both the library and the program read it.
 * The bottom part checks the pin assignment at compile time (#error on collision).
 */
#pragma once

/// Library version reported by the "info" command and the Telnet banner.
#define ESPCAMWEB_VERSION "1.1.0"

// ---------- Modules ----------
#define LED_ON          1  ///< Flash LED lamp.
#define SD_ON           1  ///< microSD card for photos (on-board slot, SD_MMC, 1-bit mode).
                           ///< SDMMC pins are fixed in the ESP32: CLK 14, CMD 15, D0 2 - not configurable.
#define SERVO_MOTORS_ON 0  ///< Two servos: horizontal (pan) and vertical (tilt).

// ---------- Command servers (same commands: help, info, led, servo, photo...) ----------
#define COM_SERVER_ON    1  ///< Commands over the serial port (pio device monitor).
#define TELNET_SERVER_ON 1  ///< Commands over Telnet: telnet esp32cam.local
#define TELNET_PORT      23 ///< Telnet TCP port.
#define TELNET_PASSWORD  "" ///< "" = no password; Telnet is NOT encrypted - trusted networks only.

// ---------- LED lamp ----------
#define LED_PIN 4  ///< Flash LED on the ESP32-CAM AI Thinker.

// ---------- Servos ----------
// In 1-bit mode the SD card uses GPIO 2, 14, 15, which leaves GPIO 12 and 13 for the servos.
// NOTE: GPIO12 is a strapping pin - at boot it selects the flash memory voltage
// (low = 3.3 V, correct). This normally works as is: GPIO12 has an internal pull-down
// and the input of a typical servo (SG90, MG90S) does not pull the line high.
// If the board still fails to boot with a servo connected, add a 10 kΩ resistor between
// GPIO12 and GND - it forces the low level at boot and does not affect servo control.
// Permanent (IRREVERSIBLE) fix: espefuse.py set_flash_voltage 3.3V - the flash voltage
// is burned into eFuse and GPIO12 is no longer a strapping pin.
#define SERVO_PAN_PIN  12  ///< Horizontal (pan) servo signal.
#define SERVO_TILT_PIN 13  ///< Vertical (tilt) servo signal.

// ---------- Pin collision checks ----------
/// True if GPIO @p p is used by the AI Thinker camera
/// (0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39) or by PSRAM (16, 17).
#define ECW_PIN_IS_CAMERA(p)                                                                   \
  ((p) == 0 || (p) == 5 || (p) == 16 || (p) == 17 || (p) == 18 || (p) == 19 || (p) == 21 || \
   (p) == 22 || (p) == 23 || (p) == 25 || (p) == 26 || (p) == 27 || (p) == 32 || (p) == 34 || \
   (p) == 35 || (p) == 36 || (p) == 39)
/// True if GPIO @p p is used by the SD card in 1-bit mode: CLK 14, CMD 15, D0 2.
#define ECW_PIN_IS_SD(p) ((p) == 2 || (p) == 14 || (p) == 15)

#if LED_ON && ECW_PIN_IS_CAMERA(LED_PIN)
#error "LED_PIN collides with the camera pins"
#endif
#if LED_ON && SD_ON && ECW_PIN_IS_SD(LED_PIN)
#error "LED_PIN collides with the SD card (GPIO 2, 14, 15)"
#endif
#if SERVO_MOTORS_ON
#if SERVO_PAN_PIN == SERVO_TILT_PIN
#error "SERVO_PAN_PIN and SERVO_TILT_PIN must differ"
#endif
#if ECW_PIN_IS_CAMERA(SERVO_PAN_PIN) || ECW_PIN_IS_CAMERA(SERVO_TILT_PIN)
#error "A servo pin collides with the camera pins"
#endif
#if SERVO_PAN_PIN == 34 || SERVO_PAN_PIN == 35 || SERVO_PAN_PIN == 36 || SERVO_PAN_PIN == 39 || \
    SERVO_TILT_PIN == 34 || SERVO_TILT_PIN == 35 || SERVO_TILT_PIN == 36 || SERVO_TILT_PIN == 39
#error "GPIO 34-39 are input-only and cannot drive servos"
#endif
#if SD_ON && (ECW_PIN_IS_SD(SERVO_PAN_PIN) || ECW_PIN_IS_SD(SERVO_TILT_PIN))
#error "A servo pin collides with the SD card (GPIO 2, 14, 15)"
#endif
#if LED_ON && (LED_PIN == SERVO_PAN_PIN || LED_PIN == SERVO_TILT_PIN)
#error "LED_PIN collides with a servo pin"
#endif
#endif
