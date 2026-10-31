# ESP32-CAM – live camera preview / podgląd kamery na żywo

**[English](#english)** | **[Polski](#polski)**

## English

PlatformIO project for the **ESP32-CAM AI Thinker** (OV2640, 4 MB PSRAM). It runs a web page with a live MJPEG preview, an FPS counter and camera controls.

All camera and server code lives in the reusable library **`lib/EspCamWeb`** (API and how to add it to other projects: [lib/EspCamWeb/README.md](lib/EspCamWeb/README.md#english)). `src/main.cpp` is only a short example that explicitly creates the camera (`EspCamera`), the optional modules, WiFi, the web server (`CamWebServer`) and the COM and Telnet command servers. The camera is the hub: modules are attached to it. The web page, COM and Telnet are equal front-ends that show only the services that are available.

- Framework: Arduino core 3.x ([pioarduino](https://github.com/pioarduino/platform-espressif32) platform, ESP-IDF 5.x)
- Server: `esp_http_server` from ESP-IDF, two instances:
  - port **80** – page, `/control`, `/status`, `/capture`, `/save`
  - port **81** – `/stream` (MJPEG), separate so the page never blocks the stream

### 0. Optional modules: `EspCamFeatures.h`

Services and their pins are set in a single file: [`lib/EspCamWeb/src/EspCamFeatures.h`](lib/EspCamWeb/src/EspCamFeatures.h). Both the library and the program read it.

| Entry | Default | What it does |
|---|---|---|
| `LED_ON` | 1 | flash LED lamp (GPIO4) |
| `SD_ON` | 1 | microSD card, “Save photo to SD” button, photos in `/photos/IMG_00001.jpg`… |
| `SERVO_MOTORS_ON` | 0 | two servos: horizontal `SERVO_PAN_PIN` (12) and vertical `SERVO_TILT_PIN` (13) |
| `COM_SERVER_ON` | 1 | text commands over the serial port (`pio device monitor`) |
| `TELNET_SERVER_ON` | 1 | the same commands over Telnet: `telnet esp32cam.local` (port `TELNET_PORT`, password `TELNET_PASSWORD`, no password by default) |
| `WIFI_SETTINGS_ON` | 1 | WiFi settings (STA/AP, SSID, password) saved in the board's NVS memory, editable on the `/wifi` page and with the `wifi` command |

A disabled module is not compiled and the page does not show its buttons. The buttons are also hidden when a module is enabled but not working (e.g. no card in the slot).

ESP32-CAM limitations:
- The SD card uses the on-board microSD slot through the hardware **SDMMC** interface (`SD_MMC` library), not SPI (`SD` library). **SDMMC pins are fixed in the ESP32 and cannot be changed:** CLK = GPIO14, CMD = GPIO15, D0 = GPIO2 (plus D1 = 4, D2 = 12, D3 = 13 in 4-bit mode). That is why they are not in `EspCamFeatures.h`. Details in the [library README](lib/EspCamWeb/README.md#sd-card-sdmmc-interface-not-spi).
- Free pins are very scarce. The SD card runs in 1-bit mode (GPIO 2, 14, 15) so that GPIO4 (lamp) and 12 and 13 (servos) stay free.
- If flashing the firmware fails, remove the SD card. A card on GPIO2 can prevent entering programming mode.
- **GPIO12 is a strapping pin:** at boot it selects the flash memory voltage. This normally works as is: GPIO12 has an internal pull-down and the input of a typical servo does not pull the line high. If the board does not boot with a servo connected, add a **10 kΩ resistor between GPIO12 and GND**. It forces the correct low level at boot and does not affect servo control. Permanent but **irreversible** fix: `espefuse.py set_flash_voltage 3.3V`.
- Power the servos separately from 5 V. Their current draw when moving causes resets (“brownout”) if it goes through the board.
- Pin collisions are checked at compile time (`#error` with a description).

### 1. WiFi setup

```bash
cp include/secrets.h.example include/secrets.h
```

Enter `WIFI_SSID` and `WIFI_PASS` in `include/secrets.h`. The file is in `.gitignore`.

If the board cannot connect within 15 s, it starts its own access point:
**ESP32-CAM**, password **12345678**, address `http://192.168.4.1`.

**Changing WiFi without reflashing** (`WIFI_SETTINGS_ON`): on the **WiFi settings** page (`http://<IP>/wifi`, linked from the main page) or with the commands `wifi set sta <ssid> [password]`, `wifi set ap <ssid> [password]`, `wifi clear yes`. The settings are saved in the board's NVS memory and survive flashing; they take effect after a **manual restart**.
- Saved STA: the board joins the saved network; if that fails within 15 s, it starts the default access point above.
- Saved AP: the board starts its own access point with the saved name and password; if that fails, it starts the default access point.
- Nothing saved (or after `wifi clear yes`): exactly as before, `secrets.h` and the default access point.
- An empty password means an open network; otherwise 8–63 characters. The password is never shown on the page, in command replies or in the logs.
- **Security:** the web page has no login - anyone on the local network can change the WiFi settings. Telnet can be protected with `TELNET_PASSWORD`.

### 2. Flashing

- **With the ESP32-CAM-MB board** (USB adapter): plug in the cable and upload; the board enters programming mode by itself.
- **With a USB-UART converter**: connect TX↔RX, RX↔TX, GND, 5 V. Before uploading, short **GPIO0 to GND** and press RESET. After uploading, disconnect GPIO0 and press RESET again.

```bash
pio run -t upload
pio device monitor
```

The monitor (115200) shows the IP address and the command server prompt (`> `). The `cam` and `status` commands show the frame rate. With `COM_SERVER_ON 0`, FPS is printed every 5 s.

### 3. Opening the page

- `http://<IP from the monitor>/` or `http://esp32cam.local/` (mDNS; Windows may need Bonjour)
- Raw stream: `http://<IP>:81/stream`
- Single snapshot: `http://<IP>/capture`
- State as JSON: `http://<IP>/status`
- Controls: `http://<IP>/control?var=<name>&val=<value>`, where name is `framesize` (5=QVGA, 8=VGA, 9=SVGA, 10=XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1), `pan`, `tilt` (degrees), `center`
- Photo to the SD card: `http://<IP>/save`
- WiFi settings: `http://<IP>/wifi`

The page, the COM/Telnet commands and the Serial logs are in English. The page shows two FPS counters: **browser** (frames actually received by the browser) and **server** (frames sent by the ESP32).

### 4. Commands over COM and Telnet

Connect with `pio device monitor` or `telnet esp32cam.local` and type `help`. The most important commands:
- `info`, `status`: board information and service state,
- `led on|off|toggle`: lamp,
- `servo`, `servo pan N`, `servo tilt N`, `servo N M`, `servo left|right|up|down [step]`, `servo center`: servos,
- `photo`: photo to the SD card,
- `sd`, `sd list [N]`: card state and recent photos,
- `cam`, `cam res vga`, `cam quality 15`: image settings,
- `wifi`: network state and saved settings,
- `wifi set sta <ssid> [password]`, `wifi set ap <ssid> [password]`, `wifi clear yes`: change or erase the saved WiFi settings (use "quotes" for spaces; restart to apply),
- `restart yes`: restart the board.

The full list is in the [library README](lib/EspCamWeb/README.md#command-servers-com-and-telnet). Commands of services disabled in `EspCamFeatures.h` are unavailable. Telnet does not encrypt the connection, so use it only on a trusted network.

### 5. FPS tips

| Resolution | Approximate (good WiFi) |
|---|---|
| QVGA 320×240 | 25+ fps |
| VGA 640×480 (default) | about 20–25 fps |
| SVGA 800×600 | about 15 fps |
| XGA 1024×768 | below 15 fps |

- **5 V supply, at least 1 A.** Weak power from a computer USB port causes resets (“brownout”) and image glitches.
- WiFi power saving is disabled in the code (`WiFi.setSleep(false)`). Without it FPS drops sharply.
- A higher `quality` value (e.g. 15–20) gives smaller frames and more FPS with a weak signal. RSSI below -70 dBm noticeably lowers the frame rate. An external antenna helps (requires moving the jumper resistor next to the IPEX connector).
- Each additional viewer shares the bandwidth. Watch the stream from one device if possible.
- The LED lamp (GPIO4) is deliberately dim, because full brightness heats the LED a lot.

Without PSRAM the program falls back to QVGA with a single buffer in DRAM.

---

## Polski

Projekt PlatformIO dla **ESP32-CAM AI Thinker** (OV2640, 4 MB PSRAM). Uruchamia stronę WWW z podglądem MJPEG na żywo, licznikiem FPS i sterowaniem kamerą.

Cały kod kamery i serwera jest w bibliotece wielokrotnego użytku **`lib/EspCamWeb`** (opis API i sposób dodania do innych projektów: [lib/EspCamWeb/README.md](lib/EspCamWeb/README.md#polski)). `src/main.cpp` to tylko krótki przykład, który jawnie tworzy kamerę (`EspCamera`), moduły opcjonalne, WiFi, serwer WWW (`CamWebServer`) oraz serwery poleceń COM i Telnet. Kamera jest centrum: moduły są do niej dołączane. Strona WWW, COM i Telnet to dodatki na równych prawach, które pokazują tylko dostępne usługi.

- Framework: Arduino core 3.x (platforma [pioarduino](https://github.com/pioarduino/platform-espressif32), ESP-IDF 5.x)
- Serwer: `esp_http_server` z ESP-IDF, dwie instancje:
  - port **80** – strona, `/control`, `/status`, `/capture`, `/save`
  - port **81** – `/stream` (MJPEG), osobno, żeby strona nie blokowała strumienia

### 0. Moduły opcjonalne: `EspCamFeatures.h`

Usługi i ich piny ustawia się w jednym pliku: [`lib/EspCamWeb/src/EspCamFeatures.h`](lib/EspCamWeb/src/EspCamFeatures.h). Czytają go zarówno biblioteka, jak i program.

| Wpis | Domyślnie | Co robi |
|---|---|---|
| `LED_ON` | 1 | lampa LED błysku (GPIO4) |
| `SD_ON` | 1 | karta microSD, przycisk „Save photo to SD”, zdjęcia w `/photos/IMG_00001.jpg`… |
| `SERVO_MOTORS_ON` | 0 | dwa serwa: poziom `SERVO_PAN_PIN` (12) i pion `SERVO_TILT_PIN` (13) |
| `COM_SERVER_ON` | 1 | polecenia tekstowe przez port szeregowy (`pio device monitor`) |
| `TELNET_SERVER_ON` | 1 | te same polecenia przez Telnet: `telnet esp32cam.local` (port `TELNET_PORT`, hasło `TELNET_PASSWORD`, domyślnie bez hasła) |
| `WIFI_SETTINGS_ON` | 1 | ustawienia WiFi (STA/AP, SSID, hasło) zapisane w pamięci NVS płytki, do zmiany na stronie `/wifi` i poleceniem `wifi` |

Wyłączony moduł nie jest kompilowany, a strona nie pokazuje jego przycisków. Przycisków nie ma też wtedy, gdy moduł jest włączony, ale nie działa (np. brak karty w slocie).

Ograniczenia ESP32-CAM:
- Karta SD korzysta z gniazda microSD na płytce przez sprzętowy interfejs **SDMMC** (biblioteka `SD_MMC`), a nie przez SPI (biblioteka `SD`). **Piny SDMMC są w ESP32 stałe i nie da się ich zmienić:** CLK = GPIO14, CMD = GPIO15, D0 = GPIO2 (oraz D1 = 4, D2 = 12, D3 = 13 w trybie 4-bit). Dlatego nie ma ich w `EspCamFeatures.h`. Szczegóły są w [README biblioteki](lib/EspCamWeb/README.md#karta-sd-interfejs-sdmmc-nie-spi).
- Wolnych pinów jest bardzo mało. Karta SD pracuje w trybie 1-bit (GPIO 2, 14, 15), żeby zostały wolne GPIO4 (lampa) oraz 12 i 13 (serwa).
- Jeśli wgrywanie firmware się nie udaje, wyjmij kartę SD. Karta na GPIO2 może zakłócać wejście w tryb programowania.
- **GPIO12 to pin strapping:** przy starcie wybiera napięcie pamięci flash. Zwykle działa bez zmian: GPIO12 ma wewnętrzny rezystor ściągający, a wejście typowego serwa nie podciąga linii do plusa. Gdyby płytka nie startowała z podłączonym serwem, dodaj **rezystor 10 kΩ między GPIO12 a GND**. Wymusza on właściwy stan niski przy starcie i nie przeszkadza w sterowaniu serwem. Rozwiązanie trwałe, ale **nieodwracalne**: `espefuse.py set_flash_voltage 3.3V`.
- Serwa zasilaj osobno z 5 V. Pobór prądu przez serwo przy ruchu powoduje restarty („brownout”), jeśli idzie przez płytkę.
- Kolizje pinów są sprawdzane przy kompilacji (`#error` z opisem).

### 1. Ustawienie WiFi

```bash
cp include/secrets.h.example include/secrets.h
```

W pliku `include/secrets.h` wpisz `WIFI_SSID` i `WIFI_PASS`. Plik jest w `.gitignore`.

Jeśli płytka nie połączy się z siecią w ciągu 15 s, uruchomi własny punkt dostępowy:
**ESP32-CAM**, hasło **12345678**, adres `http://192.168.4.1`.

**Zmiana WiFi bez ponownego wgrywania** (`WIFI_SETTINGS_ON`): na stronie **WiFi settings** (`http://<IP>/wifi`, link na stronie głównej) albo poleceniami `wifi set sta <ssid> [hasło]`, `wifi set ap <ssid> [hasło]`, `wifi clear yes`. Ustawienia trafiają do pamięci NVS płytki i przetrwają wgranie nowego programu. Działają po **ręcznym restarcie**.
- Zapisany tryb STA: płytka łączy się z zapisaną siecią. Jeśli się nie uda w ciągu 15 s, uruchamia domyślny punkt dostępowy opisany wyżej.
- Zapisany tryb AP: płytka uruchamia własny punkt dostępowy z zapisaną nazwą i hasłem. Przy błędzie uruchamia domyślny punkt dostępowy.
- Brak zapisanych ustawień (albo po `wifi clear yes`): dokładnie jak dotąd, czyli `secrets.h` i domyślny punkt dostępowy.
- Puste hasło oznacza sieć otwartą, a w przeciwnym razie hasło musi mieć 8–63 znaki. Hasło nigdy nie jest pokazywane na stronie, w odpowiedziach poleceń ani w logach.
- **Bezpieczeństwo:** strona nie ma logowania, więc każdy w sieci lokalnej może zmienić ustawienia WiFi. Telnet można zabezpieczyć hasłem `TELNET_PASSWORD`.

### 2. Wgrywanie

- **Z płytką ESP32-CAM-MB** (nakładka z USB): podłącz kabel i wgraj, płytka sama przejdzie w tryb programowania.
- **Z konwerterem USB-UART**: połącz TX↔RX, RX↔TX, GND, 5 V. Przed wgrywaniem zewrzyj **GPIO0 z GND** i naciśnij RESET. Po wgraniu rozłącz GPIO0 i znów naciśnij RESET.

```bash
pio run -t upload
pio device monitor
```

Na monitorze (115200) pojawi się adres IP i znak zachęty serwera poleceń (`> `). Liczbę klatek pokazują polecenia `cam` i `status`. Przy `COM_SERVER_ON 0` FPS jest wypisywany co 5 s.

### 3. Otwarcie strony

- `http://<IP z monitora>/` albo `http://esp32cam.local/` (mDNS; na Windows może wymagać Bonjour)
- Surowy strumień: `http://<IP>:81/stream`
- Pojedyncze zdjęcie: `http://<IP>/capture`
- Stan w JSON: `http://<IP>/status`
- Sterowanie: `http://<IP>/control?var=<nazwa>&val=<wartość>`, gdzie nazwa to `framesize` (5=QVGA, 8=VGA, 9=SVGA, 10=XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1), `pan`, `tilt` (stopnie), `center`
- Zdjęcie na kartę SD: `http://<IP>/save`
- Ustawienia WiFi: `http://<IP>/wifi`

Strona, polecenia COM/Telnet i logi na Serial są po angielsku. Na stronie widać dwa liczniki FPS: **browser** (klatki faktycznie odebrane przez przeglądarkę) i **server** (klatki wysłane przez ESP32).

### 4. Polecenia przez COM i Telnet

Połącz się przez `pio device monitor` albo `telnet esp32cam.local` i wpisz `help`. Najważniejsze polecenia:
- `info`, `status`: informacje o płytce i stanie usług,
- `led on|off|toggle`: lampa,
- `servo`, `servo pan N`, `servo tilt N`, `servo N M`, `servo left|right|up|down [krok]`, `servo center`: serwa,
- `photo`: zdjęcie na kartę SD,
- `sd`, `sd list [N]`: stan karty i ostatnie zdjęcia,
- `cam`, `cam res vga`, `cam quality 15`: ustawienia obrazu,
- `wifi`: stan sieci i zapisane ustawienia,
- `wifi set sta <ssid> [hasło]`, `wifi set ap <ssid> [hasło]`, `wifi clear yes`: zmiana lub skasowanie zapisanych ustawień WiFi (spacje w „cudzysłowie”; działa po restarcie),
- `restart yes`: restart płytki.

Pełna lista jest w [README biblioteki](lib/EspCamWeb/README.md#serwery-poleceń-com-i-telnet). Polecenia usług wyłączonych w `EspCamFeatures.h` są niedostępne. Telnet nie szyfruje połączenia, więc używaj go tylko w zaufanej sieci.

### 5. Wskazówki dotyczące FPS

| Rozdzielczość | Orientacyjnie (dobre WiFi) |
|---|---|
| QVGA 320×240 | 25+ kl./s |
| VGA 640×480 (domyślna) | ok. 20–25 kl./s |
| SVGA 800×600 | ok. 15 kl./s |
| XGA 1024×768 | poniżej 15 kl./s |

- **Zasilanie 5 V, min. 1 A.** Słabe zasilanie z USB komputera daje restarty („brownout”) i zakłócenia obrazu.
- Usypianie WiFi jest wyłączone w kodzie (`WiFi.setSleep(false)`). Bez tego FPS mocno spada.
- Wyższa wartość `quality` (np. 15–20) daje mniejsze klatki i więcej FPS przy słabym sygnale. RSSI poniżej -70 dBm wyraźnie obniża liczbę klatek. Pomaga zewnętrzna antena (wymaga przelutowania zworki przy złączu IPEX).
- Każdy dodatkowy oglądający dzieli przepustowość. Najlepiej oglądać strumień z jednego urządzenia.
- Lampa LED (GPIO4) świeci celowo słabo, bo pełna jasność mocno grzeje diodę.

Bez PSRAM program przechodzi w tryb awaryjny: tylko QVGA, jeden bufor w DRAM.
