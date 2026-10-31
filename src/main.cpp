// ESP32-CAM AI Thinker - podgląd kamery na żywo przez WWW
//  - port 80: strona HTML, /control, /status, /capture
//  - port 81: /stream (MJPEG multipart/x-mixed-replace)

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "esp_camera.h"
#include "esp_http_server.h"

#include "camera_pins.h"
#include "secrets.h"

static const char *MDNS_NAME = "esp32cam";
static const char *AP_SSID = "ESP32-CAM";
static const char *AP_PASS = "12345678";
static const uint32_t WIFI_TIMEOUT_MS = 15000;

// Lampa LED: PWM, celowo słaba jasność (pełna moc oślepia i grzeje diodę)
static const uint32_t LED_FREQ = 5000;
static const uint8_t LED_RES_BITS = 8;
static const uint32_t LED_DUTY_ON = 24;
static const uint8_t LED_LEDC_CHANNEL = 2;

static httpd_handle_t web_httpd = nullptr;
static httpd_handle_t stream_httpd = nullptr;

static bool has_psram = false;
static bool led_on = false;
static volatile uint32_t frames_sent = 0;   // licznik klatek wysłanych w strumieniu
static volatile uint8_t stream_clients = 0;
static float server_fps = 0.0f;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pl">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32-CAM</title>
<style>
:root{--bg:#111418;--panel:#1b2027;--line:#2b323c;--text:#e6e9ee;--muted:#8b95a3;--accent:#3b82f6}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;justify-content:space-between;align-items:center;padding:10px 16px;border-bottom:1px solid var(--line)}
h1{font-size:18px;margin:0}
.stats{display:flex;gap:14px;color:var(--muted);font-variant-numeric:tabular-nums;font-size:13px}
.stats b{color:var(--text)}
main{display:flex;flex-wrap:wrap;gap:16px;padding:16px;max-width:1200px;margin:0 auto}
.view{flex:1 1 560px;min-width:0}
.view img{display:block;width:100%;height:auto;background:#000;border-radius:8px;min-height:200px}
aside{flex:0 1 300px;background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px;display:flex;flex-direction:column;gap:14px}
label.t{display:block;color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.04em;margin-bottom:6px}
.btns{display:flex;flex-wrap:wrap;gap:6px}
button{flex:1;background:#232a33;color:var(--text);border:1px solid var(--line);border-radius:6px;padding:8px 10px;font:inherit;cursor:pointer}
button.on{background:var(--accent);border-color:var(--accent)}
input[type=range]{width:100%;accent-color:var(--accent)}
.row{display:flex;justify-content:space-between;align-items:center}
a{color:var(--accent)}
.foot{color:var(--muted);font-size:12px}
@media(max-width:640px){main{padding:10px}.stats{gap:8px}}
</style>
</head>
<body>
<header>
  <h1>ESP32-CAM</h1>
  <div class="stats">
    <span>przegl.: <b id="fpsB">-</b> kl/s</span>
    <span>serwer: <b id="fpsS">-</b> kl/s</span>
    <span>RSSI: <b id="rssi">-</b></span>
  </div>
</header>
<main>
  <div class="view"><img id="cam" alt="Podgląd kamery"></div>
  <aside>
    <div>
      <label class="t">Rozdzielczość</label>
      <div class="btns" id="fs">
        <button data-v="5">QVGA</button><button data-v="8">VGA</button>
        <button data-v="9">SVGA</button><button data-v="10">XGA</button>
      </div>
    </div>
    <div>
      <div class="row"><label class="t">Jakość JPEG</label><span id="qv">12</span></div>
      <input type="range" id="q" min="5" max="40" value="12">
      <div class="foot">mniej = lepsza jakość, większe klatki</div>
    </div>
    <div>
      <label class="t">Obraz</label>
      <div class="btns">
        <button id="led">Lampa</button><button id="vflip">Flip</button><button id="hmirror">Lustro</button>
      </div>
    </div>
    <div class="foot" id="info"></div>
    <div class="foot"><a href="/capture" target="_blank">Zdjęcie (JPEG)</a> · <a id="raw" target="_blank">Surowy strumień</a></div>
  </aside>
</main>
<script>
const $ = id => document.getElementById(id);
const streamUrl = `${location.protocol}//${location.hostname}:81/stream`;
$('raw').href = streamUrl;
let frames = 0;

function ctl(v, val) { return fetch(`/control?var=${v}&val=${val}`).then(refresh); }

// Strumień czytany przez fetch, żeby liczyć klatki w przeglądarce.
// Gdy fetch/ReadableStream nie działa, fallback na zwykłe <img src>.
async function startStream() {
  const img = $('cam');
  try {
    const res = await fetch(streamUrl);
    const reader = res.body.getReader();
    let buf = new Uint8Array(0), prevUrl = null;
    const dec = new TextDecoder();
    const find = (a, seq, from) => {
      outer: for (let i = from; i <= a.length - seq.length; i++) {
        for (let j = 0; j < seq.length; j++) if (a[i + j] !== seq[j]) continue outer;
        return i;
      }
      return -1;
    };
    const CRLF2 = [13, 10, 13, 10];
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      const nb = new Uint8Array(buf.length + value.length);
      nb.set(buf); nb.set(value, buf.length); buf = nb;
      for (;;) {
        const h = find(buf, CRLF2, 0);
        if (h < 0) break;
        const m = /Content-Length:\s*(\d+)/i.exec(dec.decode(buf.subarray(0, h)));
        if (!m) { buf = buf.subarray(h + 4); continue; }
        const len = +m[1], start = h + 4;
        if (buf.length < start + len) break;
        const url = URL.createObjectURL(new Blob([buf.slice(start, start + len)], { type: 'image/jpeg' }));
        img.src = url;
        if (prevUrl) URL.revokeObjectURL(prevUrl);
        prevUrl = url;
        frames++;
        buf = buf.subarray(start + len);
      }
    }
  } catch (e) {
    console.warn('fetch stream niedostępny, używam <img>', e);
    img.src = streamUrl;
    return;
  }
  setTimeout(startStream, 1000);  // strumień zerwany - spróbuj ponownie
}

let last = performance.now();
setInterval(() => {
  const now = performance.now();
  $('fpsB').textContent = (frames * 1000 / (now - last)).toFixed(1);
  frames = 0; last = now;
}, 1000);

async function refresh() {
  try {
    const s = await (await fetch('/status')).json();
    $('fpsS').textContent = s.fps.toFixed(1);
    $('rssi').textContent = s.rssi + ' dBm';
    $('qv').textContent = s.quality; $('q').value = s.quality;
    document.querySelectorAll('#fs button').forEach(b => b.classList.toggle('on', +b.dataset.v === s.framesize));
    ['led', 'vflip', 'hmirror'].forEach(k => $(k).classList.toggle('on', !!s[k]));
    $('info').textContent = `${s.width}×${s.height} · ${s.mode} ${s.ip} · PSRAM: ${s.psram ? 'tak' : 'nie'} · klienci: ${s.clients}`;
  } catch (e) {}
}

document.querySelectorAll('#fs button').forEach(b => b.onclick = () => ctl('framesize', b.dataset.v));
$('q').oninput = e => $('qv').textContent = e.target.value;
$('q').onchange = e => ctl('quality', e.target.value);
['led', 'vflip', 'hmirror'].forEach(k => $(k).onclick = () => ctl(k, $(k).classList.contains('on') ? 0 : 1));

refresh();
setInterval(refresh, 1000);
startStream();
</script>
</body>
</html>
)rawliteral";

static const char *framesizeName(framesize_t fs) {
  switch (fs) {
    case FRAMESIZE_QVGA: return "QVGA";
    case FRAMESIZE_CIF:  return "CIF";
    case FRAMESIZE_VGA:  return "VGA";
    case FRAMESIZE_SVGA: return "SVGA";
    case FRAMESIZE_XGA:  return "XGA";
    default:             return "?";
  }
}

static void setLed(bool on) {
  led_on = on;
  ledcWrite(LED_GPIO_NUM, on ? LED_DUTY_ON : 0);
}

// ---------- port 80 ----------

static esp_err_t indexHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
}

