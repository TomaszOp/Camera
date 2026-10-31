# ESP32-CAM – podgląd kamery na żywo

Projekt PlatformIO dla **ESP32-CAM AI Thinker** (OV2640, 4 MB PSRAM). Uruchamia stronę WWW z podglądem MJPEG na żywo, licznikiem FPS i sterowaniem kamerą.

Cały kod kamery i serwera jest w bibliotece wielokrotnego użytku **`lib/EspCamWeb`** (opis API i sposób dodania do innych projektów: [lib/EspCamWeb/README.md](lib/EspCamWeb/README.md)). `src/main.cpp` to tylko krótki przykład, który jawnie tworzy kamerę (`EspCamera`), moduły opcjonalne, WiFi, serwer WWW (`CamWebServer`) oraz serwery poleceń COM i Telnet. Kamera jest centrum: moduły są do niej dołączane. Strona WWW, COM i Telnet to dodatki na równych prawach, które pokazują tylko dostępne usługi.

- Framework: Arduino core 3.x (platforma [pioarduino](https://github.com/pioarduino/platform-espressif32), ESP-IDF 5.x)
- Serwer: `esp_http_server` z ESP-IDF, dwie instancje:
  - port **80** – strona, `/control`, `/status`, `/capture`, `/save`
  - port **81** – `/stream` (MJPEG), osobno, żeby strona nie blokowała strumienia

## 0. Moduły opcjonalne: `EspCamFeatures.h`

Usługi i ich piny ustawia się w jednym pliku: [`lib/EspCamWeb/src/EspCamFeatures.h`](lib/EspCamWeb/src/EspCamFeatures.h). Czytają go zarówno biblioteka, jak i program.

| Wpis | Domyślnie | Co robi |
|---|---|---|
| `LED_ON` | 1 | lampa LED błysku (GPIO4) |
| `SD_ON` | 1 | karta microSD, przycisk „Save photo to SD”, zdjęcia w `/photos/IMG_00001.jpg`… |
| `SERVO_MOTORS_ON` | 0 | dwa serwa: poziom `SERVO_PAN_PIN` (12) i pion `SERVO_TILT_PIN` (13) |
| `COM_SERVER_ON` | 1 | polecenia tekstowe przez port szeregowy (`pio device monitor`) |
| `TELNET_SERVER_ON` | 1 | te same polecenia przez Telnet: `telnet esp32cam.local` (port `TELNET_PORT`, hasło `TELNET_PASSWORD`, domyślnie bez hasła) |

Wyłączony moduł nie jest kompilowany, a strona nie pokazuje jego przycisków. Przycisków nie ma też wtedy, gdy moduł jest włączony, ale nie działa (np. brak karty w slocie).

Ograniczenia ESP32-CAM:
- Karta SD korzysta z gniazda microSD na płytce przez sprzętowy interfejs **SDMMC** (biblioteka `SD_MMC`), a nie przez SPI (biblioteka `SD`). **Piny SDMMC są w ESP32 stałe i nie da się ich zmienić:** CLK = GPIO14, CMD = GPIO15, D0 = GPIO2 (oraz D1 = 4, D2 = 12, D3 = 13 w trybie 4-bit). Dlatego nie ma ich w `EspCamFeatures.h`. Szczegóły są w [README biblioteki](lib/EspCamWeb/README.md#karta-sd-interfejs-sdmmc-nie-spi).
- Wolnych pinów jest bardzo mało. Karta SD pracuje w trybie 1-bit (GPIO 2, 14, 15), żeby zostały wolne GPIO4 (lampa) oraz 12 i 13 (serwa).
- Jeśli wgrywanie firmware się nie udaje, wyjmij kartę SD. Karta na GPIO2 może zakłócać wejście w tryb programowania.
- **GPIO12 to pin strapping:** przy starcie wybiera napięcie pamięci flash. Zwykle działa bez zmian: GPIO12 ma wewnętrzny rezystor ściągający, a wejście typowego serwa nie podciąga linii do plusa. Gdyby płytka nie startowała z podłączonym serwem, dodaj **rezystor 10 kΩ między GPIO12 a GND**. Wymusza on właściwy stan niski przy starcie i nie przeszkadza w sterowaniu serwem. Rozwiązanie trwałe, ale **nieodwracalne**: `espefuse.py set_flash_voltage 3.3V`.
- Serwa zasilaj osobno z 5 V. Pobór prądu przez serwo przy ruchu powoduje restarty („brownout”), jeśli idzie przez płytkę.
- Kolizje pinów są sprawdzane przy kompilacji (`#error` z opisem).

## 1. Ustawienie WiFi

```bash
cp include/secrets.h.example include/secrets.h
```

W pliku `include/secrets.h` wpisz `WIFI_SSID` i `WIFI_PASS`. Plik jest w `.gitignore`.

Jeśli płytka nie połączy się z siecią w ciągu 15 s, uruchomi własny punkt dostępowy:
**ESP32-CAM**, hasło **12345678**, adres `http://192.168.4.1`.

## 2. Wgrywanie

- **Z płytką ESP32-CAM-MB** (nakładka z USB): podłącz kabel i wgraj, płytka sama przejdzie w tryb programowania.
- **Z konwerterem USB-UART**: połącz TX↔RX, RX↔TX, GND, 5 V. Przed wgrywaniem zewrzyj **GPIO0 z GND** i naciśnij RESET. Po wgraniu rozłącz GPIO0 i znów naciśnij RESET.

```bash
pio run -t upload
pio device monitor
```

Na monitorze (115200) pojawi się adres IP i znak zachęty serwera poleceń (`> `). Liczbę klatek pokazują polecenia `cam` i `status`. Przy `COM_SERVER_ON 0` FPS jest wypisywany co 5 s.

## 3. Otwarcie strony

- `http://<IP z monitora>/` albo `http://esp32cam.local/` (mDNS; na Windows może wymagać Bonjour)
- Surowy strumień: `http://<IP>:81/stream`
- Pojedyncze zdjęcie: `http://<IP>/capture`
- Stan w JSON: `http://<IP>/status`
- Sterowanie: `http://<IP>/control?var=<nazwa>&val=<wartość>`, gdzie nazwa to `framesize` (5=QVGA, 8=VGA, 9=SVGA, 10=XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1), `pan`, `tilt` (stopnie), `center`
- Zdjęcie na kartę SD: `http://<IP>/save`

Strona, polecenia COM/Telnet i logi na Serial są po angielsku. Na stronie widać dwa liczniki FPS: **browser** (klatki faktycznie odebrane przez przeglądarkę) i **server** (klatki wysłane przez ESP32).

## 4. Polecenia przez COM i Telnet

Połącz się przez `pio device monitor` albo `telnet esp32cam.local` i wpisz `help`. Najważniejsze polecenia:
- `info`, `status`: informacje o płytce i stanie usług,
- `led on|off|toggle`: lampa,
- `servo`, `servo pan N`, `servo tilt N`, `servo N M`, `servo left|right|up|down [krok]`, `servo center`: serwa,
- `photo`: zdjęcie na kartę SD,
- `sd`, `sd list [N]`: stan karty i ostatnie zdjęcia,
- `cam`, `cam res vga`, `cam quality 15`: ustawienia obrazu,
- `wifi`: stan sieci,
- `restart yes`: restart płytki.

Pełna lista jest w [README biblioteki](lib/EspCamWeb/README.md#serwery-poleceń-com-i-telnet). Polecenia usług wyłączonych w `EspCamFeatures.h` są niedostępne. Telnet nie szyfruje połączenia, więc używaj go tylko w zaufanej sieci.

## 5. Wskazówki dotyczące FPS

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
