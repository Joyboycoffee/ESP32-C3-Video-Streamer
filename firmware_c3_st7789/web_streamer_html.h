#pragma once
#include <Arduino.h>

// Full HTML5 Video Streamer & Wi-Fi Configuration Web App (Embedded in ESP32 Flash)
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>ESP32-C3 Video Streamer // Web Deck</title>
  <style>
    :root {
      --bg: #0B0D10; --surface: #13171D; --surface-raised: #1A2028;
      --border: #262F3D; --cyan: #00F0FF; --amber: #FFB800;
      --emerald: #00FF66; --crimson: #FF3366; --text: #F0F4F8;
      --text-muted: #8E9BAE; --text-dim: #546274; --radius: 10px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace; -webkit-tap-highlight-color: transparent; }
    body { background: var(--bg); color: var(--text); min-height: 100vh; display: flex; flex-direction: column; align-items: center; padding: 14px; }
    .container { width: 100%; max-width: 740px; display: flex; flex-direction: column; gap: 14px; }
    .header { display: flex; align-items: center; justify-content: space-between; padding: 12px 16px; background: var(--surface); border: 1px solid var(--border); border-radius: var(--radius); }
    .brand-logo { background: var(--cyan); color: #000; font-weight: 900; font-size: 0.75rem; padding: 4px 8px; border-radius: 6px; letter-spacing: 1px; }
    .brand-title { font-size: 0.95rem; font-weight: 800; letter-spacing: 0.5px; margin-left: 8px; }
    .btn-tool { background: var(--surface-raised); border: 1px solid var(--border); color: var(--text); padding: 6px 12px; font-size: 0.75rem; font-weight: 700; border-radius: 6px; cursor: pointer; }
    .btn-tool:hover { border-color: var(--cyan); color: var(--cyan); }
    .card { background: var(--surface); border: 1px solid var(--border); border-radius: var(--radius); padding: 14px; display: flex; flex-direction: column; gap: 12px; }
    .conn-row { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; }
    .input-group { flex: 1; min-width: 180px; display: flex; background: var(--bg); border: 1px solid var(--border); border-radius: 6px; overflow: hidden; }
    .input-prefix { padding: 8px 10px; background: var(--surface-raised); color: var(--text-dim); font-size: 0.75rem; font-weight: 700; }
    .input-field { flex: 1; background: transparent; border: none; padding: 8px 10px; color: var(--text); font-size: 0.85rem; outline: none; }
    .btn-primary { background: var(--cyan); color: #000; border: none; padding: 8px 16px; font-size: 0.82rem; font-weight: 800; border-radius: 6px; cursor: pointer; }
    .btn-danger { background: var(--crimson); color: #fff; border: none; padding: 8px 16px; font-size: 0.82rem; font-weight: 800; border-radius: 6px; cursor: pointer; }
    .status-badge { display: inline-flex; align-items: center; gap: 6px; padding: 4px 10px; border-radius: 20px; font-size: 0.7rem; font-weight: 800; border: 1px solid var(--border); }
    .badge-offline { background: rgba(255, 51, 102, 0.1); color: var(--crimson); border-color: rgba(255, 51, 102, 0.3); }
    .badge-connecting { background: rgba(255, 184, 0, 0.1); color: var(--amber); border-color: rgba(255, 184, 0, 0.3); }
    .badge-online { background: rgba(0, 255, 102, 0.1); color: var(--emerald); border-color: rgba(0, 255, 102, 0.3); }
    .preview-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    @media (max-width: 600px) { .preview-grid { grid-template-columns: 1fr; } }
    .preview-box { background: var(--bg); border: 1px solid var(--border); border-radius: 8px; padding: 10px; display: flex; flex-direction: column; align-items: center; gap: 8px; }
    .preview-title { width: 100%; font-size: 0.72rem; font-weight: 800; color: var(--text-muted); display: flex; justify-content: space-between; }
    .canvas-frame { width: 100%; height: 200px; display: flex; align-items: center; justify-content: center; background: #000; border: 1px solid var(--border); border-radius: 6px; overflow: hidden; }
    #streamCanvas { image-rendering: pixelated; max-width: 100%; max-height: 100%; }
    #videoPlayer { max-width: 100%; max-height: 100%; }
    .controls-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 10px; }
    .control-item { display: flex; flex-direction: column; gap: 4px; }
    .control-label { font-size: 0.68rem; font-weight: 800; color: var(--text-muted); }
    .select-field { background: var(--bg); border: 1px solid var(--border); color: var(--text); padding: 7px 10px; border-radius: 6px; font-size: 0.75rem; font-weight: 700; outline: none; }
    .telemetry-row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; background: var(--bg); border: 1px solid var(--border); border-radius: 8px; padding: 8px 12px; }
    .stat-label { font-size: 0.65rem; font-weight: 700; color: var(--text-dim); }
    .stat-value { font-size: 0.95rem; font-weight: 900; color: var(--cyan); }
    .dropzone { border: 2px dashed var(--border); border-radius: 8px; padding: 20px 14px; text-align: center; cursor: pointer; background: rgba(0, 240, 255, 0.02); }
    .dropzone:hover { border-color: var(--cyan); }
    .modal-backdrop { position: fixed; inset: 0; background: rgba(0,0,0,0.85); backdrop-filter: blur(4px); display: none; align-items: center; justify-content: center; padding: 14px; z-index: 1000; }
    .modal-backdrop.active { display: flex; }
    .modal-card { background: var(--surface); border: 1px solid var(--border); border-radius: var(--radius); width: 100%; max-width: 440px; overflow: hidden; }
    .modal-header { padding: 12px 16px; border-bottom: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; }
    .modal-body { padding: 16px; display: flex; flex-direction: column; gap: 12px; max-height: 70vh; overflow-y: auto; }
    .wifi-item { display: flex; justify-content: space-between; padding: 8px 10px; background: var(--bg); border: 1px solid var(--border); border-radius: 6px; cursor: pointer; }
    .wifi-item:hover { border-color: var(--cyan); }
  </style>
</head>
<body>
  <div class="container">
    <header class="header">
      <div>
        <span class="brand-logo">ESP32-C3</span>
        <span class="brand-title">VIDEO STREAMER</span>
      </div>
      <div>
        <button class="btn-tool" onclick="openWifiModal()">⚙️ WI-FI CONFIG</button>
        <button class="btn-tool" onclick="toggleCamera()" id="btnCamera">📷 CAMERA</button>
      </div>
    </header>

    <div class="card">
      <div class="conn-row">
        <div class="input-group">
          <span class="input-prefix">WS://</span>
          <input type="text" id="espIpInput" class="input-field" placeholder="192.168.4.1 or IP">
          <span class="input-prefix">:81</span>
        </div>
        <button class="btn-primary" id="btnConnect" onclick="toggleConnection()">CONNECT</button>
        <div id="statusBadge" class="status-badge badge-offline">
          <span id="statusText">DISCONNECTED</span>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="dropzone" onclick="document.getElementById('videoFileInput').click()">
        <input type="file" id="videoFileInput" accept="video/*,.mp4,.webm,.mov,.avi,.gif" style="display:none;" onchange="handleFileSelect(event)">
        <div style="font-size:0.85rem; font-weight:800;">📁 SELECT VIDEO (MP4, WEBM, GIF)</div>
        <div style="font-size:0.72rem; color:var(--text-dim); margin-top:4px;">Runs in any browser on Mobile & PC</div>
      </div>
    </div>

    <div class="preview-grid">
      <div class="preview-box">
        <div class="preview-title"><span>SOURCE</span><span id="videoSourceInfo" style="color:var(--cyan);">NONE</span></div>
        <div class="canvas-frame"><video id="videoPlayer" playsinline loop muted controls></video></div>
      </div>
      <div class="preview-box">
        <div class="preview-title"><span>ESP32 DISPLAY</span><span id="displayResBadge" style="color:var(--emerald);">240x240 IPS</span></div>
        <div class="canvas-frame"><canvas id="streamCanvas" width="240" height="240"></canvas></div>
      </div>
    </div>

    <div class="card">
      <div class="controls-grid">
        <div class="control-item">
          <label class="control-label">DISPLAY</label>
          <select id="selDisplay" class="select-field" onchange="updateDisplayMode()">
            <option value="st7789-native">ST7789 240x240 IPS</option>
            <option value="st7789-hud">ST7789 HUD (128x64)</option>
            <option value="ssd1306">SSD1306 128x64 OLED</option>
          </select>
        </div>
        <div class="control-item">
          <label class="control-label">RENDER</label>
          <select id="selMode" class="select-field">
            <option value="dither">Floyd-Steinberg</option>
            <option value="threshold">Binary Threshold</option>
          </select>
        </div>
        <div class="control-item">
          <label class="control-label">FIT</label>
          <select id="selFit" class="select-field">
            <option value="crop">Crop (Fill)</option>
            <option value="letterbox">Letterbox (Fit)</option>
          </select>
        </div>
        <div class="control-item">
          <label class="control-label">FPS (<span id="fpsDisplay">15</span>)</label>
          <input type="range" id="fpsSlider" min="5" max="30" value="15" step="1" oninput="document.getElementById('fpsDisplay').innerText = this.value" style="margin-top:6px; accent-color:var(--cyan);">
        </div>
      </div>

      <div class="telemetry-row">
        <div class="stat-pod"><span class="stat-label">FPS</span><span class="stat-value" id="hudActualFps">0.0</span></div>
        <div class="stat-pod"><span class="stat-label">FRAMES</span><span class="stat-value" id="hudFramesSent">0</span></div>
        <div class="stat-pod"><span class="stat-label">BITRATE</span><span class="stat-value" id="hudBitrate">0 KB/s</span></div>
      </div>

      <button class="btn-primary" id="btnPlayStream" style="padding:12px;" onclick="toggleStreaming()">▶ START STREAMING</button>
    </div>
  </div>

  <div class="modal-backdrop" id="wifiModal" onclick="closeWifiModal(event)">
    <div class="modal-card" onclick="event.stopPropagation()">
      <div class="modal-header">
        <div style="font-weight:800; font-size:0.85rem;">⚙️ ESP32 WI-FI CONFIG</div>
        <button class="btn-tool" onclick="closeWifiModal()">&times;</button>
      </div>
      <div class="modal-body">
        <div class="control-item">
          <label class="control-label">SSID</label>
          <div style="display:flex; gap:6px;">
            <input type="text" id="wifiSsidInput" class="input-field" style="background:var(--bg); border:1px solid var(--border); border-radius:6px;" placeholder="Network name">
            <button class="btn-tool" onclick="scanWifiNetworks()">SCAN</button>
          </div>
        </div>
        <div id="wifiScanList" style="display:flex; flex-direction:column; gap:4px; max-height:100px; overflow-y:auto;"></div>
        <div class="control-item">
          <label class="control-label">PASSWORD</label>
          <input type="password" id="wifiPassInput" class="input-field" style="background:var(--bg); border:1px solid var(--border); border-radius:6px;" placeholder="Wi-Fi Password">
        </div>
        <div style="display:flex; gap:8px; margin-top:6px;">
          <button class="btn-primary" style="flex:1;" onclick="submitWifiCredentials()">CONNECT TO WI-FI</button>
          <button class="btn-danger" onclick="resetWifiCredentials()">RESET AP</button>
        </div>
      </div>
    </div>
  </div>

  <script>
    let socket = null, isConnected = false, isStreaming = false, streamInterval = null, cameraStream = null;
    let frameCount = 0, bytesSent = 0, lastStatsTime = performance.now();
    const video = document.getElementById('videoPlayer'), canvas = document.getElementById('streamCanvas');
    const ctx = canvas.getContext('2d', { willReadFrequently: true });
    const ipInput = document.getElementById('espIpInput'), statusBadge = document.getElementById('statusBadge');
    const statusText = document.getElementById('statusText'), btnConnect = document.getElementById('btnConnect');
    const btnPlayStream = document.getElementById('btnPlayStream');

    window.addEventListener('load', () => {
      const host = window.location.hostname;
      ipInput.value = (host && host !== 'localhost' && host !== '127.0.0.1' && !host.includes('github.io')) ? host : (localStorage.getItem('esp32_streamer_ip') || '192.168.4.1');
    });

    function toggleConnection() { if (isConnected) disconnectSocket(); else connectSocket(); }
    function connectSocket() {
      const ip = ipInput.value.trim();
      if (!ip) return alert("Enter ESP32 IP");
      localStorage.setItem('esp32_streamer_ip', ip);
      statusBadge.className = "status-badge badge-connecting"; statusText.innerText = "CONNECTING...";
      try {
        socket = new WebSocket(`ws://${ip}:81/`);
        socket.binaryType = "arraybuffer";
        socket.onopen = () => { isConnected = true; statusBadge.className = "status-badge badge-online"; statusText.innerText = "CONNECTED"; btnConnect.innerText = "DISCONNECT"; btnConnect.className = "btn-danger"; };
        socket.onclose = disconnectSocket; socket.onerror = disconnectSocket;
      } catch (e) { disconnectSocket(); }
    }
    function disconnectSocket() {
      if (isStreaming) toggleStreaming();
      if (socket) try { socket.close(); } catch (e) {}
      socket = null; isConnected = false;
      statusBadge.className = "status-badge badge-offline"; statusText.innerText = "DISCONNECTED";
      btnConnect.innerText = "CONNECT"; btnConnect.className = "btn-primary";
    }

    function handleFileSelect(e) {
      const file = e.target.files[0]; if (!file) return;
      if (cameraStream) stopCamera();
      video.src = URL.createObjectURL(file); video.load(); video.play().catch(()=>{});
      document.getElementById('videoSourceInfo').innerText = file.name.substring(0, 14);
    }
    async function toggleCamera() {
      if (cameraStream) stopCamera();
      else {
        try {
          cameraStream = await navigator.mediaDevices.getUserMedia({ video: true, audio: false });
          video.srcObject = cameraStream; video.play();
          document.getElementById('videoSourceInfo').innerText = "LIVE CAMERA";
        } catch (err) { alert("Camera error: " + err.message); }
      }
    }
    function stopCamera() { if (cameraStream) cameraStream.getTracks().forEach(t=>t.stop()); cameraStream = null; video.srcObject = null; }

    function updateDisplayMode() {
      const disp = document.getElementById('selDisplay').value;
      canvas.width = (disp === 'ssd1306') ? 128 : 240; canvas.height = (disp === 'ssd1306') ? 64 : 240;
      document.getElementById('displayResBadge').innerText = (disp === 'ssd1306') ? "128x64 OLED" : "240x240 IPS";
    }

    function toggleStreaming() {
      if (isStreaming) {
        isStreaming = false; clearInterval(streamInterval);
        btnPlayStream.innerText = "▶ START STREAMING"; btnPlayStream.className = "btn-primary"; video.pause();
      } else {
        if (!isConnected) return alert("Connect to ESP32 first!");
        if (!video.videoWidth && !video.srcObject) return alert("Select video or camera!");
        isStreaming = true; btnPlayStream.innerText = "⏹ STOP STREAMING"; btnPlayStream.className = "btn-danger"; video.play().catch(()=>{});
        const fps = parseInt(document.getElementById('fpsSlider').value) || 15;
        streamInterval = setInterval(processFrame, Math.round(1000 / fps));
      }
    }

    function processFrame() {
      if (!isStreaming || !video.videoWidth) return;
      const targetDisp = document.getElementById('selDisplay').value;
      const targetW = (targetDisp === 'ssd1306' || targetDisp === 'st7789-hud') ? 128 : 240;
      const targetH = (targetDisp === 'ssd1306' || targetDisp === 'st7789-hud') ? 64 : 240;
      ctx.fillStyle = "#000"; ctx.fillRect(0, 0, canvas.width, canvas.height);
      const vw = video.videoWidth, vh = video.videoHeight;
      if (document.getElementById('selFit').value === 'crop') {
        const sc = Math.max(targetW / vw, targetH / vh);
        ctx.drawImage(video, (vw - targetW/sc)/2, (vh - targetH/sc)/2, targetW/sc, targetH/sc, 0, 0, targetW, targetH);
      } else {
        const sc = Math.min(targetW / vw, targetH / vh);
        ctx.drawImage(video, 0, 0, vw, vh, (targetW - vw*sc)/2, (targetH - vh*sc)/2, vw*sc, vh*sc);
      }
      const imgData = ctx.getImageData(0, 0, targetW, targetH);
      const bin = (document.getElementById('selMode').value === 'dither') ? ditherFS(imgData, targetW, targetH) : thresh(imgData, targetW, targetH);
      for (let i = 0; i < targetW * targetH; i++) {
        const val = bin[i] ? 255 : 0, idx = i * 4;
        imgData.data[idx] = (targetDisp === 'st7789-native' && val) ? 0 : val;
        imgData.data[idx+1] = (targetDisp === 'st7789-native' && val) ? 240 : val;
        imgData.data[idx+2] = (targetDisp === 'st7789-native' && val) ? 255 : val;
        imgData.data[idx+3] = 255;
      }
      ctx.putImageData(imgData, 0, 0);

      if (socket && socket.readyState === WebSocket.OPEN) {
        if (targetDisp === 'ssd1306' || targetDisp === 'st7789-hud') {
          const pkt = new Uint8Array(4 + 1024);
          pkt[0]=0x56; pkt[1]=0x49; pkt[2]=0x44; pkt[3]=0x30;
          for (let p = 0; p < 8; p++) {
            for (let x = 0; x < 128; x++) {
              let b = 0;
              for (let bit = 0; bit < 8; bit++) if (bin[(p * 8 + bit) * 128 + x]) b |= (1 << bit);
              pkt[4 + p * 128 + x] = b;
            }
          }
          socket.send(pkt.buffer); bytesSent += pkt.byteLength; frameCount++;
        } else {
          const seq = frameCount & 0xFFFF;
          for (let c = 0; c < 6; c++) {
            const pkt = new Uint8Array(8 + 1200);
            pkt[0]=0x56; pkt[1]=0x32; pkt[2]=0x34; pkt[3]=0x30; pkt[4]=c; pkt[5]=6; pkt[6]=(seq>>8)&0xFF; pkt[7]=seq&0xFF;
            const startY = c * 40;
            for (let y = 0; y < 40; y++) {
              const rowY = startY + y, ro = 8 + (y * 30);
              for (let bx = 0; bx < 30; bx++) {
                let byteVal = 0, sx = bx * 8;
                for (let bit = 0; bit < 8; bit++) if (bin[rowY * 240 + (sx + bit)]) byteVal |= (0x80 >> bit);
                pkt[ro + bx] = byteVal;
              }
            }
            socket.send(pkt.buffer); bytesSent += pkt.byteLength;
          }
          frameCount++;
        }
      }
      const now = performance.now(), el = (now - lastStatsTime) / 1000.0;
      if (el >= 0.8) {
        document.getElementById('hudActualFps').innerText = (frameCount / el).toFixed(1);
        document.getElementById('hudFramesSent').innerText = frameCount;
        document.getElementById('hudBitrate').innerText = `${(bytesSent / 1024 / el).toFixed(1)} KB/s`;
        frameCount = 0; bytesSent = 0; lastStatsTime = now;
      }
    }

    function ditherFS(imgData, w, h) {
      const d = imgData.data, g = new Float32Array(w * h), out = new Uint8Array(w * h);
      for (let i = 0; i < w * h; i++) g[i] = 0.299 * d[i*4] + 0.587 * d[i*4+1] + 0.114 * d[i*4+2];
      for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
          const i = y * w + x, oldVal = g[i], newVal = oldVal < 128 ? 0 : 255;
          out[i] = newVal ? 1 : 0; const err = oldVal - newVal;
          if (x + 1 < w) g[i + 1] += err * (7/16);
          if (y + 1 < h) {
            if (x > 0) g[i + w - 1] += err * (3/16);
            g[i + w] += err * (5/16);
            if (x + 1 < w) g[i + w + 1] += err * (1/16);
          }
        }
      }
      return out;
    }
    function thresh(imgData, w, h) {
      const d = imgData.data, out = new Uint8Array(w * h);
      for (let i = 0; i < w * h; i++) out[i] = (0.299*d[i*4] + 0.587*d[i*4+1] + 0.114*d[i*4+2] >= 128) ? 1 : 0;
      return out;
    }

    function openWifiModal() { document.getElementById('wifiModal').classList.add('active'); }
    function closeWifiModal(e) { if (e && e.target && e.target !== e.currentTarget && !e.target.classList.contains('btn-tool')) return; document.getElementById('wifiModal').classList.remove('active'); }
    async function scanWifiNetworks() {
      const ip = ipInput.value.trim() || '192.168.4.1', list = document.getElementById('wifiScanList');
      list.innerHTML = "<div style='font-size:0.75rem;'>Scanning...</div>";
      try {
        const r = await fetch(`http://${ip}/scan`, { signal: AbortSignal.timeout(6000) });
        const nets = await r.json(); list.innerHTML = "";
        nets.forEach(n => {
          const d = document.createElement('div'); d.className = "wifi-item";
          d.innerHTML = `<span>${n.ssid}</span><span style="color:var(--text-dim);">${n.rssi}dBm</span>`;
          d.onclick = () => { document.getElementById('wifiSsidInput').value = n.ssid; };
          list.appendChild(d);
        });
      } catch (e) { list.innerHTML = `<div style="font-size:0.75rem; color:var(--crimson);">Scan failed: ${e.message}</div>`; }
    }
    async function submitWifiCredentials() {
      const ip = ipInput.value.trim() || '192.168.4.1', ssid = document.getElementById('wifiSsidInput').value.trim(), pass = document.getElementById('wifiPassInput').value;
      if (!ssid) return alert("Enter SSID");
      try {
        const fd = new FormData(); fd.append('ssid', ssid); fd.append('pass', pass);
        await fetch(`http://${ip}/save-wifi`, { method: 'POST', body: fd });
      } catch (e) {}
      alert("Credentials saved! ESP32 is connecting to " + ssid);
      closeWifiModal();
    }
    async function resetWifiCredentials() {
      if (!confirm("Reset to Hotspot mode?")) return;
      const ip = ipInput.value.trim() || '192.168.4.1';
      try { await fetch(`http://${ip}/reset-wifi`, { method: 'POST' }); } catch (e) {}
      alert("ESP32 reset to AP mode!"); closeWifiModal();
    }
  </script>
</body>
</html>
)rawliteral";
