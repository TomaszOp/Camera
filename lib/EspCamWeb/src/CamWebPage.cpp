/**
 * @file CamWebPage.cpp
 * @brief Preview page HTML/CSS/JS stored in flash (PROGMEM).
 */
#include "CamWebPage.h"

namespace espcamweb {

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
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
    <span>browser: <b id="fpsB">-</b> fps</span>
    <span>server: <b id="fpsS">-</b> fps</span>
    <span>RSSI: <b id="rssi">-</b></span>
  </div>
</header>
<main>
  <div class="view"><img id="cam" alt="Camera preview"></div>
  <aside>
    <div>
      <label class="t">Resolution</label>
      <div class="btns" id="fs">
        <button data-v="5">QVGA</button><button data-v="8">VGA</button>
        <button data-v="9">SVGA</button><button data-v="10">XGA</button>
      </div>
    </div>
    <div>
      <div class="row"><label class="t">JPEG quality</label><span id="qv">12</span></div>
      <input type="range" id="q" min="5" max="40" value="12">
      <div class="foot">lower = better quality, bigger frames</div>
    </div>
    <div>
      <label class="t">Image</label>
      <div class="btns">
        <button id="led" hidden>Lamp</button><button id="vflip">Flip</button><button id="hmirror">Mirror</button>
      </div>
    </div>
    <div id="secServo" hidden>
      <label class="t">Camera movement</label>
      <div class="row"><span>Pan</span><span id="panV">-</span></div>
      <input type="range" id="pan" min="0" max="180" step="1">
      <div class="row"><span>Tilt</span><span id="tiltV">-</span></div>
      <input type="range" id="tilt" min="0" max="180" step="1">
      <div class="btns">
        <button data-a="pan" data-d="-10">&#9664;</button><button data-a="tilt" data-d="10">&#9650;</button>
        <button id="center">&#9679;</button>
        <button data-a="tilt" data-d="-10">&#9660;</button><button data-a="pan" data-d="10">&#9654;</button>
      </div>
    </div>
    <div id="secSd" hidden>
      <label class="t">SD card</label>
      <div class="btns"><button id="save">Save photo to SD</button></div>
      <div class="foot" id="sdInfo"></div>
    </div>
    <div class="foot" id="info"></div>
    <div class="foot"><a href="/capture" target="_blank">Snapshot (JPEG)</a> · <a id="raw" target="_blank">Raw stream</a></div>
    <div class="foot" id="wifiLink" hidden><a href="/wifi">WiFi settings</a></div>
  </aside>
</main>
<script>
const $ = id => document.getElementById(id);
let streamUrl = '';
let frames = 0;

function ctl(v, val) { return fetch(`/control?var=${v}&val=${val}`).then(refresh); }

// The stream is read with fetch() so the browser can count frames.
// If fetch/ReadableStream is not supported, fall back to a plain <img src>.
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
    console.warn('fetch stream unavailable, using <img>', e);
    img.src = streamUrl;
    return;
  }
  setTimeout(startStream, 1000);  // stream dropped - retry
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
    $('wifiLink').hidden = !s.features.settings;
    if (s.features.sd) $('sdInfo').textContent = `photos: ${s.sdPhotos} · free: ${s.sdFreeMB} MB` + (s.sdLast ? ` · last: ${s.sdLast}` : '');
    if (s.features.servos) {
      pos.pan = s.pan; pos.tilt = s.tilt;
      ['pan', 'tilt'].forEach(k => { $(k + 'V').textContent = s[k] + '°'; if (document.activeElement !== $(k)) $(k).value = s[k]; });
    }
    $('info').textContent = `${s.width}×${s.height} · ${s.mode} ${s.ip} · PSRAM: ${s.psram ? 'yes' : 'no'} · clients: ${s.clients}`;
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
    $('sdInfo').textContent = r.ok ? `saved: ${r.file}` : `error: ${r.error}`;
  } catch (e) { $('sdInfo').textContent = 'save failed'; }
  $('save').disabled = false;
};

refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)rawliteral";

const size_t INDEX_HTML_LEN = sizeof(INDEX_HTML) - 1;

