# EspCamWeb

Biblioteka PlatformIO do podglądu kamery **ESP32-CAM** na żywo przez WWW: strumień MJPEG, liczniki FPS, zmiana rozdzielczości i jakości, flip/lustro. Opcjonalnie: lampa LED, zapis zdjęć na kartę microSD, serwa pan/tilt oraz sterowanie poleceniami tekstowymi przez port szeregowy (COM) i Telnet.
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
lib_deps =
    lennarthennigs/ESP Telnet@^2.3.0       ; tylko dla TELNET_SERVER_ON (dociąga się też z library.json)
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
#define COM_SERVER_ON    0  // polecenia przez port szeregowy (CamComServer)
#define TELNET_SERVER_ON 0  // polecenia przez Telnet         (CamTelnetServer)
#define TELNET_PORT      23
#define TELNET_PASSWORD  "" // "" = bez hasła

#define LED_PIN        4
#define SERVO_PAN_PIN  12
#define SERVO_TILT_PIN 13
```

- Bez `config.h` obowiązują wartości domyślne: LED włączony, a SD, serwa oraz serwery COM i Telnet wyłączone.
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

### Karta SD: interfejs SDMMC, nie SPI

`CamSdCard` korzysta ze sprzętowego kontrolera **SDMMC** przez bibliotekę `SD_MMC` z core. Nie używa biblioteki `SD` (`SD.h`), która działa przez **SPI**. SDMMC jest szybszy i nie zajmuje magistrali SPI. Tak jest też podłączone gniazdo microSD na płytce ESP32-CAM.

**Piny są stałe i nie da się ich zmienić.** W klasycznym ESP32 slot 1 kontrolera SDMMC jest na stałe połączony z konkretnymi nóżkami. Dlatego piny karty nie występują w `config.h` ani w `SdSettings`. Definiuje je ESP-IDF w pliku `soc/esp32/include/soc/sdmmc_pins.h` (`SDMMC_SLOT1_IOMUX_PIN_NUM_*`), a `SD_MMC.begin()` używa ich automatycznie.

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
- Na ESP32-S3 piny SDMMC można wybrać dowolnie (`SD_MMC.setPins()`), ale ta biblioteka obecnie tego nie obsługuje.
- GPIO2 musi być w stanie niskim lub nieustalonym przy wgrywaniu firmware. Karta włożona w gniazdo czasem to zakłóca. Jeśli wgrywanie się nie udaje, wyjmij kartę na ten czas.
- **GPIO12 to pin strapping**, który przy starcie ustala napięcie pamięci flash (stan niski = 3,3 V, prawidłowo). Typowe serwo (SG90, MG90S) tego nie zakłóca, bo jego wejście nie podciąga linii do plusa. Gdyby płytka mimo to nie startowała z podłączonym serwem, dodaj **rezystor 10 kΩ między GPIO12 a GND**. Wymusza on stan niski przy starcie i nie przeszkadza w sterowaniu serwem. Rozwiązanie trwałe, ale **nieodwracalne**: `espefuse.py set_flash_voltage 3.3V` ustawia napięcie flash w eFuse, a wtedy GPIO12 przestaje być pinem strapping.
- Serwa zasilaj osobno z 5 V, nie z płytki.
- Do serw nie używam biblioteki ESP32Servo, bo sama przydziela timery LEDC i koliduje z kamerą.

## Serwery poleceń: COM i Telnet

Oba serwery są, tak jak strona WWW, dodatkami korzystającymi wyłącznie z `EspCamera`. Polecenia wykonuje wspólna klasa `CamCommands`, więc na COM i przez Telnet są identyczne.
- Słowa kluczowe są po angielsku, odpowiedzi po polsku, a wielkość liter nie ma znaczenia.
- Polecenie usługi niedostępnej (wyłączonej w `config.h` albo niedziałającej, np. bez karty) zwraca komunikat „Usługa niedostępna”, a `help` go nie pokazuje.

| Polecenie | Usługa | Działanie |
|---|---|---|
| `help` / `?` | – | lista dostępnych poleceń |
| `info` | – | wersja, czas pracy, wolny heap/PSRAM, WiFi, lista usług |
| `status` | – | skrót stanu: kamera, lampa, serwa, karta SD |
| `wifi` | – | tryb STA/AP, SSID, IP, RSSI |
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
| `restart yes` | – | restart płytki (wymaga potwierdzenia `yes`) |
| `quit` / `exit` | – | rozłączenie (Telnet) |

### Połączenie

- **COM:** `pio device monitor` (115200). Serwer korzysta z tego samego portu `Serial`, na który idą logi. Przy włączonym serwerze COM okresowy log FPS jest domyślnie wyłączony, żeby nie przerywał pisania. FPS pokazują polecenia `cam` i `status`. Serwer odsyła wpisywane znaki (echo) i obsługuje Backspace.
- **Telnet:** `telnet esp32cam.local` albo `telnet <IP>`; na Windows np. PuTTY w trybie Telnet. Działa w trybie STA i AP.

```text
$ telnet esp32cam.local
EspCamWeb 1.1.0 - ESP32-CAM
Wpisz help, aby zobaczyć polecenia, quit - rozłącz.
> led on
Lampa: wł.
> servo 60 100
Usługa niedostępna: serwa (wyłączona w config.h lub nie działa)
> photo
Zapisano /photos/IMG_00012.jpg (48 KB)
> sd list 3
  /photos/IMG_00012.jpg  48 KB
  /photos/IMG_00011.jpg  47 KB
  /photos/IMG_00010.jpg  51 KB
