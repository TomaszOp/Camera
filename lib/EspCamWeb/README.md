# EspCamWeb

Biblioteka PlatformIO do podglądu kamery **ESP32-CAM** na żywo przez WWW: strumień MJPEG, liczniki FPS, zmiana rozdzielczości i jakości, flip/lustro. Opcjonalnie: lampa LED, zapis zdjęć na kartę microSD i serwa pan/tilt.
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
    -Iinclude                              ; potrzebne, jeśli projekt ma include/config.h
```

## Moduły opcjonalne: `config.h`

Moduły włącza się w pliku `include/config.h` projektu (1 = włączony, 0 = wyłączony). Kod wyłączonego modułu w ogóle nie jest kompilowany.

```cpp
#pragma once
#define LED_ON          1   // lampa LED błysku          (CamLed)
#define SD_ON           0   // karta microSD na zdjęcia  (CamSdCard)
#define SERVO_MOTORS_ON 0   // serwa poziom/pion         (CamServos)

#define LED_PIN        4
#define SERVO_PAN_PIN  12
#define SERVO_TILT_PIN 13
```

- Bez `config.h` obowiązują wartości domyślne: LED włączony, SD i serwa wyłączone.
- **Ważne:** PlatformIO nie przekazuje katalogu `include/` projektu do bibliotek z `lib/`. Dlatego w `build_flags` musi być `-Iinclude`. Inaczej biblioteka i program zbudowałyby się z różnymi ustawieniami. Taką niezgodność wykrywa linker i zgłasza błąd w rodzaju `undefined reference to 'espcamweb_config_led1_sd1_servo0'`.
- `EspCamFeatures.h` sprawdza przy kompilacji kolizje pinów: z kamerą, z kartą SD, LED z serwem, pan z tilt oraz piny 34–39, które są tylko wejściami. Kolizja daje błąd `#error` z opisem.

### Przydział pinów i kanałów LEDC (AI Thinker)

| Zasób | Piny | LEDC |
|---|---|---|
| Kamera | 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39 (PSRAM: 16, 17) | kanał 0 / timer 0 (XCLK 20 MHz) |
| Lampa LED | 4 | kanał 2 / timer 1 (5 kHz) |
| Karta SD (1-bit) | 2 (D0), 14 (CLK), 15 (CMD) | – |
| Serwo pan / tilt | 12 / 13 | kanały 4 i 5 / timer 2 (50 Hz) |

- Karta pracuje w trybie **1-bit**. W trybie 4-bit zajęłaby też GPIO 4, 12 i 13, czyli piny lampy i serw.
- **GPIO12 to pin strapping**, który ustala napięcie pamięci flash. Jeśli serwo podciągnie go do stanu wysokiego przy starcie, płytka się nie uruchomi. Wtedy zamień piny serw albo podłącz serwo przez rezystor lub tranzystor. Serwa zasilaj osobno z 5 V, nie z płytki.
- Do serw nie używam biblioteki ESP32Servo, bo sama przydziela timery LEDC i koliduje z kamerą.
## Klasy

Każdej klasy można używać samodzielnie. Wszystko jest w przestrzeni nazw `espcamweb`, a jeden nagłówek `#include <EspCamWeb.h>` dołącza całość.

| Klasa | Plik | Rola |
|---|---|---|
| `EspCamera` | `EspCamera.h` | **Centrum.** Kamera: inicjalizacja (z trybem awaryjnym bez PSRAM), `capture()`/`release()`, rozdzielczość, jakość, flip, lustro, FPS. Wszystkie usługi opcjonalne idą przez nią (tabela niżej). Nie potrzebuje WiFi ani serwera. |
| `CamWebServer` | `CamWebServer.h` | Serwer WWW (esp_http_server), czyli dodatek. Zależy wyłącznie od `EspCamera` i pokazuje tylko te usługi, które kamera udostępnia. |
| `CamWifi` | `CamWifi.h` | WiFi STA bez usypiania, mDNS i awaryjny punkt dostępowy. |
| `CamLed` | `CamLed.h` | `LED_ON`. Lampa LED przez `ledcAttachChannel` (GPIO4, kanał 2, słaba jasność). |
| `CamSdCard` | `CamSdCard.h` | `SD_ON`. Karta microSD (SD_MMC 1-bit): montowanie, wolne miejsce, zapis JPEG jako `/photos/IMG_00001.jpg`… Numeracja jest kontynuowana po restarcie, bo płytka nie ma zegara RTC. |
| `CamServos` | `CamServos.h` | `SERVO_MOTORS_ON`. Dwa serwa: pan (poziom) i tilt (pion), PWM 50 Hz, kąty 0–180° z limitami. |
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
  - `panPin`, `tiltPin`: z `config.h`
  - `panChannel` 4, `tiltChannel` 5
  - `minPulseUs` 500, `maxPulseUs` 2400
  - zakresy `panMin`/`panMax`, `tiltMin`/`tiltMax`
  - pozycja startowa `panStart`/`tiltStart`: 90°
  - odwrócenie kierunku: `invertPan`, `invertTilt`
