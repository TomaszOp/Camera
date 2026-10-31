/**
 * @file CamWebPage.cpp
 * @brief Preview page HTML/CSS/JS stored in flash (PROGMEM). The page text itself is user-facing (Polish).
 */
#include "CamWebPage.h"

namespace espcamweb {

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
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
[hidden]{display:none!important}
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
        <button id="led" hidden>Lampa</button><button id="vflip">Flip</button><button id="hmirror">Lustro</button>
      </div>
    </div>
    <div id="secServo" hidden>
      <label class="t">Ruch kamery</label>
      <div class="row"><span>Poziom</span><span id="panV">-</span></div>
      <input type="range" id="pan" min="0" max="180" step="1">
      <div class="row"><span>Pion</span><span id="tiltV">-</span></div>
      <input type="range" id="tilt" min="0" max="180" step="1">
      <div class="btns">
        <button data-a="pan" data-d="-10">&#9664;</button><button data-a="tilt" data-d="10">&#9650;</button>
        <button id="center">&#9679;</button>
        <button data-a="tilt" data-d="-10">&#9660;</button><button data-a="pan" data-d="10">&#9654;</button>
      </div>
    </div>
    <div id="secSd" hidden>
      <label class="t">Karta SD</label>
      <div class="btns"><button id="save">Zapisz zdjęcie na SD</button></div>
      <div class="foot" id="sdInfo"></div>
    </div>
    <div class="foot" id="info"></div>
    <div class="foot"><a href="/capture" target="_blank">Zdjęcie (JPEG)</a> · <a id="raw" target="_blank">Surowy strumień</a></div>
  </aside>
</main>
<script>
const $ = id => document.getElementById(id);
let streamUrl = '';
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
    if (!streamUrl) {
      streamUrl = `${location.protocol}//${location.hostname}:${s.streamPort}/stream`;
      $('raw').href = streamUrl;
      startStream();
    }
    $('fpsS').textContent = s.fps.toFixed(1);
    $('rssi').textContent = s.rssi + ' dBm';
    $('qv').textContent = s.quality; $('q').value = s.quality;
    document.querySelectorAll('#fs button').forEach(b => b.classList.toggle('on', +b.dataset.v === s.framesize));
    ['led', 'vflip', 'hmirror'].forEach(k => $(k).classList.toggle('on', !!s[k]));
    document.querySelectorAll('#fs button').forEach(b => b.hidden = +b.dataset.v > s.maxFramesize);
    $('led').hidden = !s.features.led;
    $('secSd').hidden = !s.features.sd;
    $('secServo').hidden = !s.features.servos;
    if (s.features.sd) $('sdInfo').textContent = `zdjęć: ${s.sdPhotos} · wolne: ${s.sdFreeMB} MB` + (s.sdLast ? ` · ostatnie: ${s.sdLast}` : '');
    if (s.features.servos) {
      pos.pan = s.pan; pos.tilt = s.tilt;
      ['pan', 'tilt'].forEach(k => { $(k + 'V').textContent = s[k] + '°'; if (document.activeElement !== $(k)) $(k).value = s[k]; });
    }
    $('info').textContent = `${s.width}×${s.height} · ${s.mode} ${s.ip} · PSRAM: ${s.psram ? 'tak' : 'nie'} · klienci: ${s.clients}`;
  } catch (e) {}
}

document.querySelectorAll('#fs button').forEach(b => b.onclick = () => ctl('framesize', b.dataset.v));
$('q').oninput = e => $('qv').textContent = e.target.value;
$('q').onchange = e => ctl('quality', e.target.value);
['led', 'vflip', 'hmirror'].forEach(k => $(k).onclick = () => ctl(k, $(k).classList.contains('on') ? 0 : 1));

const pos = { pan: 90, tilt: 90 };
['pan', 'tilt'].forEach(k => {
  $(k).oninput = e => $(k + 'V').textContent = e.target.value + '°';
  $(k).onchange = e => ctl(k, e.target.value);
});
document.querySelectorAll('#secServo [data-a]').forEach(b => b.onclick = () => ctl(b.dataset.a, pos[b.dataset.a] + +b.dataset.d));
$('center').onclick = () => ctl('center', 1);
$('save').onclick = async () => {
  $('save').disabled = true;
  try {
    const r = await (await fetch('/save')).json();
    $('sdInfo').textContent = r.ok ? `zapisano: ${r.file}` : `błąd: ${r.error}`;
  } catch (e) { $('sdInfo').textContent = 'błąd zapisu'; }
  $('save').disabled = false;
};

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawliteral";

const size_t INDEX_HTML_LEN = sizeof(INDEX_HTML) - 1;

}  // namespace espcamweb