> quit
Do widzenia.
```

### Telnet: biblioteka i ograniczenia

- Core Arduino nie ma serwera telnet, jest tylko surowy `WiFiServer`. Dlatego `CamTelnetServer` używa gotowej biblioteki **ESPTelnet** (`lennarthennigs/ESP Telnet` 2.3.0, aktualizowana w 2026 r.). Kompiluje się z core 3.x bez ostrzeżeń.
- Obsługiwany jest **jeden klient naraz**. Kolejne połączenie jest odrzucane, a na Serial pojawia się komunikat.
- **Telnet nie szyfruje** niczego, także hasła `TELNET_PASSWORD`. Używaj go tylko w zaufanej sieci domowej.
- ESPTelnet dołącza `ETH.h`, więc do programu trafia ok. 12 KB obsługi Ethernetu, nawet przy `TELNET_SERVER_ON 0`. Nie da się tego wyłączyć bez zmian w pakiecie frameworka. **Nie dodawaj `lib_ignore = Ethernet`:** pioarduino usuwa wtedy ścieżki Ethernetu we współdzielonym pakiecie frameworka, co psuje kompilację również innych projektów.

### Wątki

Polecenia COM i Telnet wykonują się w `loop()`, a strona WWW w zadaniach httpd. `EspCamera` chroni operacje zmieniające stan (sensor, lampa, serwa, zapis na SD) rekurencyjnym mutexem FreeRTOS. Dzięki temu można np. zrobić zdjęcie z Telnetu w trakcie oglądania strumienia.

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
  - `panPin`, `tiltPin`: z `config.h`
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

### Usługi w `EspCamera`

| Usługa | Dołączenie | Metody |
|---|---|---|
| wszystkie | – | `features()` zwraca `{led, sd, servos}`: włączone w `config.h` **i** poprawnie uruchomione (np. karta włożona) |
| lampa | `attachLed(CamLed&)` | `hasLed()`, `setLed(bool)`, `ledOn()` |
| karta SD | `attachSd(CamSdCard&)` | `hasSd()`, `savePhotoToSd(&bajty)` (zwraca ścieżkę pliku), `sdPhotoCount()`, `sdFreeBytes()`, `sdLastPhoto()`, `sdListPhotos(Print&, N)` |
| serwa | `attachServos(CamServos&)` | `hasServos()`, `setPan(deg)`, `setTilt(deg)`, `centerServos()`, `pan()`, `tilt()`, `servoLimits(...)` |

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
