#include "wifi_ota.h"
#include "settings.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static WiFiManager   _wm;
static WebServer     _server(80);
static bool          _connected  = false;
static bool          _serverUp   = false;
static volatile bool _initDone   = false;
static String        _ip;
static String        _deviceName;
static String        _fwVersion;

// ── Dashboard HTML ────────────────────────────────────────────────────────────
static const char PAGE_DASHBOARD[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>IQRA PAD</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Arial,sans-serif;background:#f0f2f5;min-height:100vh}
.header{background:linear-gradient(135deg,#1a1a2e 0%,#16213e 100%);
        color:white;padding:28px 20px 22px;text-align:center}
.header h1{font-size:2em;letter-spacing:4px;font-weight:700;margin-bottom:4px}
.header p{color:#8892a4;font-size:.8em;letter-spacing:2px;text-transform:uppercase}
.tabs{display:flex;background:#fff;border-bottom:2px solid #e1e4e8;padding:0 16px}
.tab{padding:14px 22px;border:none;background:none;cursor:pointer;font-size:.9em;
     color:#888;border-bottom:3px solid transparent;margin-bottom:-2px;
     font-weight:600;transition:all .2s;letter-spacing:.3px}
.tab.active{color:#2563eb;border-bottom-color:#2563eb}
.tab:hover:not(.active){color:#2563eb;background:#f8faff}
.panel{display:none;padding:20px;max-width:520px;margin:0 auto}
.panel.active{display:block}
.card{background:white;border-radius:14px;padding:22px;
      box-shadow:0 2px 12px rgba(0,0,0,.07);margin-bottom:16px}
.card-title{font-size:.7em;font-weight:700;color:#aaa;letter-spacing:2px;
            text-transform:uppercase;margin-bottom:16px;padding-bottom:10px;
            border-bottom:1px solid #f0f0f0}
.row{display:flex;justify-content:space-between;align-items:center;
     padding:11px 0;border-bottom:1px solid #f7f7f7}
.row:last-child{border-bottom:none}
.lbl{color:#777;font-size:.88em}
.val{font-weight:600;color:#1a1a2e;font-size:.88em}
.badge{padding:3px 12px;border-radius:20px;font-size:.78em;font-weight:700}
.green{background:#f0fdf4;color:#16a34a;border:1px solid #bbf7d0}
.amber{background:#fef9c3;color:#92400e;border:1px solid #fde68a}
.blue{background:#eff6ff;color:#2563eb;border:1px solid #bfdbfe}
/* Update tab */
.pick{display:block;width:100%;padding:14px;background:#2563eb;color:white;
      border:none;border-radius:10px;cursor:pointer;font-size:.95em;
      font-weight:600;margin-bottom:12px;text-align:center}
.pick:hover{background:#1d4ed8}
#file{display:none}
.fname{color:#666;font-size:.83em;margin-bottom:14px;min-height:18px;text-align:center}
.upbtn{display:none;width:100%;padding:14px;background:#16a34a;color:white;
       border:none;border-radius:10px;cursor:pointer;font-size:.95em;font-weight:600}
.upbtn:hover{background:#15803d}
.bar-wrap{display:none;background:#f0f0f0;border-radius:6px;
          height:10px;margin-top:16px;overflow:hidden}
.bar{height:10px;background:#16a34a;width:0%;transition:width .3s;border-radius:6px}
.msg{margin-top:12px;font-size:.88em;color:#555;text-align:center;min-height:20px}
</style>
</head>
<body>
<div class="header">
  <h1>IQRA PAD</h1>
  <p>Device Management</p>
</div>
<div class="tabs">
  <button class="tab active" onclick="show('about',this)">About Device</button>
  <button class="tab" onclick="show('update',this)">Firmware Update</button>
</div>

<div id="about" class="panel active">
  <div class="card">
    <div class="card-title">Device Information</div>
    <div class="row"><span class="lbl">Device Name</span><span class="val" id="d-name">...</span></div>
    <div class="row"><span class="lbl">Model</span><span class="val" id="d-model">...</span></div>
    <div class="row"><span class="lbl">User</span><span class="val" id="d-user">...</span></div>
    <div class="row"><span class="lbl">Firmware</span><span class="badge blue" id="d-ver">...</span></div>
    <div class="row"><span class="lbl">IP Address</span><span class="val" id="d-ip">...</span></div>
    <div class="row"><span class="lbl">Bluetooth</span><span class="badge amber">Coming Soon</span></div>
  </div>
</div>

<div id="update" class="panel">
  <div class="card">
    <div class="card-title">Firmware Update</div>
    <p style="color:#888;font-size:.83em;text-align:center;margin-bottom:18px">
      Installed: <strong id="cur-ver" style="color:#2563eb">...</strong>
    </p>
    <label class="pick" for="file">Choose firmware.bin</label>
    <input type="file" id="file" accept=".bin">
    <div class="fname" id="fname"></div>
    <button class="upbtn" id="upbtn" onclick="doUpload()">Upload and Install</button>
    <div class="bar-wrap" id="prog"><div class="bar" id="bar"></div></div>
    <div class="msg" id="msg"></div>
  </div>
</div>

<script>
function show(id,btn){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  btn.classList.add('active');
}
fetch('/api/info').then(r=>r.json()).then(d=>{
  document.getElementById('d-name').textContent  = d.device  ||'-';
  document.getElementById('d-model').textContent = d.model   ||'-';
  document.getElementById('d-user').textContent  = d.user    ||'-';
  document.getElementById('d-ver').textContent   = 'v'+(d.version||'-');
  document.getElementById('d-ip').textContent    = d.ip      ||'-';
  document.getElementById('cur-ver').textContent = 'v'+(d.version||'-');
}).catch(()=>{});
document.getElementById('file').addEventListener('change',function(){
  var f=this.files[0];
  document.getElementById('fname').textContent=f?f.name+' ('+(f.size/1024).toFixed(1)+' KB)':'';
  document.getElementById('upbtn').style.display=f?'block':'none';
});
function doUpload(){
  var f=document.getElementById('file').files[0];
  if(!f)return;
  var fd=new FormData();fd.append('firmware',f);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/do_update');
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      var p=Math.round(e.loaded/e.total*100);
      document.getElementById('prog').style.display='block';
      document.getElementById('bar').style.width=p+'%';
      document.getElementById('msg').textContent='Uploading... '+p+'%';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){
      document.getElementById('msg').innerHTML='<b style="color:#16a34a">Done! Device is rebooting...</b>';
      document.getElementById('bar').style.width='100%';
    }else{
      document.getElementById('msg').innerHTML='<b style="color:red">Update failed. Try again.</b>';
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

static void handleDashboard() {
  _server.send_P(200, "text/html", PAGE_DASHBOARD);
}

static void handleApiInfo() {
  String json = "{";
  json += "\"device\":\""  + String(DEVICE_NAME)  + "\",";
  json += "\"model\":\""   + String(DEVICE_MODEL) + "\",";
  json += "\"user\":\""    + String(DEVICE_USER)  + "\",";
  json += "\"version\":\"" + _fwVersion           + "\",";
  json += "\"ip\":\""      + _ip                  + "\"";
  json += "}";
  _server.send(200, "application/json", json);
}

static uint32_t _otaWritten = 0;

static void handleDoUpdate() {
  HTTPUpload& up = _server.upload();
  if (up.status == UPLOAD_FILE_START) {
    _otaWritten = 0;
    Serial.printf("[OTA-web] Start: %s\n", up.filename.c_str());
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
    _otaWritten += up.currentSize;
    Serial.printf("[OTA-web] %u KB\r", _otaWritten / 1024);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
    Serial.printf("\n[OTA-web] Done — %u bytes\n", _otaWritten);
  }
}

static void handleUpdateDone() {
  bool ok = !Update.hasError();
  _server.send(200, "text/plain", ok ? "OK" : "FAIL");
  if (ok) { delay(500); ESP.restart(); }
}

// ── Start web server ──────────────────────────────────────────────────────────
static void startWebServer()
{
  if (_serverUp) return;

  if (MDNS.begin(_deviceName.c_str())) {
    Serial.printf("[WiFi] mDNS: http://%s.local/update\n", _deviceName.c_str());
    MDNS.addService("http", "tcp", 80);
  }

  _server.on("/",          HTTP_GET,  handleRoot);
  _server.on("/update",    HTTP_GET,  handleDashboard);
  _server.on("/api/info",  HTTP_GET,  handleApiInfo);
  _server.on("/do_update", HTTP_POST, handleUpdateDone, handleDoUpdate);
  _server.onNotFound([]() {
    _server.sendHeader("Location", "/update", true);
    _server.send(302, "text/plain", "");
  });

  _server.begin();
  _serverUp = true;
  Serial.printf("[WiFi] Web server up — IP: %s\n", _ip.c_str());
}

// ── Background WiFi task ──────────────────────────────────────────────────────
static void wifiTask(void*)
{
  _wm.setConfigPortalBlocking(false);
  _wm.setConnectTimeout(20);
  _wm.setConnectRetries(3);
  _wm.setConfigPortalTimeout(0);

  _wm.setAPCallback([](WiFiManager*) {
    Serial.println("[WiFi] No saved network — hotspot 'IQRA-PAD-SETUP' is open.");
  });

  if (_wm.autoConnect("IQRA-PAD-SETUP")) {
    _connected = true;
    _ip = WiFi.localIP().toString();
    Serial.printf("[WiFi] Connected — IP: %s\n", _ip.c_str());
    startWebServer();
  } else {
    Serial.println("[WiFi] Portal running...");
  }

  _initDone = true;
  vTaskDelete(nullptr);
}

// ── Public API ────────────────────────────────────────────────────────────────
void wifiOtaInit(const char* deviceName, const char* fwVersion)
{
  _deviceName = deviceName;
  _fwVersion  = fwVersion;
  xTaskCreatePinnedToCore(wifiTask, "wifi_init", 8192,
                          nullptr, 1, nullptr, 1);
}

void wifiOtaHandle()
{
  if (!_initDone) return;

  if (!_connected) {
    _wm.process();
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