static esp_err_t captureHandler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t statusHandler(httpd_req_t *req) {
  sensor_t *s = esp_camera_sensor_get();
  framesize_t fs = (framesize_t)s->status.framesize;
  bool ap = WiFi.status() != WL_CONNECTED;
  String ip = ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

  char json[320];
  snprintf(json, sizeof(json),
           "{\"fps\":%.1f,\"framesize\":%d,\"name\":\"%s\",\"width\":%u,\"height\":%u,"
           "\"quality\":%d,\"led\":%d,\"vflip\":%d,\"hmirror\":%d,\"rssi\":%d,"
           "\"psram\":%d,\"clients\":%u,\"mode\":\"%s\",\"ip\":\"%s\"}",
           server_fps, (int)fs, framesizeName(fs), resolution[fs].width, resolution[fs].height,
           s->status.quality, led_on, s->status.vflip, s->status.hmirror,
           ap ? 0 : (int)WiFi.RSSI(), has_psram, stream_clients, ap ? "AP" : "STA", ip.c_str());

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t controlHandler(httpd_req_t *req) {
  char query[64], var[16], val[8];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
      httpd_query_key_value(query, "var", var, sizeof(var)) != ESP_OK ||
      httpd_query_key_value(query, "val", val, sizeof(val)) != ESP_OK) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "brak var/val");
    return ESP_FAIL;
  }

  int v = atoi(val);
  sensor_t *s = esp_camera_sensor_get();
  int res = 0;

  if (!strcmp(var, "framesize")) {
    // Bufory zaalokowano dla XGA (PSRAM) lub QVGA (bez PSRAM) - wyżej nie wolno.
    framesize_t maxFs = has_psram ? FRAMESIZE_XGA : FRAMESIZE_QVGA;
    if (v < 0 || v > maxFs) res = -1;
    else res = s->set_framesize(s, (framesize_t)v);
  } else if (!strcmp(var, "quality")) {
    res = s->set_quality(s, constrain(v, 4, 63));
  } else if (!strcmp(var, "vflip")) {
    res = s->set_vflip(s, v ? 1 : 0);
  } else if (!strcmp(var, "hmirror")) {
    res = s->set_hmirror(s, v ? 1 : 0);
  } else if (!strcmp(var, "led")) {
    setLed(v != 0);
  } else {
    res = -1;
  }

  if (res) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "niepoprawna wartość");
    return ESP_FAIL;
  }
  return httpd_resp_send(req, nullptr, 0);
}

