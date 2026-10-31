# EspCamWeb

Biblioteka PlatformIO do podglądu kamery **ESP32-CAM** na żywo przez WWW: strumień MJPEG, liczniki FPS, zmiana rozdzielczości i jakości, flip/lustro, lampa LED.
Domyślnie skonfigurowana pod **AI Thinker (OV2640, 4 MB PSRAM)**, ale piny i wszystkie parametry można zmienić.

Wymaga Arduino core 3.x (platforma pioarduino). Moduł `esp_camera` jest wbudowany w core, więc nie trzeba dodawać zewnętrznej biblioteki kamery.

## Dodanie do innego projektu

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

### Wymagane ustawienia `platformio.ini`

```ini
[env:esp32cam]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32cam
framework = arduino
monitor_speed = 115200
board_build.partitions = huge_app.csv      ; aplikacja ~1,1 MB nie mieści się w domyślnej partycji
build_flags =
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
```

## Klasy

Każdej klasy można używać samodzielnie. Wszystko jest w przestrzeni nazw `espcamweb`, a jeden nagłówek `#include <EspCamWeb.h>` dołącza całość.

| Klasa | Plik | Rola |
|---|---|---|
| `EspCamera` | `EspCamera.h` | Sama kamera: inicjalizacja (z trybem awaryjnym bez PSRAM), `capture()`/`release()`, rozdzielczość, jakość, flip, lustro, FPS. Nie potrzebuje WiFi ani serwera. |
| `CamWebServer` | `CamWebServer.h` | Serwer WWW (esp_http_server). Dostaje referencję do `EspCamera` i opcjonalnie wskaźnik do `CamLed`. |
| `CamWifi` | `CamWifi.h` | WiFi STA bez usypiania, mDNS i awaryjny punkt dostępowy. |
| `CamLed` | `CamLed.h` | Lampa LED przez `ledcAttachChannel` (domyślnie GPIO4, kanał 2, słaba jasność). `pin = -1` wyłącza lampę. |
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
- **`ServerSettings`**: `httpPort` 80, `streamPort` 81
- **`EspCamWebConfig`**: wszystkie powyższe plus `fpsLogIntervalMs` (5000, a 0 wyłącza wypisywanie FPS)

Inny moduł kamery: wypełnij własną strukturę `CameraPins` i przypisz ją do `config.camera.pins`.

## Przykłady

### Jawne użycie klas

```cpp
#include <EspCamWeb.h>
using namespace espcamweb;

EspCamera camera;
CamLed led;
CamWifi wifi;
CamWebServer server(camera, &led);   // serwer używa tej samej kamery

void setup() {
  Serial.begin(115200);
  led.begin(LedSettings());
  if (!camera.begin(CameraSettings())) ESP.restart();

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

### Sama kamera, bez serwera

```cpp
EspCamera camera;
camera.begin(CameraSettings());
camera_fb_t *fb = camera.capture();   // JPEG: fb->buf, fb->len
if (fb) { /* zapis na kartę SD, wysyłka... */ camera.release(fb); }
```

### Fasada

Zobacz `examples/Basic/main.cpp`: konfiguracja, `cam.begin(config)` w `setup()` i `cam.loop()` w `loop()`.

## Adresy HTTP

| Adres | Opis |
|---|---|
| `http://<IP>/` | strona z podglądem i sterowaniem |
| `http://<IP>/status` | JSON: fps, rozdzielczość, jakość, led, flip, RSSI, PSRAM, liczba klientów, port strumienia |
| `http://<IP>/control?var=X&val=Y` | `framesize` (5 QVGA, 8 VGA, 9 SVGA, 10 XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1) |
| `http://<IP>/capture` | pojedyncze zdjęcie JPEG |
| `http://<IP>:81/stream` | strumień MJPEG (osobna instancja httpd) |

Uwagi:
- `setFrameSize()` odrzuca rozdzielczość większą niż `maxFrameSize`, bo bufor by się przepełnił. Strona chowa takie przyciski.
- FPS liczy `EspCamera` na podstawie klatek pobranych przez `capture()`. Wartość odświeża `updateFps()`, które trzeba wywoływać w `loop()`; fasada robi to sama.
