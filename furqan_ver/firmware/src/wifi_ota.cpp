#include "wifi_ota.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>

static WiFiManager _wm;
static WebServer   _server(80);
static bool        _connected   = false;
static bool        _serverUp    = false;
static String      _ip;
static String      _deviceName;

// ── Upload page HTML ──────────────────────────────────────────────────────────
static const char PAGE_UPDATE[] PROGMEM = R"HTML(
<!DOCTYPE html><html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>IQRA PAD - Firmware Update</title>
<style>
body{font-family:Arial,sans-serif;background:#f5f5f5;
     display:flex;justify-content:center;align-items:center;
     min-height:100vh;margin:0}
.card{background:white;border-radius:12px;padding:40px;max-width:420px;
      width:90%;box-shadow:0 4px 20px rgba(0,0,0,.12);text-align:center}
h1{color:#2c3e50;margin-bottom:6px;font-size:1.6em}
p{color:#7f8c8d;margin-bottom:28px;font-size:.95em}
.pick{display:block;padding:14px 24px;background:#3498db;color:white;
      border-radius:8px;cursor:pointer;margin-bottom:16px;
      border:none;font-size:1em;width:100%}
.pick:hover{background:#2980b9}
#file{display:none}
#fname{color:#2c3e50;font-size:.9em;margin-bottom:18px;min-height:20px}
.btn{padding:14px 24px;background:#27ae60;color:white;border-radius:8px;
     cursor:pointer;border:none;font-size:1em;width:100%;display:none}
.btn:hover{background:#229954}
#prog{width:100%;background:#ecf0f1;border-radius:6px;
      height:12px;margin-top:20px;display:none}
#bar{height:12px;background:#27ae60;border-radius:6px;width:0%}
#msg{margin-top:16px;font-size:.95em;color:#2c3e50}
</style>
</head>
<body>
<div class="card">
<h1>IQRA PAD</h1>
<p>Firmware Update<br>
<small style="color:#aaa">Upload the .bin file you received</small></p>
<label class="pick" for="file">Choose firmware.bin</label>
<input type="file" id="file" accept=".bin">
<div id="fname"></div>
<button class="btn" id="upbtn" onclick="doUpload()">Upload and Install</button>
<div id="prog"><div id="bar"></div></div>
<div id="msg"></div>
</div>
<script>
document.getElementById('file').addEventListener('change',function(){
  var f=this.files[0];
  document.getElementById('fname').textContent=
    f ? f.name+' ('+(f.size/1024).toFixed(1)+' KB)' : '';
  document.getElementById('upbtn').style.display=f?'block':'none';
});
function doUpload(){
  var f=document.getElementById('file').files[0];
  if(!f) return;
  var fd=new FormData();
  fd.append('firmware',f);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/do_update');
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      var pct=Math.round(e.loaded/e.total*100);
      document.getElementById('prog').style.display='block';
      document.getElementById('bar').style.width=pct+'%';
      document.getElementById('msg').textContent='Uploading... '+pct+'%';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){
      document.getElementById('msg').innerHTML=
        '<b style="color:green">Done! Device is rebooting...</b>';
      document.getElementById('bar').style.width='100%';
    } else {
      document.getElementById('msg').innerHTML=
        '<b style="color:red">Update failed. Try again.</b>';
    }
  };
  xhr.send(fd);
}
</script>
</body></html>
)HTML";

// ── Web server handlers ───────────────────────────────────────────────────────
static void handleRoot() {
  _server.sendHeader("Location", "/update", true);
  _server.send(302, "text/plain", "");
}

static void handleUpdatePage() {
  _server.send_P(200, "text/html", PAGE_UPDATE);
}

static uint32_t _otaWritten = 0;

static void handleDoUpdate() {
  HTTPUpload& up = _server.upload();
  if (up.status == UPLOAD_FILE_START) {
    _otaWritten = 0;
    Serial.printf("[OTA-web] Start: %s\n", up.filename.c_str());
    Update.begin(UPDATE_SIZE_UNKNOWN);
  }
  else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
    _otaWritten += up.currentSize;
    Serial.printf("[OTA-web] %u KB\r", _otaWritten / 1024);
  }
  else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
    Serial.printf("\n[OTA-web] Done — %u bytes\n", _otaWritten);
  }
}

static void handleUpdateDone() {
  bool ok = !Update.hasError();
  _server.send(200, "text/plain", ok ? "OK" : "FAIL");
  if (ok) { delay(500); ESP.restart(); }
}

// ── Start web server (called once WiFi is actually up) ────────────────────────
static void startWebServer()
{
  if (_serverUp) return;

  if (MDNS.begin(_deviceName.c_str())) {
    Serial.printf("[WiFi] mDNS: http://%s.local/update\n", _deviceName.c_str());
    MDNS.addService("http", "tcp", 80);
  }

  _server.on("/",          HTTP_GET,  handleRoot);
  _server.on("/update",    HTTP_GET,  handleUpdatePage);
  _server.on("/do_update", HTTP_POST, handleUpdateDone, handleDoUpdate);
  _server.onNotFound([]() {
    _server.sendHeader("Location", "/update", true);
    _server.send(302, "text/plain", "");
  });

  _server.begin();
  _serverUp = true;
  Serial.printf("[WiFi] Web server up — IP: %s\n", _ip.c_str());
}

// ── Public API ────────────────────────────────────────────────────────────────
void wifiOtaInit(const char* deviceName)
{
  _deviceName = deviceName;

  // Non-blocking mode: returns immediately whether WiFi connects or not.
  // The captive-portal hotspot (IQRA-PAD-SETUP) is served via wifiOtaHandle().
  _wm.setConfigPortalBlocking(false);
  _wm.setConnectTimeout(5);           // 5 s max to try saved network, then fall to portal
  _wm.setConnectRetries(1);           // only 1 attempt — fail fast if not in range
  _wm.setConfigPortalTimeout(0);      // portal stays open indefinitely (no auto-close)

  _wm.setAPCallback([](WiFiManager*) {
    Serial.println("[WiFi] No saved network — hotspot 'IQRA-PAD-SETUP' is open.");
    Serial.println("[WiFi] Connect your phone to it to set up WiFi.");
  });

  if (_wm.autoConnect("IQRA-PAD-SETUP")) {
    // Saved WiFi found and connected immediately
    _connected = true;
    _ip = WiFi.localIP().toString();
    Serial.printf("[WiFi] Connected — IP: %s\n", _ip.c_str());
    startWebServer();
  } else {
    Serial.println("[WiFi] Captive portal running in background...");
  }
}

void wifiOtaHandle()
{
  if (!_connected) {
    _wm.process();   // drives the captive-portal web server

    // Check if the portal flow just finished connecting
    if (WiFi.status() == WL_CONNECTED) {
      _connected = true;
      _ip = WiFi.localIP().toString();
      Serial.printf("[WiFi] Connected via portal — IP: %s\n", _ip.c_str());
      startWebServer();
    }
  } else {
    _server.handleClient();
  }
}

bool   wifiIsConnected() { return _connected; }
String wifiIP()          { return _ip; }
String wifiSSID()        { return _connected ? WiFi.SSID() : ""; }

void wifiReset()
{
  _wm.resetSettings();
  Serial.println("[WiFi] Credentials cleared — rebooting.");
  delay(300);
  ESP.restart();
}
