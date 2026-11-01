# EspCamWeb

**[English](#english)** | **[Polski](#polski)**

## English

PlatformIO library for a live **ESP32-CAM** camera preview over the web: MJPEG stream, FPS counters, resolution and quality changes, flip/mirror. Optional: LED lamp, saving photos to a microSD card, pan/tilt servos, text commands over the serial port (COM) and Telnet, WiFi settings saved in NVS and firmware update over WiFi (OTA).
Configured for the **AI Thinker (OV2640, 4 MB PSRAM)** by default, but the pins and all parameters can be changed.

Requires Arduino core 3.x (pioarduino platform). The `esp_camera` module is built into the core, so no external camera library is needed.

### Adding it to another project

There are three options:

1. **Copy:** copy the `EspCamWeb` directory into your project's `lib/`.
2. **Shared library directory** in `platformio.ini`:
   ```ini
   lib_extra_dirs = /home/tomek/Documents/PlatformIO/Projects/Camera/lib
   ```
3. **Link:** library changes are immediately visible in all projects.
   ```ini
   lib_deps = symlink:///home/tomek/Documents/PlatformIO/Projects/Camera/lib/EspCamWeb
   ```

#### Required `platformio.ini` settings

```ini
[env:esp32cam]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32cam
framework = arduino
monitor_speed = 115200
board_build.partitions = min_spiffs.csv    ; 2 app slots of 1.875 MB for OTA (huge_app.csv = 1 slot, no OTA)
lib_deps =
    lennarthennigs/ESP Telnet@^2.3.0       ; only for TELNET_SERVER_ON (also pulled in by library.json)
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
```

### Optional modules: `EspCamFeatures.h`

Services and their pins are set in a single library file: `src/EspCamFeatures.h` (1 = enabled, 0 = disabled). The code of a disabled service is not compiled at all. The library and the program read the same file, so the settings always match.

```cpp
#define LED_ON          1   // flash LED lamp            (CamLed)
#define SD_ON           1   // microSD card for photos   (CamSdCard)
#define SERVO_MOTORS_ON 0   // pan/tilt servos           (CamServos)
#define COM_SERVER_ON    1  // commands over the serial port (CamComServer)
#define TELNET_SERVER_ON 1  // commands over Telnet          (CamTelnetServer)
#define TELNET_PORT      23
#define TELNET_PASSWORD  "" // "" = no password
#define WIFI_SETTINGS_ON 1  // WiFi settings in NVS          (NvsSettings)
#define OTA_ON           1  // firmware update over WiFi     (CamOta)
#define OTA_PASSWORD     "esp32cam-ota"  // change it! "" = no password

#define LED_PIN        4
#define SERVO_PAN_PIN  12
#define SERVO_TILT_PIN 13
```

- A library added through `symlink://` or `lib_extra_dirs` is shared by every project that uses it, so a change in `EspCamFeatures.h` affects all of them. If a project needs a different set of services, copy the library into its `lib/`.
- `EspCamFeatures.h` checks pin collisions at compile time: with the camera, with the SD card, LED with a servo, pan with tilt, and pins 34–39, which are input-only. A collision gives an `#error` with a description.

#### Pin and LEDC channel assignment (AI Thinker)

| Resource | Pins | LEDC |
|---|---|---|
| Camera | 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39 (PSRAM: 16, 17) | **low-speed** group: channel 0, timer 0 (XCLK 20 MHz, 1 bit) |
| LED lamp | 4 | **high-speed** group: channel 2, timer 0 (5 kHz, 8 bits) |
| SD card (1-bit) | 2 (D0), 14 (CLK), 15 (CMD) | – |
| Pan / tilt servo | 12 / 13 | **high-speed** group: channels 4 and 5, shared timer 1 (50 Hz, 14 bits) |

- The card runs in **1-bit** mode. In 4-bit mode it would also take GPIO 4, 12 and 13, i.e. the lamp and servo pins.

#### Camera, lamp and servos together: timers and interrupts

The camera, lamp and servos can run at the same time without conflicts. Verified against the installed code (Arduino core 3.3.9, esp32-camera from ESP-IDF 5.5):
- **Separate timers.** The ESP32 has two independent LEDC groups with 4 timers each: high-speed and low-speed.
  - The camera driver (`xclk.c`, function `xclk_timer_conf`) sets up XCLK in the **low-speed** group on timer 0, with 1-bit resolution at 20 MHz.
  - Arduino assigns the group as `channel / 8`, so channels 2, 4 and 5 land in the **high-speed** group.
  - There the core picks the first free timer (`find_free_timer` in `esp32-hal-ledc.c`): the lamp gets timer 0 and both servos (same 50 Hz) share timer 1.
  - Changing the lamp brightness or moving the servos therefore never touches the camera clock.
- **Trap: channels 8–15.** They belong to the low-speed group. The Arduino core does not know that the camera has taken timer 0 there, because the camera configures it directly through ESP-IDF. So it could reconfigure it and the camera would lose its clock. `CamLed` and `CamServos` reject channels outside 0–7 (constant `LEDC_SAFE_CHANNELS`) and print an error.
- **Interrupts.**
  - LEDC as used here generates no interrupts: `ledcWrite` only writes the duty, and the LEDC interrupt handler is installed only by `ledcFade`, which is not used. The servos hold their position in hardware, without the CPU.
  - The camera uses the I2S/DMA interrupt and a GPIO interrupt on VSYNC (`ll_cam.c`).
  - The SD card has the SDMMC controller interrupt, and WiFi has its own.
  - These are separate sources allocated independently by ESP-IDF (`esp_intr_alloc`), so nothing is shared.
- **Threads.** Servo and lamp commands come from the web server tasks and from `loop()` (COM/Telnet). `EspCamera` serialises them with a recursive mutex. `capture()` is not locked, so the stream never waits for the servos or for SD writes.

**Servo power – this is the real source of problems:**
- power the servos from a separate 5 V supply (at least 1 A for two small servos), not from the board's 5 V pin,
- the servo ground and the ESP32-CAM ground must be connected (common ground), otherwise the control signal does not work,
- put a 470–1000 µF electrolytic capacitor on the servo supply.

A servo starting to move briefly draws up to 0.5–1 A each. On a shared supply this causes voltage drops, resets (“brownout”), stripes and glitches in the image and WiFi drop-outs.

#### SD card: SDMMC interface, not SPI

`CamSdCard` uses the hardware **SDMMC** controller through the core's `SD_MMC` library. It does not use the `SD` library (`SD.h`), which works over **SPI**. SDMMC is faster and does not occupy the SPI bus. It is also how the microSD slot on the ESP32-CAM board is wired.

**The pins are fixed and cannot be changed.** On the classic ESP32, slot 1 of the SDMMC controller is hard-wired to specific pins. That is why the card pins appear neither in `EspCamFeatures.h` nor in `SdSettings`. ESP-IDF defines them in `soc/esp32/include/soc/sdmmc_pins.h` (`SDMMC_SLOT1_IOMUX_PIN_NUM_*`), and `SD_MMC.begin()` uses them automatically.

| Card signal | GPIO | 1-bit mode (default) | 4-bit mode |
|---|---|---|---|
| CLK | 14 | used | used |
| CMD | 15 | used | used |
| D0 | 2 | used | used |
| D1 | 4 | free (LED lamp) | used |
| D2 | 12 | free (pan servo) | used |
| D3 | 13 | free (tilt servo) | used |

Conclusions:
- The microSD slot on the ESP32-CAM AI Thinker is wired to exactly these pins, together with the pull-up resistors.
- Your own SD slot on another classic ESP32 board must be connected to exactly these pins, with 10 kΩ pull-ups on CMD and D0–D3. A card on other pins would need SPI and a separate class based on `SD.h`.
- The card must be formatted as **FAT32** and inserted **before power-up**. `begin()` detects it only at start-up.
- **Photo browser** (`/photos`): one pass over the directory keeps only the number and size of each `.jpg/.jpeg` file (names only for files not named `IMG_nnnnn.jpg`), sorts newest first and sends just the requested page of 100 as chunked JSON. Photos are streamed from the card in 4 KB chunks, never loaded whole into RAM. File names are validated for opening and deleting (plain name in the photo directory, no `/`, `\` or `..`, `.jpg/.jpeg` only). Listing and deleting go through the `EspCamera` mutex; reading does not (FATFS is re-entrant), so a slow download never blocks other commands, and none of it blocks the stream. No login - anyone on the local network can delete photos.
- On the ESP32-S3 the SDMMC pins can be chosen freely (`SD_MMC.setPins()`), but this library does not support that yet.
- GPIO2 must be low or floating while flashing firmware. A card in the slot sometimes interferes with this. If flashing fails, remove the card for that time.
- **GPIO12 is a strapping pin** that sets the flash memory voltage at boot (low = 3.3 V, correct). This normally works as is: GPIO12 has an internal pull-down and the input of a typical servo (SG90, MG90S) does not pull the line high. If the board still does not boot with a servo connected, add a **10 kΩ resistor between GPIO12 and GND**. It forces the low level at boot and does not affect servo control. Permanent but **irreversible** fix: `espefuse.py set_flash_voltage 3.3V` burns the flash voltage into eFuse, and GPIO12 is no longer a strapping pin.
- Power the servos separately from 5 V, not from the board.
- The ESP32Servo library is not used for the servos, because it allocates LEDC timers on its own and collides with the camera.

### Command servers: COM and Telnet

Both servers are, like the web page, front-ends that use only `EspCamera`. Commands are executed by the shared `CamCommands` class, so they are identical on COM and over Telnet.
- Commands and replies are in English (as are the web page and the Serial logs); commands are case-insensitive. An SSID or password with spaces goes in "double quotes", e.g. `wifi set sta "My Home" "secret pass"`.
- A command of an unavailable service (disabled in `EspCamFeatures.h` or not working, e.g. no card) replies “Service unavailable”, and `help` does not list it.

| Command | Service | Action |
|---|---|---|
| `help` / `?` | – | list of available commands |
| `info` | – | version, uptime, free heap/PSRAM, WiFi, list of services |
| `status` | – | summary: camera, lamp, servos, SD card |
| `wifi` | – | STA/AP mode, SSID, IP, RSSI, saved settings (password masked), saved/default, fallback |
| `wifi set sta <ssid> [password]` | WiFi settings | save a network to join (restart to apply) |
| `wifi set ap <ssid> [password]` | WiFi settings | save an own access point (restart to apply) |
| `wifi clear yes` | WiFi settings | erase the saved settings (defaults after a restart) |
| `cam` | – | resolution, quality, FPS, flip, mirror |
| `cam res qvga\|vga\|svga\|xga` | – | change resolution |
| `cam quality N` | – | JPEG quality 4–63 (lower = better) |
| `cam flip on\|off`, `cam mirror on\|off` | – | flip / mirror the image |
| `photo` | – / SD | with a card: save to SD (file name and size); without a card: frame size only |
| `led` | LED | lamp state |
| `led on\|off\|toggle` | LED | on / off / toggle |
| `servo` | servos | positions and ranges |
| `servo pan N`, `servo tilt N` | servos | horizontal / vertical angle |
| `servo N M` | servos | both angles at once (pan tilt) |
| `servo left\|right\|up\|down [step]` | servos | move by a step (default 10°) |
| `servo center` | servos | center position |
| `sd` | SD | number of photos, free MB, last photo |
| `sd list [N]` | SD | last N photos (default 10) with sizes |
| `sd delete <name>` | SD | delete a photo, e.g. `sd delete IMG_00012.jpg` (plain .jpg/.jpeg name only) |
| `ota` | OTA | OTA state: password set, running partition, next slot and its size |
| `restart yes` | – | restart the board (requires the `yes` confirmation) |
| `quit` / `exit` | – | disconnect (Telnet) |

#### Connecting

- **COM:** `pio device monitor` (115200). The server uses the same `Serial` port as the logs. With the COM server enabled the periodic FPS log is disabled by default so it does not interrupt typing; the `cam` and `status` commands show FPS. The server echoes typed characters and supports Backspace.
- **Telnet:** `telnet esp32cam.local` or `telnet <IP>`; on Windows e.g. PuTTY in Telnet mode. Works in both STA and AP mode.

```text
$ telnet esp32cam.local
EspCamWeb 1.1.0 - ESP32-CAM
Type help for the list of commands, quit to disconnect.
> led on
Lamp: on
> servo 60 100
Service unavailable: servos (disabled in EspCamFeatures.h or not working)
> photo
Saved /photos/IMG_00012.jpg (48 KB)
> sd list 3
  /photos/IMG_00012.jpg  48 KB
  /photos/IMG_00011.jpg  47 KB
  /photos/IMG_00010.jpg  51 KB
> quit
Goodbye.
```

#### Telnet: library and limitations

- The Arduino core has no Telnet server, only a raw `WiFiServer`. That is why `CamTelnetServer` uses the ready-made **ESPTelnet** library (`lennarthennigs/ESP Telnet` 2.3.0, updated in 2026). It compiles with core 3.x without warnings.
- **One client at a time** is supported. A further connection is rejected and a message appears on Serial.
- **Telnet encrypts nothing**, including the `TELNET_PASSWORD`. Use it only on a trusted home network.
- ESPTelnet includes `ETH.h`, so about 12 KB of Ethernet support ends up in the program, even with `TELNET_SERVER_ON 0`. This cannot be turned off without modifying the framework package. **Do not add `lib_ignore = Ethernet`:** pioarduino then removes the Ethernet paths in the shared framework package, which breaks the build of other projects too.

#### Threads

COM and Telnet commands run in `loop()`, the web page in the httpd tasks. `EspCamera` protects state-changing operations (sensor, lamp, servos, SD writes) with a recursive FreeRTOS mutex. This way you can e.g. take a photo from Telnet while watching the stream.

### WiFi settings (NVS)

With `WIFI_SETTINGS_ON` the WiFi role (STA/AP), SSID and password can be changed without reflashing. `NvsSettings` stores them in the board's NVS memory (core `Preferences` library, namespace `ecw_wifi`), so they survive flashing a new program. New settings take effect after a **manual restart** - the board never restarts by itself.

Start-up (`CamWifi::begin(settings, nvs)`):

| Saved settings | Behaviour | On failure |
|---|---|---|
| none | as before: STA with `secrets.h` / `WifiSettings` | default access point from code |
| STA | joins the saved network (empty password = open network) | default access point from code |
| AP | starts an own access point with the saved SSID/password (empty = open AP) | default access point from code |

The Serial log shows whether saved or default settings were used and whether the fallback happened (`WiFi settings: saved (STA)`, `Falling back to the default access point`).

- Validation: SSID 1–32 characters; password empty or 8–63 characters (WPA2 requirement). Invalid input is rejected with a message.
- Changing: the **WiFi settings** page (`/wifi`, linked from the main page), the commands `wifi set sta|ap <ssid> [password]` and `wifi clear yes` over COM/Telnet. All of them go through `EspCamera` (`saveWifiSettings()`, `clearWifiSettings()`, `wifiSettings()`), which never returns the password.
- **The password is never sent:** not in `/status`, not in `/wifi/settings` (only `hasPassword`), not in command replies (shown as `********`) and not in the logs.
- **Security:** the web page has no login - anyone on the local network can change the WiFi settings. Telnet is protected only by `TELNET_PASSWORD`, and Telnet is not encrypted.

### Firmware update (OTA)

`CamOta` (`OTA_ON`) offers two ways, both attached through `EspCamera::attachOta()`:
- **ArduinoOTA** (espota): `pio run -e esp32cam_ota -t upload`, with `upload_protocol = espota`, `upload_port = esp32cam.local`, `upload_flags = --port=3232 --auth=<OTA_PASSWORD>` in `platformio.ini`.
- **Web upload:** page `GET /update` and `POST /update` with the raw `firmware.bin` body. The body is streamed in 4 KB chunks into the next OTA partition (`Update` library), never buffered whole, then verified (`Update.end(true)`); the board restarts after success.
- Both require a partition table with **two app slots** (`min_spiffs.csv`: 2 × 1.875 MB); the first upload after changing the table must be over USB/serial. NVS keeps its address (0x9000), so saved settings survive.
- `OTA_PASSWORD` protects both: espota `--auth`, and HTTP Basic Auth on `/update` (any user name). Default `esp32cam-ota` - change it. "" = no password (anyone on the LAN can flash - not recommended).
- When an update starts, `EspCamera` switches the lamp off and `otaInProgress()` ends the MJPEG stream, so bandwidth and CPU go to the flash write. The servos keep their position.

### Classes

Every class can be used on its own. Everything is in the `espcamweb` namespace, and the single header `#include <EspCamWeb.h>` includes it all.

| Class | File | Role |
|---|---|---|
| `EspCamera` | `EspCamera.h` | **The hub.** Camera: initialisation (with a no-PSRAM fallback), `capture()`/`release()`, resolution, quality, flip, mirror, FPS. All optional services go through it (table below). Needs neither WiFi nor a server. |
| `CamWebServer` | `CamWebServer.h` | Web server (esp_http_server), i.e. a front-end. Depends only on `EspCamera` and shows only the services the camera provides. |
| `CamWifi` | `CamWifi.h` | WiFi station without power saving, mDNS and a fallback access point. |
| `CamLed` | `CamLed.h` | `LED_ON`. LED lamp via `ledcAttachChannel` (GPIO4, channel 2, dim). |
| `CamSdCard` | `CamSdCard.h` | `SD_ON`. microSD card (SD_MMC 1-bit): mounting, free space, saving JPEGs as `/photos/IMG_00001.jpg`… Numbering continues after a restart, because the board has no RTC. |
| `CamServos` | `CamServos.h` | `SERVO_MOTORS_ON`. Two servos: pan (horizontal) and tilt (vertical), 50 Hz PWM, angles 0–180° with limits. |
| `NvsSettings` | `NvsSettings.h` | `WIFI_SETTINGS_ON`. WiFi role, SSID and password in NVS: `begin()`, `save()`, `clear()`, `hasSettings()`, validation. |
| `CamOta` | `CamOta.h` | `OTA_ON`. Firmware update: ArduinoOTA (`begin()`, `handle()` in `loop()`) and web upload into the next OTA slot. |
| `CamCommands` | `CamCommands.h` | Shared text command interpreter: `execute(line, Print&)`. |
| `CamComServer` | `CamComServer.h` | `COM_SERVER_ON`. Commands over the serial port, serviced in `loop()` via `handle()`. |
| `CamTelnetServer` | `CamTelnetServer.h` | `TELNET_SERVER_ON`. Commands over Telnet (ESPTelnet), `handle()` in `loop()`, optional password. |
| `EspCamWeb` | `EspCamWeb.h` | Optional facade that starts all of the above at once. |

The configuration is in `EspCamWebConfig.h`. Every structure has default values.
- **`CameraSettings`**:
  - `pins`: `AI_THINKER_PINS` by default
  - `xclkFreqHz`: 20 MHz
  - `frameSize`: VGA
  - `maxFrameSize`: XGA, the buffers are allocated for this size
  - `jpegQuality`: 12
  - `fbCount`: 2
  - `noPsramFrameSize`: QVGA
- **`WifiSettings`**:
  - `ssid`, `password`: `nullptr` means AP right away
  - `hostname`: `esp32cam`
  - `connectTimeoutMs`: 15000
  - `apFallback`, `apSsid`, `apPassword`, `mdnsHttpPort`
- **`LedSettings`**: `pin`, `channel`, `freq`, `resolutionBits`, `dutyOn`
- **`SdSettings`**:
  - `mountPoint`: `/sdcard`
  - `photoDir`: `/photos`
  - `oneBitMode`: `true`
  - `formatIfMountFailed`: `false`
- **`ServoSettings`**:
  - `panPin`, `tiltPin`: from `EspCamFeatures.h`
  - `panChannel` 4, `tiltChannel` 5
  - `minPulseUs` 500, `maxPulseUs` 2400
  - ranges `panMin`/`panMax`, `tiltMin`/`tiltMax`
  - start position `panStart`/`tiltStart`: 90°
  - reversed direction: `invertPan`, `invertTilt`
- **`ServerSettings`**: `httpPort` 80, `streamPort` 81
- **`ComServerSettings`**: `echo` (`true`), `prompt` (`"> "`)
- **`TelnetSettings`**: `port` (`TELNET_PORT`), `password` (`TELNET_PASSWORD`)
- **`EspCamWebConfig`**: all of the above plus `fpsLogIntervalMs` (5000, or 0 with `COM_SERVER_ON`, i.e. no periodic log)

Another camera module: fill in your own `CameraPins` structure and assign it to `config.camera.pins`.

Dependencies: `CamWebServer` / `CamComServer` / `CamTelnetServer` → `EspCamera` → `CamLed` / `CamSdCard` / `CamServos` (each optional). The COM and Telnet servers use `CamCommands`. `CamWifi` is independent.
Modules are attached to the camera by reference rather than created inside it. This way the camera works without them, and every class can also be used on its own.

#### Services in `EspCamera`

| Service | Attaching | Methods |
|---|---|---|
| all | – | `features()` returns `{led, sd, servos, settings, ota}`: enabled in `EspCamFeatures.h` **and** started successfully (e.g. card inserted) |
| lamp | `attachLed(CamLed&)` | `hasLed()`, `setLed(bool)`, `ledOn()` |
| SD card | `attachSd(CamSdCard&)` | `hasSd()`, `savePhotoToSd(&bytes)` (returns the file path), `sdPhotoCount()`, `sdFreeBytes()`, `sdLastPhoto()`, `sdListPhotos(Print&, N)`, `sdListPhotoPage(page, perPage, fn)`, `sdReadPhoto(name, sink)`, `sdDeletePhoto(name)` |
| servos | `attachServos(CamServos&)` | `hasServos()`, `setPan(deg)`, `setTilt(deg)`, `centerServos()`, `pan()`, `tilt()`, `servoLimits(...)` |
| WiFi settings | `attachSettings(NvsSettings&)` | `hasWifiSettings()`, `wifiSettings(info)` (without the password), `saveWifiSettings(role, ssid, password)`, `clearWifiSettings()` |
| OTA | `attachOta(CamOta&)` | `hasOta()`, `otaInProgress()`, `otaInfo(info)`, `otaAuthorize(password)`, `otaBegin(size)`, `otaWrite(data, len)`, `otaEnd(ok)` |

The `attach…()` methods exist only when the module is enabled. The other methods always exist and return `false`, 0 or -1 when the module is disabled. This way the server needs no `#if`.

### Examples

#### Explicit use of the classes

```cpp
#include <EspCamWeb.h>
using namespace espcamweb;

EspCamera camera;
CamLed led;
CamWifi wifi;
CamWebServer server(camera);   // the server depends only on the camera
#if SD_ON
CamSdCard sd;
#endif

void setup() {
  Serial.begin(115200);
  led.begin(LedSettings());
  camera.attachLed(led);   // the lamp belongs to the camera
  if (!camera.begin(CameraSettings())) ESP.restart();
#if SD_ON
  sd.begin(SdSettings());
  camera.attachSd(sd);     // no card -> hasSd() == false, the page hides the button
#endif

  WifiSettings net;
  net.ssid = "YourNetwork";
  net.password = "YourPassword";
  wifi.begin(net);
  server.begin(ServerSettings());
}

void loop() {
  camera.updateFps(millis());   // FPS shown in /status
  delay(100);
}
```

#### Camera only, without a server

```cpp
EspCamera camera;
camera.begin(CameraSettings());
camera_fb_t *fb = camera.capture();   // JPEG: fb->buf, fb->len
if (fb) { /* send, process... */ camera.release(fb); }
camera.savePhotoToSd();               // when a card is attached (SD_ON)
camera.setPan(45);                    // when servos are attached (SERVO_MOTORS_ON)
```

#### Facade

See `examples/Basic/main.cpp`: configuration, `cam.begin(config)` in `setup()` and `cam.loop()` in `loop()`. The facade creates and attaches the modules enabled in `EspCamFeatures.h` by itself. A missing SD card is not an error: the service is simply unavailable.

### HTTP endpoints

| Address | Description |
|---|---|
| `http://<IP>/` | page with the preview and controls |
| `http://<IP>/status` | JSON: `features` (`led`, `sd`, `servos`, `settings`, `ota`), fps, resolution, quality, led, flip, RSSI, PSRAM, number of clients, stream port, `sdPhotos`, `sdFreeMB`, `sdLast`, `pan`, `tilt` |
| `http://<IP>/control?var=X&val=Y` | `framesize` (5 QVGA, 8 VGA, 9 SVGA, 10 XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1), `pan`, `tilt` (degrees), `center` |
| `http://<IP>/capture` | single JPEG snapshot |
| `http://<IP>/save` | take a photo and save it to the SD card; JSON `{"ok":true,"file":"/photos/IMG_00001.jpg"}`, code 503 without a card |
| `http://<IP>/wifi` | WiFi settings page (`WIFI_SETTINGS_ON`) |
| `GET http://<IP>/wifi/settings` | JSON: current network, `source` (saved/default), `fallback`, saved settings (`mode`, `ssid`, `hasPassword` - never the password) |
| `POST http://<IP>/wifi/save` | form fields `mode` (sta/ap), `ssid`, `password`; validated, reply `{"ok":..,"message":".."}` |
| `POST http://<IP>/wifi/clear` | erase the saved settings |
| `http://<IP>/photos` | photo browser of the SD card (`SD_ON`): 100 per page, newest first, open and delete |
| `GET http://<IP>/photos/list?page=N` | JSON `{"items":[{"name","size"}],"total","page","pages","perPage"}`; 503 without a card |
| `GET http://<IP>/photo?name=IMG_00001.jpg` | the photo (image/jpeg), streamed from the card in 4 KB chunks |
| `POST http://<IP>/photo/delete?name=IMG_00001.jpg` | delete a photo; JSON `{"ok","message"}` |
| `http://<IP>/update` | firmware update page (`OTA_ON`, HTTP Basic Auth with `OTA_PASSWORD`) |
| `POST http://<IP>/update` | raw `firmware.bin` body; streamed into the OTA slot, verified, then restart |
| `http://<IP>:81/stream` | MJPEG stream (separate httpd instance) |

Notes:
- The page shows the lamp button, the SD card section and the servo controls (sliders, arrows, center) only when `/status` → `features` says the service is available.
- `setFrameSize()` rejects a resolution larger than `maxFrameSize`, because the buffer would overflow. The page hides such buttons.
- FPS is counted by `EspCamera` from the frames taken with `capture()`. The value is refreshed by `updateFps()`, which must be called in `loop()`; the facade does it by itself.

---

## Polski

Biblioteka PlatformIO do podglądu kamery **ESP32-CAM** na żywo przez WWW: strumień MJPEG, liczniki FPS, zmiana rozdzielczości i jakości, flip/lustro. Opcjonalnie: lampa LED, zapis zdjęć na kartę microSD, serwa pan/tilt, sterowanie poleceniami tekstowymi przez port szeregowy (COM) i Telnet ustawienia WiFi zapisane w NVS oraz aktualizacja programu przez WiFi (OTA).
Domyślnie skonfigurowana pod **AI Thinker (OV2640, 4 MB PSRAM)**, ale piny i wszystkie parametry można zmienić.

Wymaga Arduino core 3.x (platforma pioarduino). Moduł `esp_camera` jest wbudowany w core, więc nie trzeba dodawać zewnętrznej biblioteki kamery.

### Dodanie do innego projektu

Masz trzy możliwości:

1. **Kopia:** skopiuj katalog `EspCamWeb` do `lib/` swojego projektu.
2. **Wspólny katalog bibliotek** w `platformio.ini`:
   ```ini
   lib_extra_dirs = /home/tomek/Documents/PlatformIO/Projects/Camera/lib
   ```
3. **Dowiązanie:** zmiany w bibliotece są od razu widoczne we wszystkich projektach.
   ```ini
   lib_deps = symlink:///home/tomek/Documents/PlatformIO/Projects/Camera/lib/EspCamWeb
   ```

#### Wymagane ustawienia `platformio.ini`

```ini
[env:esp32cam]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32cam
framework = arduino
monitor_speed = 115200
board_build.partitions = min_spiffs.csv    ; 2 sloty po 1,875 MB dla OTA (huge_app.csv = 1 slot, bez OTA)
lib_deps =
    lennarthennigs/ESP Telnet@^2.3.0       ; tylko dla TELNET_SERVER_ON (dociąga się też z library.json)
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
```

### Moduły opcjonalne: `EspCamFeatures.h`

Usługi i ich piny ustawia się w jednym pliku biblioteki: `src/EspCamFeatures.h` (1 = włączona, 0 = wyłączona). Kod wyłączonej usługi w ogóle nie jest kompilowany. Ten sam plik czytają biblioteka i program, więc ustawienia są zawsze zgodne.

```cpp
#define LED_ON          1   // lampa LED błysku          (CamLed)
#define SD_ON           1   // karta microSD na zdjęcia  (CamSdCard)
#define SERVO_MOTORS_ON 0   // serwa poziom/pion         (CamServos)
#define COM_SERVER_ON    1  // polecenia przez port szeregowy (CamComServer)
#define TELNET_SERVER_ON 1  // polecenia przez Telnet         (CamTelnetServer)
#define TELNET_PORT      23
#define TELNET_PASSWORD  "" // "" = bez hasła
#define WIFI_SETTINGS_ON 1  // ustawienia WiFi w NVS         (NvsSettings)
#define OTA_ON           1  // aktualizacja przez WiFi       (CamOta)
#define OTA_PASSWORD     "esp32cam-ota"  // zmień! "" = bez hasła

#define LED_PIN        4
#define SERVO_PAN_PIN  12
#define SERVO_TILT_PIN 13
```

- Biblioteka dołączona przez `symlink://` lub `lib_extra_dirs` jest wspólna dla wszystkich projektów, które z niej korzystają, więc zmiana w `EspCamFeatures.h` dotyczy ich wszystkich. Jeśli projekt potrzebuje innego zestawu usług, skopiuj bibliotekę do jego `lib/`.
- `EspCamFeatures.h` sprawdza przy kompilacji kolizje pinów: z kamerą, z kartą SD, LED z serwem, pan z tilt oraz piny 34–39, które są tylko wejściami. Kolizja daje błąd `#error` z opisem.

#### Przydział pinów i kanałów LEDC (AI Thinker)

| Zasób | Piny | LEDC |
|---|---|---|
| Kamera | 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39 (PSRAM: 16, 17) | grupa **low-speed**: kanał 0, timer 0 (XCLK 20 MHz, 1 bit) |
| Lampa LED | 4 | grupa **high-speed**: kanał 2, timer 0 (5 kHz, 8 bitów) |
| Karta SD (1-bit) | 2 (D0), 14 (CLK), 15 (CMD) | – |
| Serwo pan / tilt | 12 / 13 | grupa **high-speed**: kanały 4 i 5, wspólny timer 1 (50 Hz, 14 bitów) |

- Karta pracuje w trybie **1-bit**. W trybie 4-bit zajęłaby też GPIO 4, 12 i 13, czyli piny lampy i serw.

#### Kamera, lampa i serwa jednocześnie: timery i przerwania

Kamera, lampa i serwa mogą działać jednocześnie, bez konfliktów. Sprawdziłem to w zainstalowanym kodzie (Arduino core 3.3.9, esp32-camera z ESP-IDF 5.5):
- **Osobne timery.** ESP32 ma dwie niezależne grupy LEDC po 4 timery: high-speed i low-speed.
  - Sterownik kamery (`xclk.c`, funkcja `xclk_timer_conf`) ustawia XCLK w grupie **low-speed** na timerze 0, z rozdzielczością 1 bit i częstotliwością 20 MHz.
  - Arduino przydziela grupę jako `kanał / 8`, więc kanały 2, 4 i 5 trafiają do grupy **high-speed**.
  - Core wybiera tam pierwszy wolny timer (`find_free_timer` w `esp32-hal-ledc.c`): lampa dostaje timer 0, a oba serwa (te same 50 Hz) wspólny timer 1.
  - Zmiana jasności lampy ani ruch serw nie dotykają więc zegara kamery.
- **Pułapka: kanały 8–15.** Należą do grupy low-speed. Core Arduino nie wie, że kamera zajęła tam timer 0, bo kamera ustawia go bezpośrednio przez ESP-IDF. Dlatego mógłby go przestawić, a kamera straciłaby zegar. `CamLed` i `CamServos` odrzucają kanały spoza 0–7 (stała `LEDC_SAFE_CHANNELS`) i wypisują błąd.
- **Przerwania.**
  - LEDC w naszym użyciu nie generuje przerwań: `ledcWrite` tylko zapisuje wypełnienie, a obsługa przerwania LEDC instaluje się dopiero przy `ledcFade`, którego nie używamy. Serwa trzymają pozycję sprzętowo, bez udziału procesora.
  - Kamera korzysta z przerwania I2S/DMA i z przerwania GPIO na VSYNC (`ll_cam.c`).
  - Karta SD ma przerwanie kontrolera SDMMC, a WiFi własne.
  - To osobne źródła, które ESP-IDF przydziela niezależnie (`esp_intr_alloc`), więc nic się nie dzieli.
- **Wątki.** Polecenia serw i lampy przychodzą z zadań serwera WWW oraz z `loop()` (COM/Telnet). `EspCamera` szereguje je rekurencyjnym mutexem. `capture()` nie jest blokowane, więc strumień nie czeka na serwa ani na zapis na kartę.

**Zasilanie serw – to jest realne źródło problemów:**
- serwa zasilaj z osobnego źródła 5 V (min. 1 A na dwa małe serwa), nie z pinu 5 V płytki,
- masa serw i masa ESP32-CAM muszą być połączone (wspólna masa), inaczej sygnał sterujący nie działa,
- przy serwach daj kondensator elektrolityczny 470–1000 µF na zasilaniu.

Ruszające serwo pobiera chwilowo nawet 0,5–1 A na sztukę. Na wspólnym zasilaniu powoduje to spadki napięcia, restarty („brownout”), paski i przekłamania obrazu oraz zrywanie WiFi.

#### Karta SD: interfejs SDMMC, nie SPI

`CamSdCard` korzysta ze sprzętowego kontrolera **SDMMC** przez bibliotekę `SD_MMC` z core. Nie używa biblioteki `SD` (`SD.h`), która działa przez **SPI**. SDMMC jest szybszy i nie zajmuje magistrali SPI. Tak jest też podłączone gniazdo microSD na płytce ESP32-CAM.

**Piny są stałe i nie da się ich zmienić.** W klasycznym ESP32 slot 1 kontrolera SDMMC jest na stałe połączony z konkretnymi nóżkami. Dlatego piny karty nie występują w `EspCamFeatures.h` ani w `SdSettings`. Definiuje je ESP-IDF w pliku `soc/esp32/include/soc/sdmmc_pins.h` (`SDMMC_SLOT1_IOMUX_PIN_NUM_*`), a `SD_MMC.begin()` używa ich automatycznie.

| Sygnał karty | GPIO | Tryb 1-bit (domyślny) | Tryb 4-bit |
|---|---|---|---|
| CLK | 14 | używany | używany |
| CMD | 15 | używany | używany |
| D0 | 2 | używany | używany |
| D1 | 4 | wolny (lampa LED) | używany |
| D2 | 12 | wolny (serwo pan) | używany |
| D3 | 13 | wolny (serwo tilt) | używany |

Wnioski:
- Gniazdo microSD na ESP32-CAM AI Thinker jest podłączone właśnie do tych pinów, razem z rezystorami podciągającymi.
- Własne gniazdo SD na innej płytce z klasycznym ESP32 musi być podłączone dokładnie do tych pinów, z rezystorami podciągającymi 10 kΩ na CMD i D0–D3. Karta na innych pinach wymagałaby SPI i osobnej klasy opartej na `SD.h`.
- Karta musi być sformatowana jako **FAT32** i włożona **przed włączeniem zasilania**. `begin()` wykrywa ją tylko przy starcie.
- **Przeglądarka zdjęć** (`/photos`):
  - Jedno przejście przez katalog zapamiętuje tylko numer i rozmiar każdego pliku `.jpg/.jpeg`; nazwę tylko dla plików nienazwanych `IMG_nnnnn.jpg`. Potem sortuje od najnowszych i wysyła wyłącznie żądaną stronę 100 pozycji jako JSON w kawałkach.
  - Zdjęcia są wysyłane z karty kawałkami po 4 KB, nigdy w całości z RAM.
  - Nazwy plików są sprawdzane przy otwieraniu i usuwaniu: tylko sama nazwa w katalogu zdjęć, bez `/`, `\` i `..`, tylko `.jpg/.jpeg`.
  - Listowanie i usuwanie idą przez mutex `EspCamera`, a odczyt nie, bo FATFS obsługuje wiele zadań jednocześnie. Dzięki temu wolne pobieranie nie blokuje innych poleceń, a nic z tego nie blokuje strumienia.
  - Strona nie ma logowania, więc każdy w sieci lokalnej może usuwać zdjęcia.
- Na ESP32-S3 piny SDMMC można wybrać dowolnie (`SD_MMC.setPins()`), ale ta biblioteka obecnie tego nie obsługuje.
- GPIO2 musi być w stanie niskim lub nieustalonym przy wgrywaniu firmware. Karta włożona w gniazdo czasem to zakłóca. Jeśli wgrywanie się nie udaje, wyjmij kartę na ten czas.
- **GPIO12 to pin strapping**, który przy starcie ustala napięcie pamięci flash (stan niski = 3,3 V, prawidłowo). Zwykle działa bez zmian: GPIO12 ma wewnętrzny rezystor ściągający, a wejście typowego serwa (SG90, MG90S) nie podciąga linii do plusa. Gdyby płytka mimo to nie startowała z podłączonym serwem, dodaj **rezystor 10 kΩ między GPIO12 a GND**. Wymusza on stan niski przy starcie i nie przeszkadza w sterowaniu serwem. Rozwiązanie trwałe, ale **nieodwracalne**: `espefuse.py set_flash_voltage 3.3V` ustawia napięcie flash w eFuse, a wtedy GPIO12 przestaje być pinem strapping.
- Serwa zasilaj osobno z 5 V, nie z płytki.
- Do serw nie używam biblioteki ESP32Servo, bo sama przydziela timery LEDC i koliduje z kamerą.

### Serwery poleceń: COM i Telnet

Oba serwery są, tak jak strona WWW, dodatkami korzystającymi wyłącznie z `EspCamera`. Polecenia wykonuje wspólna klasa `CamCommands`, więc na COM i przez Telnet są identyczne.
- Polecenia i odpowiedzi są po angielsku (tak samo strona WWW i logi na Serial), a wielkość liter w poleceniach nie ma znaczenia. SSID lub hasło ze spacjami podaje się w cudzysłowie, np. `wifi set sta "My Home" "secret pass"`.
- Polecenie usługi niedostępnej (wyłączonej w `EspCamFeatures.h` albo niedziałającej, np. bez karty) zwraca komunikat „Service unavailable”, a `help` go nie pokazuje.

| Polecenie | Usługa | Działanie |
|---|---|---|
| `help` / `?` | – | lista dostępnych poleceń |
| `info` | – | wersja, czas pracy, wolny heap/PSRAM, WiFi, lista usług |
| `status` | – | skrót stanu: kamera, lampa, serwa, karta SD |
| `wifi` | – | tryb STA/AP, SSID, IP, RSSI, zapisane ustawienia (hasło zamaskowane), saved/default, fallback |
| `wifi set sta <ssid> [hasło]` | ustawienia WiFi | zapis sieci, z którą płytka ma się łączyć (działa po restarcie) |
| `wifi set ap <ssid> [hasło]` | ustawienia WiFi | zapis własnego punktu dostępowego (działa po restarcie) |
| `wifi clear yes` | ustawienia WiFi | skasowanie zapisanych ustawień (po restarcie domyślne) |
| `cam` | – | rozdzielczość, jakość, FPS, flip, lustro |
| `cam res qvga\|vga\|svga\|xga` | – | zmiana rozdzielczości |
| `cam quality N` | – | jakość JPEG 4–63 (mniej = lepsza) |
| `cam flip on\|off`, `cam mirror on\|off` | – | odwrócenie obrazu |
| `photo` | – / SD | z kartą: zapis na SD (nazwa pliku i rozmiar); bez karty: tylko rozmiar klatki |
| `led` | LED | stan lampy |
| `led on\|off\|toggle` | LED | włącz / wyłącz / przełącz |
| `servo` | serwa | pozycje i zakresy |
| `servo pan N`, `servo tilt N` | serwa | kąt poziomo / pionowo |
| `servo N M` | serwa | oba kąty naraz (pan tilt) |
| `servo left\|right\|up\|down [krok]` | serwa | przesunięcie o krok (domyślnie 10°) |
| `servo center` | serwa | pozycja środkowa |
| `sd` | SD | liczba zdjęć, wolne MB, ostatnie zdjęcie |
| `sd list [N]` | SD | ostatnie N zdjęć (domyślnie 10) z rozmiarami |
| `sd delete <nazwa>` | SD | usunięcie zdjęcia, np. `sd delete IMG_00012.jpg` (tylko sama nazwa .jpg/.jpeg) |
| `ota` | OTA | stan OTA: hasło ustawione, bieżąca partycja, następny slot i jego rozmiar |
| `restart yes` | – | restart płytki (wymaga potwierdzenia `yes`) |
| `quit` / `exit` | – | rozłączenie (Telnet) |

#### Połączenie

- **COM:** `pio device monitor` (115200). Serwer korzysta z tego samego portu `Serial`, na który idą logi. Przy włączonym serwerze COM okresowy log FPS jest domyślnie wyłączony, żeby nie przerywał pisania. FPS pokazują polecenia `cam` i `status`. Serwer odsyła wpisywane znaki (echo) i obsługuje Backspace.
- **Telnet:** `telnet esp32cam.local` albo `telnet <IP>`; na Windows np. PuTTY w trybie Telnet. Działa w trybie STA i AP.

```text
$ telnet esp32cam.local
EspCamWeb 1.1.0 - ESP32-CAM
Type help for the list of commands, quit to disconnect.
> led on
Lamp: on
> servo 60 100
Service unavailable: servos (disabled in EspCamFeatures.h or not working)
> photo
Saved /photos/IMG_00012.jpg (48 KB)
> sd list 3
  /photos/IMG_00012.jpg  48 KB
  /photos/IMG_00011.jpg  47 KB
  /photos/IMG_00010.jpg  51 KB
> quit
Goodbye.
```

#### Telnet: biblioteka i ograniczenia

- Core Arduino nie ma serwera telnet, jest tylko surowy `WiFiServer`. Dlatego `CamTelnetServer` używa gotowej biblioteki **ESPTelnet** (`lennarthennigs/ESP Telnet` 2.3.0, aktualizowana w 2026 r.). Kompiluje się z core 3.x bez ostrzeżeń.
- Obsługiwany jest **jeden klient naraz**. Kolejne połączenie jest odrzucane, a na Serial pojawia się komunikat.
- **Telnet nie szyfruje** niczego, także hasła `TELNET_PASSWORD`. Używaj go tylko w zaufanej sieci domowej.
- ESPTelnet dołącza `ETH.h`, więc do programu trafia ok. 12 KB obsługi Ethernetu, nawet przy `TELNET_SERVER_ON 0`. Nie da się tego wyłączyć bez zmian w pakiecie frameworka. **Nie dodawaj `lib_ignore = Ethernet`:** pioarduino usuwa wtedy ścieżki Ethernetu we współdzielonym pakiecie frameworka, co psuje kompilację również innych projektów.

#### Wątki

Polecenia COM i Telnet wykonują się w `loop()`, a strona WWW w zadaniach httpd. `EspCamera` chroni operacje zmieniające stan (sensor, lampa, serwa, zapis na SD) rekurencyjnym mutexem FreeRTOS. Dzięki temu można np. zrobić zdjęcie z Telnetu w trakcie oglądania strumienia.

### Ustawienia WiFi (NVS)

Z `WIFI_SETTINGS_ON` tryb WiFi (STA/AP), SSID i hasło można zmieniać bez ponownego wgrywania programu. `NvsSettings` zapisuje je w pamięci NVS płytki (biblioteka `Preferences` z core, przestrzeń nazw `ecw_wifi`), więc przetrwają wgranie nowego programu. Nowe ustawienia działają po **ręcznym restarcie**, bo płytka nigdy nie restartuje się sama.

Start (`CamWifi::begin(settings, nvs)`):

| Zapisane ustawienia | Działanie | Przy błędzie |
|---|---|---|
| brak | jak dotąd: STA z `secrets.h` / `WifiSettings` | domyślny punkt dostępowy z kodu |
| STA | łączy się z zapisaną siecią (puste hasło = sieć otwarta) | domyślny punkt dostępowy z kodu |
| AP | uruchamia własny punkt dostępowy z zapisanym SSID i hasłem (puste = otwarty AP) | domyślny punkt dostępowy z kodu |

Log na Serial pokazuje, czy użyto zapisanych, czy domyślnych ustawień i czy nastąpił fallback (`WiFi settings: saved (STA)`, `Falling back to the default access point`).

- Walidacja: SSID 1–32 znaki, hasło puste albo 8–63 znaki (wymóg WPA2). Błędne dane są odrzucane z komunikatem.
- Zmiana: strona **WiFi settings** (`/wifi`, link na stronie głównej) oraz polecenia `wifi set sta|ap <ssid> [hasło]` i `wifi clear yes` przez COM/Telnet. Wszystkie idą przez `EspCamera` (`saveWifiSettings()`, `clearWifiSettings()`, `wifiSettings()`), która nigdy nie zwraca hasła.
- **Hasło nigdy nie jest wysyłane:** nie ma go w `/status` ani w `/wifi/settings` (jest tylko `hasPassword`), w odpowiedziach poleceń jest pokazane jako `********` i nie trafia do logów.
- **Bezpieczeństwo:** strona nie ma logowania, więc każdy w sieci lokalnej może zmienić ustawienia WiFi. Telnet chroni tylko `TELNET_PASSWORD`, a Telnet nie jest szyfrowany.

### Aktualizacja programu (OTA)

`CamOta` (`OTA_ON`) daje dwa sposoby, oba dołączone przez `EspCamera::attachOta()`:
- **ArduinoOTA** (espota): `pio run -e esp32cam_ota -t upload`, z `upload_protocol = espota`, `upload_port = esp32cam.local`, `upload_flags = --port=3232 --auth=<OTA_PASSWORD>` w `platformio.ini`.
- **Wgrywanie przez stronę:** strona `GET /update` i `POST /update` z surowym plikiem `firmware.bin` w treści. Plik trafia kawałkami po 4 KB prosto do następnej partycji OTA (biblioteka `Update`), bez buforowania całości. Potem jest weryfikowany (`Update.end(true)`), a po sukcesie płytka się restartuje.
- Oba sposoby wymagają tablicy partycji z **dwoma slotami na program** (`min_spiffs.csv`: 2 × 1,875 MB). Pierwsze wgranie po zmianie tablicy musi być przez USB/UART. NVS zostaje pod tym samym adresem (0x9000), więc zapisane ustawienia przetrwają.
- `OTA_PASSWORD` chroni oba sposoby: `--auth` w espota oraz HTTP Basic Auth na `/update` (nazwa użytkownika dowolna). Domyślnie `esp32cam-ota`, więc zmień je. "" oznacza brak hasła, czyli każdy w sieci lokalnej może wgrać program (niezalecane).
- Gdy aktualizacja się zaczyna, `EspCamera` gasi lampę, a `otaInProgress()` kończy strumień MJPEG, żeby łącze i procesor obsługiwały zapis do flash. Serwa zostają w swojej pozycji.

### Klasy

Każdej klasy można używać samodzielnie. Wszystko jest w przestrzeni nazw `espcamweb`, a jeden nagłówek `#include <EspCamWeb.h>` dołącza całość.

| Klasa | Plik | Rola |
|---|---|---|
| `EspCamera` | `EspCamera.h` | **Centrum.** Kamera: inicjalizacja (z trybem awaryjnym bez PSRAM), `capture()`/`release()`, rozdzielczość, jakość, flip, lustro, FPS. Wszystkie usługi opcjonalne idą przez nią (tabela niżej). Nie potrzebuje WiFi ani serwera. |
| `CamWebServer` | `CamWebServer.h` | Serwer WWW (esp_http_server), czyli dodatek. Zależy wyłącznie od `EspCamera` i pokazuje tylko te usługi, które kamera udostępnia. |
| `CamWifi` | `CamWifi.h` | WiFi STA bez usypiania, mDNS i awaryjny punkt dostępowy. |
| `CamLed` | `CamLed.h` | `LED_ON`. Lampa LED przez `ledcAttachChannel` (GPIO4, kanał 2, słaba jasność). |
| `CamSdCard` | `CamSdCard.h` | `SD_ON`. Karta microSD (SD_MMC 1-bit): montowanie, wolne miejsce, zapis JPEG jako `/photos/IMG_00001.jpg`… Numeracja jest kontynuowana po restarcie, bo płytka nie ma zegara RTC. |
| `CamServos` | `CamServos.h` | `SERVO_MOTORS_ON`. Dwa serwa: pan (poziom) i tilt (pion), PWM 50 Hz, kąty 0–180° z limitami. |
| `NvsSettings` | `NvsSettings.h` | `WIFI_SETTINGS_ON`. Tryb WiFi, SSID i hasło w NVS: `begin()`, `save()`, `clear()`, `hasSettings()`, walidacja. |
| `CamOta` | `CamOta.h` | `OTA_ON`. Aktualizacja programu: ArduinoOTA (`begin()`, `handle()` w `loop()`) i wgrywanie przez stronę do następnego slotu OTA. |
| `CamCommands` | `CamCommands.h` | Wspólny interpreter poleceń tekstowych: `execute(linia, Print&)`. |
| `CamComServer` | `CamComServer.h` | `COM_SERVER_ON`. Polecenia przez port szeregowy, obsługiwane w `loop()` przez `handle()`. |
| `CamTelnetServer` | `CamTelnetServer.h` | `TELNET_SERVER_ON`. Polecenia przez Telnet (ESPTelnet), `handle()` w `loop()`, opcjonalne hasło. |
| `EspCamWeb` | `EspCamWeb.h` | Opcjonalna fasada, która uruchamia wszystkie powyższe naraz. |

Konfiguracja jest w `EspCamWebConfig.h`. Każda struktura ma domyślne wartości.
- **`CameraSettings`**:
  - `pins`: domyślnie `AI_THINKER_PINS`
  - `xclkFreqHz`: 20 MHz
  - `frameSize`: VGA
  - `maxFrameSize`: XGA, na ten rozmiar alokowane są bufory
  - `jpegQuality`: 12
  - `fbCount`: 2
  - `noPsramFrameSize`: QVGA
- **`WifiSettings`**:
  - `ssid`, `password`: `nullptr` oznacza od razu AP
  - `hostname`: `esp32cam`
  - `connectTimeoutMs`: 15000
  - `apFallback`, `apSsid`, `apPassword`, `mdnsHttpPort`
- **`LedSettings`**: `pin`, `channel`, `freq`, `resolutionBits`, `dutyOn`
- **`SdSettings`**:
  - `mountPoint`: `/sdcard`
  - `photoDir`: `/photos`
  - `oneBitMode`: `true`
  - `formatIfMountFailed`: `false`
- **`ServoSettings`**:
  - `panPin`, `tiltPin`: z `EspCamFeatures.h`
  - `panChannel` 4, `tiltChannel` 5
  - `minPulseUs` 500, `maxPulseUs` 2400
  - zakresy `panMin`/`panMax`, `tiltMin`/`tiltMax`
  - pozycja startowa `panStart`/`tiltStart`: 90°
  - odwrócenie kierunku: `invertPan`, `invertTilt`
- **`ServerSettings`**: `httpPort` 80, `streamPort` 81
- **`ComServerSettings`**: `echo` (`true`), `prompt` (`"> "`)
- **`TelnetSettings`**: `port` (`TELNET_PORT`), `password` (`TELNET_PASSWORD`)
- **`EspCamWebConfig`**: wszystkie powyższe plus `fpsLogIntervalMs` (5000, a przy `COM_SERVER_ON` 0, czyli bez okresowego logu)

Inny moduł kamery: wypełnij własną strukturę `CameraPins` i przypisz ją do `config.camera.pins`.

Zależności: `CamWebServer` / `CamComServer` / `CamTelnetServer` → `EspCamera` → `CamLed` / `CamSdCard` / `CamServos` (każdy opcjonalny). Serwery COM i Telnet korzystają z `CamCommands`. `CamWifi` jest niezależny.
Moduły dołącza się do kamery przez referencję, a nie tworzy wewnątrz niej. Dzięki temu kamera działa bez nich, a każdą klasę można też używać samodzielnie.

#### Usługi w `EspCamera`

| Usługa | Dołączenie | Metody |
|---|---|---|
| wszystkie | – | `features()` zwraca `{led, sd, servos, settings, ota}`: włączone w `EspCamFeatures.h` **i** poprawnie uruchomione (np. karta włożona) |
| lampa | `attachLed(CamLed&)` | `hasLed()`, `setLed(bool)`, `ledOn()` |
| karta SD | `attachSd(CamSdCard&)` | `hasSd()`, `savePhotoToSd(&bajty)` (zwraca ścieżkę pliku), `sdPhotoCount()`, `sdFreeBytes()`, `sdLastPhoto()`, `sdListPhotos(Print&, N)`, `sdListPhotoPage(strona, naStronę, fn)`, `sdReadPhoto(nazwa, sink)`, `sdDeletePhoto(nazwa)` |
| serwa | `attachServos(CamServos&)` | `hasServos()`, `setPan(deg)`, `setTilt(deg)`, `centerServos()`, `pan()`, `tilt()`, `servoLimits(...)` |
| ustawienia WiFi | `attachSettings(NvsSettings&)` | `hasWifiSettings()`, `wifiSettings(info)` (bez hasła), `saveWifiSettings(tryb, ssid, hasło)`, `clearWifiSettings()` |
| OTA | `attachOta(CamOta&)` | `hasOta()`, `otaInProgress()`, `otaInfo(info)`, `otaAuthorize(hasło)`, `otaBegin(rozmiar)`, `otaWrite(dane, długość)`, `otaEnd(ok)` |

Metody `attach…()` istnieją tylko wtedy, gdy moduł jest włączony. Pozostałe metody istnieją zawsze, a przy wyłączonym module zwracają `false`, 0 albo -1. Dzięki temu serwer nie potrzebuje żadnych `#if`.

### Przykłady

#### Jawne użycie klas

```cpp
#include <EspCamWeb.h>
using namespace espcamweb;

EspCamera camera;
CamLed led;
CamWifi wifi;
CamWebServer server(camera);   // serwer zależy tylko od kamery
#if SD_ON
CamSdCard sd;
#endif

void setup() {
  Serial.begin(115200);
  led.begin(LedSettings());
  camera.attachLed(led);   // lampa należy do kamery
  if (!camera.begin(CameraSettings())) ESP.restart();
#if SD_ON
  sd.begin(SdSettings());
  camera.attachSd(sd);     // brak karty -> hasSd() == false, strona ukryje przycisk
#endif

  WifiSettings net;
  net.ssid = "TwojaSiec";
  net.password = "TwojeHaslo";
  wifi.begin(net);
  server.begin(ServerSettings());
}

void loop() {
  camera.updateFps(millis());   // FPS widoczny w /status
  delay(100);
}
```

#### Sama kamera, bez serwera

```cpp
EspCamera camera;
camera.begin(CameraSettings());
camera_fb_t *fb = camera.capture();   // JPEG: fb->buf, fb->len
if (fb) { /* wysyłka, przetwarzanie... */ camera.release(fb); }
camera.savePhotoToSd();               // gdy dołączona karta (SD_ON)
camera.setPan(45);                    // gdy dołączone serwa (SERVO_MOTORS_ON)
```

#### Fasada

Zobacz `examples/Basic/main.cpp`: konfiguracja, `cam.begin(config)` w `setup()` i `cam.loop()` w `loop()`. Fasada sama tworzy i dołącza moduły włączone w `EspCamFeatures.h`. Brak karty SD nie jest błędem: usługa po prostu nie jest dostępna.

### Adresy HTTP

| Adres | Opis |
|---|---|
| `http://<IP>/` | strona z podglądem i sterowaniem |
| `http://<IP>/status` | JSON: `features` (`led`, `sd`, `servos`, `settings`, `ota`), fps, rozdzielczość, jakość, led, flip, RSSI, PSRAM, liczba klientów, port strumienia, `sdPhotos`, `sdFreeMB`, `sdLast`, `pan`, `tilt` |
| `http://<IP>/control?var=X&val=Y` | `framesize` (5 QVGA, 8 VGA, 9 SVGA, 10 XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1), `pan`, `tilt` (stopnie), `center` |
| `http://<IP>/capture` | pojedyncze zdjęcie JPEG |
| `http://<IP>/save` | zrób zdjęcie i zapisz na karcie SD; JSON `{"ok":true,"file":"/photos/IMG_00001.jpg"}`, a bez karty kod 503 |
| `http://<IP>/wifi` | strona ustawień WiFi (`WIFI_SETTINGS_ON`) |
| `GET http://<IP>/wifi/settings` | JSON: obecna sieć, `source` (saved/default), `fallback`, zapisane ustawienia (`mode`, `ssid`, `hasPassword`, nigdy samo hasło) |
| `POST http://<IP>/wifi/save` | pola formularza `mode` (sta/ap), `ssid`, `password`; walidacja, odpowiedź `{"ok":..,"message":".."}` |
| `POST http://<IP>/wifi/clear` | skasowanie zapisanych ustawień |
| `http://<IP>/photos` | przeglądarka zdjęć z karty SD (`SD_ON`): 100 na stronę, od najnowszych, otwieranie i usuwanie |
| `GET http://<IP>/photos/list?page=N` | JSON `{"items":[{"name","size"}],"total","page","pages","perPage"}`; 503 bez karty |
| `GET http://<IP>/photo?name=IMG_00001.jpg` | zdjęcie (image/jpeg), wysyłane z karty kawałkami po 4 KB |
| `POST http://<IP>/photo/delete?name=IMG_00001.jpg` | usunięcie zdjęcia; JSON `{"ok","message"}` |
| `http://<IP>/update` | strona aktualizacji programu (`OTA_ON`, HTTP Basic Auth z `OTA_PASSWORD`) |
| `POST http://<IP>/update` | surowy `firmware.bin` w treści; zapis do slotu OTA, weryfikacja, restart |
| `http://<IP>:81/stream` | strumień MJPEG (osobna instancja httpd) |

Uwagi:
- Strona pokazuje przycisk lampy, sekcję karty SD i sterowanie serwami (suwaki, strzałki, środek) tylko wtedy, gdy `/status` → `features` mówi, że dana usługa jest dostępna.
- `setFrameSize()` odrzuca rozdzielczość większą niż `maxFrameSize`, bo bufor by się przepełnił. Strona chowa takie przyciski.
- FPS liczy `EspCamera` na podstawie klatek pobranych przez `capture()`. Wartość odświeża `updateFps()`, które trzeba wywoływać w `loop()`; fasada robi to sama.