- **`ServerSettings`**: `httpPort` 80, `streamPort` 81
- **`EspCamWebConfig`**: wszystkie powyższe plus `fpsLogIntervalMs` (5000, a 0 wyłącza wypisywanie FPS)

Inny moduł kamery: wypełnij własną strukturę `CameraPins` i przypisz ją do `config.camera.pins`.

Zależności: `CamWebServer` → `EspCamera` → `CamLed` / `CamSdCard` / `CamServos` (każdy opcjonalny). `CamWifi` jest niezależny.
Moduły dołącza się do kamery przez referencję, a nie tworzy wewnątrz niej. Dzięki temu kamera działa bez nich, a każdą klasę można też używać samodzielnie.

### Usługi w `EspCamera`

| Usługa | Dołączenie | Metody |
|---|---|---|
| wszystkie | – | `features()` zwraca `{led, sd, servos}`: włączone w `config.h` **i** poprawnie uruchomione (np. karta włożona) |
| lampa | `attachLed(CamLed&)` | `hasLed()`, `setLed(bool)`, `ledOn()` |
| karta SD | `attachSd(CamSdCard&)` | `hasSd()`, `savePhotoToSd()` (zwraca ścieżkę pliku), `sdPhotoCount()`, `sdFreeBytes()`, `sdLastPhoto()` |
| serwa | `attachServos(CamServos&)` | `hasServos()`, `setPan(deg)`, `setTilt(deg)`, `centerServos()`, `pan()`, `tilt()` |

Metody `attach…()` istnieją tylko wtedy, gdy moduł jest włączony. Pozostałe metody istnieją zawsze, a przy wyłączonym module zwracają `false`, 0 albo -1. Dzięki temu serwer nie potrzebuje żadnych `#if`.

## Przykłady

### Jawne użycie klas

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

### Sama kamera, bez serwera

```cpp
EspCamera camera;
camera.begin(CameraSettings());
camera_fb_t *fb = camera.capture();   // JPEG: fb->buf, fb->len
if (fb) { /* wysyłka, przetwarzanie... */ camera.release(fb); }
camera.savePhotoToSd();               // gdy dołączona karta (SD_ON)
camera.setPan(45);                    // gdy dołączone serwa (SERVO_MOTORS_ON)
```

### Fasada

Zobacz `examples/Basic/main.cpp`: konfiguracja, `cam.begin(config)` w `setup()` i `cam.loop()` w `loop()`. Fasada sama tworzy i dołącza moduły włączone w `config.h`. Brak karty SD nie jest błędem: usługa po prostu nie jest dostępna.

## Adresy HTTP

| Adres | Opis |
|---|---|
| `http://<IP>/` | strona z podglądem i sterowaniem |
| `http://<IP>/status` | JSON: `features` (`led`, `sd`, `servos`), fps, rozdzielczość, jakość, led, flip, RSSI, PSRAM, liczba klientów, port strumienia, `sdPhotos`, `sdFreeMB`, `sdLast`, `pan`, `tilt` |
| `http://<IP>/control?var=X&val=Y` | `framesize` (5 QVGA, 8 VGA, 9 SVGA, 10 XGA), `quality` (4–63), `led`, `vflip`, `hmirror` (0/1), `pan`, `tilt` (stopnie), `center` |
| `http://<IP>/capture` | pojedyncze zdjęcie JPEG |
| `http://<IP>/save` | zrób zdjęcie i zapisz na karcie SD; JSON `{"ok":true,"file":"/photos/IMG_00001.jpg"}`, a bez karty kod 503 |
| `http://<IP>:81/stream` | strumień MJPEG (osobna instancja httpd) |

Uwagi:
- Strona pokazuje przycisk lampy, sekcję karty SD i sterowanie serwami (suwaki, strzałki, środek) tylko wtedy, gdy `/status` → `features` mówi, że dana usługa jest dostępna.
- `setFrameSize()` odrzuca rozdzielczość większą niż `maxFrameSize`, bo bufor by się przepełnił. Strona chowa takie przyciski.
- FPS liczy `EspCamera` na podstawie klatek pobranych przez `capture()`. Wartość odświeża `updateFps()`, które trzeba wywoływać w `loop()`; fasada robi to sama.
