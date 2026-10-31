# ESP32-CAM – podgląd kamery na żywo

Projekt PlatformIO dla **ESP32-CAM AI Thinker** (OV2640, 4 MB PSRAM). Uruchamia stronę WWW z podglądem MJPEG na żywo, licznikiem FPS i sterowaniem kamerą.

Cały kod kamery i serwera jest w bibliotece wielokrotnego użytku **`lib/EspCamWeb`** (opis API i sposób dodania do innych projektów: [lib/EspCamWeb/README.md](lib/EspCamWeb/README.md)). `src/main.cpp` to tylko krótki przykład, który jawnie tworzy kamerę (`EspCamera`), lampę, WiFi i serwer WWW (`CamWebServer`).

- Framework: Arduino core 3.x (platforma [pioarduino](https://github.com/pioarduino/platform-espressif32), ESP-IDF 5.x)
- Serwer: `esp_http_server` z ESP-IDF, dwie instancje:
  - port **80** – strona, `/control`, `/status`, `/capture`
  - port **81** – `/stream` (MJPEG), osobno, żeby strona nie blokowała strumienia

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

Na monitorze (115200) pojawi się adres IP, a co 5 s liczba klatek na sekundę.

## 3. Otwarcie strony

- `http://<IP z monitora>/` albo `http://esp32cam.local/` (mDNS; na Windows może wymagać Bonjour)
- Surowy strumień: `http://<IP>:81/stream`
- Pojedyncze zdjęcie: `http://<IP>/capture`
- Stan w JSON: `http://<IP>/status`
- Sterowanie: `http://<IP>/control?var=<nazwa>&val=<wartość>`, gdzie nazwa to `framesize` (5=QVGA, 8=VGA, 9=SVGA, 10=XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1)

Na stronie widać dwa liczniki FPS: **przeglądarka** (klatki faktycznie odebrane) i **serwer** (klatki wysłane przez ESP32).

## 4. Wskazówki dotyczące FPS

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