// ---------- port 81 ----------

static esp_err_t streamHandler(httpd_req_t *req) {
  char part[64];
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");

  stream_clients++;
  Serial.printf("Strumień: klient połączony (%u)\n", stream_clients);

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Błąd pobrania klatki");
      res = ESP_FAIL;
      break;
    }
    size_t hlen = snprintf(part, sizeof(part), STREAM_PART, fb->len);
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, part, hlen);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
    frames_sent++;
  }

  stream_clients--;
  Serial.printf("Strumień: klient rozłączony (%u)\n", stream_clients);
  return res;
}

static void startServers() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 8;
  config.lru_purge_enable = true;

  const httpd_uri_t index_uri   = {"/",        HTTP_GET, indexHandler,   nullptr};
  const httpd_uri_t status_uri  = {"/status",  HTTP_GET, statusHandler,  nullptr};
  const httpd_uri_t control_uri = {"/control", HTTP_GET, controlHandler, nullptr};
  const httpd_uri_t capture_uri = {"/capture", HTTP_GET, captureHandler, nullptr};
  const httpd_uri_t stream_uri  = {"/stream",  HTTP_GET, streamHandler,  nullptr};

  if (httpd_start(&web_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(web_httpd, &index_uri);
    httpd_register_uri_handler(web_httpd, &status_uri);
    httpd_register_uri_handler(web_httpd, &control_uri);
    httpd_register_uri_handler(web_httpd, &capture_uri);
  } else {
    Serial.println("Nie udało się uruchomić serwera na porcie 80");
  }

  // Osobna instancja httpd: strumień blokuje swoje zadanie, więc nie może dzielić go ze stroną.
  config.server_port = 81;
  config.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  } else {
    Serial.println("Nie udało się uruchomić serwera strumienia na porcie 81");
  }
}

// ---------- setup ----------

static bool initCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 12;

  has_psram = psramFound();
  if (has_psram) {
    // Bufory alokowane dla największej oferowanej rozdzielczości (XGA),
    // potem przełączamy na VGA - dzięki temu zmiana rozdzielczości nie przepełni bufora.
    config.frame_size = FRAMESIZE_XGA;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Błąd inicjalizacji kamery: 0x%x (%s)\n", err, esp_err_to_name(err));
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (has_psram) s->set_framesize(s, FRAMESIZE_VGA);
  Serial.printf("Kamera OK (PID 0x%x), PSRAM: %s, rozdzielczość: %s\n", s->id.PID,
                has_psram ? "tak" : "nie", framesizeName((framesize_t)s->status.framesize));
  return true;
}

static void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // oszczędzanie energii WiFi mocno obniża FPS
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Łączenie z WiFi \"%s\"", WIFI_SSID);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Połączono, IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("Brak połączenia - uruchamiam punkt dostępowy");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("AP \"%s\" (hasło %s), IP: %s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
  }

  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("mDNS: http://%s.local\n", MDNS_NAME);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-CAM start");

  // Kanał 2 (timer 1): kamera zajmuje kanał 0 / timer 0 na sygnał XCLK
  ledcAttachChannel(LED_GPIO_NUM, LED_FREQ, LED_RES_BITS, LED_LEDC_CHANNEL);
  setLed(false);

  if (!initCamera()) {
    Serial.println("Restart za 5 s...");
    delay(5000);
    ESP.restart();
  }

  initWiFi();
  startServers();

  IPAddress ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP();
  Serial.printf("Strona: http://%s/  strumień: http://%s:81/stream\n", ip.toString().c_str(),
                ip.toString().c_str());
}

void loop() {
  static uint32_t lastSec = millis(), lastPrint = millis();
  static uint32_t secFrames = 0, printFrames = 0;

  uint32_t now = millis();
  if (now - lastSec >= 1000) {
    uint32_t total = frames_sent;
    server_fps = (total - secFrames) * 1000.0f / (now - lastSec);
    secFrames = total;
    lastSec = now;
  }
  if (now - lastPrint >= 5000) {
    uint32_t total = frames_sent;
    float fps = (total - printFrames) * 1000.0f / (now - lastPrint);
    printFrames = total;
    lastPrint = now;
    Serial.printf("FPS: %.1f, klienci: %u, wolna PSRAM: %u KB, heap: %u KB\n", fps, stream_clients,
                  ESP.getFreePsram() / 1024, ESP.getFreeHeap() / 1024);
  }
  delay(100);
}