#if WIFI_SETTINGS_ON
const char WIFI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32-CAM WiFi</title>
<style>
:root{--bg:#111418;--panel:#1b2027;--line:#2b323c;--text:#e6e9ee;--muted:#8b95a3;--accent:#3b82f6;--ok:#22c55e;--err:#ef4444}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;justify-content:space-between;align-items:center;padding:10px 16px;border-bottom:1px solid var(--line)}
h1{font-size:18px;margin:0}
main{display:flex;flex-direction:column;gap:16px;padding:16px;max-width:560px;margin:0 auto}
section{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:14px;display:flex;flex-direction:column;gap:10px}
label.t{display:block;color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.04em}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font-variant-numeric:tabular-nums}
.kv span:nth-child(odd){color:var(--muted)}
.btns{display:flex;flex-wrap:wrap;gap:6px}
button{flex:1;background:#232a33;color:var(--text);border:1px solid var(--line);border-radius:6px;padding:8px 10px;font:inherit;cursor:pointer}
button.on,button.primary{background:var(--accent);border-color:var(--accent)}
button.danger{border-color:var(--err);color:var(--err)}
input[type=text],input[type=password]{width:100%;background:#0d1014;color:var(--text);border:1px solid var(--line);border-radius:6px;padding:8px 10px;font:inherit}
.check{display:flex;align-items:center;gap:6px;color:var(--muted);font-size:13px}
a{color:var(--accent)}
.foot{color:var(--muted);font-size:12px}
#msg.ok{color:var(--ok)}#msg.err{color:var(--err)}
</style>
</head>
<body>
<header><h1>WiFi settings</h1><a href="/">&larr; Camera</a></header>
<main>
  <section>
    <label class="t">Current network</label>
    <div class="kv" id="cur"></div>
  </section>
  <section>
    <label class="t">Saved settings</label>
    <div class="kv" id="saved"></div>
  </section>
  <section>
    <label class="t">Change settings</label>
    <div class="btns">
      <button id="mSta" class="on" type="button">Join a network (STA)</button>
      <button id="mAp" type="button">Own access point (AP)</button>
    </div>
    <div><label class="t" for="ssid">SSID</label><input type="text" id="ssid" maxlength="32" autocomplete="off"></div>
    <div><label class="t" for="pass">Password</label><input type="password" id="pass" maxlength="63" autocomplete="new-password"></div>
    <label class="check"><input type="checkbox" id="show"> Show password</label>
    <div class="foot">Empty password = open network. Otherwise 8-63 characters.</div>
    <div class="btns">
      <button id="save" class="primary" type="button">Save</button>
      <button id="clear" class="danger" type="button">Clear settings</button>
    </div>
    <div id="msg"></div>
    <div class="foot">Changes take effect after restarting the device (power cycle, RESET button or the "restart yes" command).
      If the network cannot be joined, the camera starts the default access point.</div>
  </section>
</main>
<script>
const $ = id => document.getElementById(id);
let mode = 'sta';

function kv(el, rows) {
  el.innerHTML = '';
  rows.forEach(([k, v]) => {
    const a = document.createElement('span'); a.textContent = k;
    const b = document.createElement('span'); b.textContent = v;
    el.append(a, b);
  });
}

function setMode(m) {
  mode = m;
  $('mSta').classList.toggle('on', m === 'sta');
  $('mAp').classList.toggle('on', m === 'ap');
}

function msg(text, ok) {
  $('msg').textContent = text;
  $('msg').className = ok ? 'ok' : 'err';
}

async function load() {
  try {
    const s = await (await fetch('/wifi/settings')).json();
    const c = s.current;
    kv($('cur'), [['Mode', c.mode], ['SSID', c.ssid || '-'], ['IP', c.ip], ['RSSI', c.mode === 'STA' ? c.rssi + ' dBm' : '-'],
      ['Started with', s.source + ' settings' + (s.fallback ? ' (fallback to the default AP)' : '')]]);
    if (s.saved.present) {
      kv($('saved'), [['Mode', s.saved.mode], ['SSID', s.saved.ssid], ['Password', s.saved.hasPassword ? 'set (hidden)' : 'none (open)']]);
      setMode(s.saved.mode === 'AP' ? 'ap' : 'sta');
      $('ssid').value = s.saved.ssid;
    } else {
      kv($('saved'), [['Saved', 'none - using the defaults from code']]);
    }
  } catch (e) { msg('Cannot read the settings', false); }
}

async function post(url, body) {
  const r = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
  return r.json();
}

$('mSta').onclick = () => setMode('sta');
$('mAp').onclick = () => setMode('ap');
$('show').onchange = e => $('pass').type = e.target.checked ? 'text' : 'password';
$('save').onclick = async () => {
  const ssid = $('ssid').value, pass = $('pass').value;
  if (!ssid) return msg('Enter the SSID', false);
  if (pass && (pass.length < 8 || pass.length > 63)) return msg('Password must be empty or 8-63 characters', false);
  try {
    const r = await post('/wifi/save', new URLSearchParams({ mode, ssid, password: pass }).toString());
    msg(r.message, r.ok);
    if (r.ok) { $('pass').value = ''; load(); }
  } catch (e) { msg('Save failed', false); }
};
$('clear').onclick = async () => {
  if (!confirm('Erase the saved WiFi settings? The defaults from code will be used after a restart.')) return;
  try {
    const r = await post('/wifi/clear', '');
    msg(r.message, r.ok);
    if (r.ok) load();
  } catch (e) { msg('Clear failed', false); }
};
load();
</script>
</body>
</html>
)rawliteral";

const size_t WIFI_HTML_LEN = sizeof(WIFI_HTML) - 1;
#endif

}  // namespace espcamweb
