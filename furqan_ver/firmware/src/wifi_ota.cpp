#include "wifi_ota.h"
#include "settings.h"
#include "sd_card.h"

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
.hdr{background:linear-gradient(135deg,#1a1a2e,#16213e);color:#fff;
     padding:24px 20px 18px;text-align:center}
.hdr h1{font-size:1.9em;letter-spacing:4px;font-weight:700;margin-bottom:4px}
.hdr p{color:#8892a4;font-size:.78em;letter-spacing:2px;text-transform:uppercase}
.tabs{display:flex;background:#fff;border-bottom:2px solid #e1e4e8;padding:0 12px}
.tab{padding:13px 18px;border:none;background:none;cursor:pointer;font-size:.87em;
     color:#888;border-bottom:3px solid transparent;margin-bottom:-2px;
     font-weight:600;transition:all .2s}
.tab.active{color:#2563eb;border-bottom-color:#2563eb}
.tab:hover:not(.active){color:#2563eb;background:#f8faff}
.panel{display:none;padding:16px;max-width:520px;margin:0 auto}
.panel.active{display:block}
.card{background:#fff;border-radius:12px;padding:20px;
      box-shadow:0 2px 10px rgba(0,0,0,.07);margin-bottom:14px}
.ct{font-size:.68em;font-weight:700;color:#aaa;letter-spacing:2px;
    text-transform:uppercase;margin-bottom:14px;padding-bottom:8px;
    border-bottom:1px solid #f0f0f0}
.row{display:flex;justify-content:space-between;align-items:center;
     padding:10px 0;border-bottom:1px solid #f7f7f7}
.row:last-child{border-bottom:none}
.lbl{color:#777;font-size:.86em}
.val{font-weight:600;color:#1a1a2e;font-size:.86em}
.badge{padding:3px 11px;border-radius:20px;font-size:.76em;font-weight:700}
.green{background:#f0fdf4;color:#16a34a;border:1px solid #bbf7d0}
.amber{background:#fef9c3;color:#92400e;border:1px solid #fde68a}
.blue{background:#eff6ff;color:#2563eb;border:1px solid #bfdbfe}
.pick{display:block;width:100%;padding:13px;background:#2563eb;color:#fff;
      border:none;border-radius:9px;cursor:pointer;font-size:.92em;
      font-weight:600;margin-bottom:10px;text-align:center}
.pick:hover{background:#1d4ed8}
.upbtn{display:none;width:100%;padding:13px;background:#16a34a;color:#fff;
       border:none;border-radius:9px;cursor:pointer;font-size:.92em;font-weight:600}
.upbtn:hover{background:#15803d}
#fw-file{display:none}
.fname{color:#666;font-size:.82em;margin-bottom:12px;min-height:18px;text-align:center}
.bar-wrap{display:none;background:#eee;border-radius:6px;height:9px;
          margin-top:14px;overflow:hidden}
.bar{height:9px;background:#16a34a;width:0%;transition:width .3s;border-radius:6px}
.msg{margin-top:10px;font-size:.86em;color:#555;text-align:center;min-height:18px}
/* file manager */
select.fsel{width:100%;padding:9px 10px;border-radius:8px;border:1px solid #ddd;
            font-size:.88em;color:#333;background:#fff;margin-bottom:10px}
.frow{display:flex;gap:8px;margin-bottom:10px}
.frow input{flex:1;padding:9px;border-radius:8px;border:1px solid #ddd;font-size:.86em}
.btn-sm{padding:9px 13px;border:none;border-radius:8px;cursor:pointer;
        font-size:.86em;font-weight:600}
.btn-blue{background:#2563eb;color:#fff}.btn-blue:hover{background:#1d4ed8}
.btn-red{background:#fee2e2;color:#dc2626}.btn-red:hover{background:#fecaca}
.flist{background:#f8f8f8;border-radius:8px;max-height:190px;overflow-y:auto;
       margin-bottom:10px;font-size:.83em}
.fitem{display:flex;justify-content:space-between;align-items:center;
       padding:7px 10px;border-bottom:1px solid #eee}
.fitem:last-child{border-bottom:none}
.fsize{color:#999;font-size:.8em;margin-right:8px}
</style>
</head>
<body>
<div class="hdr">
  <h1>IQRA PAD</h1>
  <p>Device Management</p>
</div>
<div class="tabs">
  <button class="tab active" onclick="show('about',this)">About</button>
  <button class="tab" onclick="show('files',this);loadFolders()">Files</button>
  <button class="tab" onclick="show('update',this)">Firmware</button>
</div>

<!-- ABOUT TAB -->
<div id="about" class="panel active">
  <div class="card">
    <div class="ct">Device Information</div>
    <div class="row"><span class="lbl">Device</span><span class="val" id="d-name">-</span></div>
    <div class="row"><span class="lbl">Model</span><span class="val" id="d-model">-</span></div>
    <div class="row"><span class="lbl">User</span><span class="val" id="d-user">-</span></div>
    <div class="row"><span class="lbl">Firmware</span><span class="badge blue" id="d-ver">-</span></div>
    <div class="row"><span class="lbl">Display</span><span class="val" id="d-disp">-</span></div>
    <div class="row"><span class="lbl">Chip</span><span class="val" id="d-chip">-</span></div>
    <div class="row"><span class="lbl">IP Address</span><span class="val" id="d-ip">-</span></div>
    <div class="row"><span class="lbl">Bluetooth</span><span class="badge amber">Coming Soon</span></div>
  </div>
</div>

<!-- FILES TAB -->
<div id="files" class="panel">
  <div class="card">
    <div class="ct">Audio File Manager</div>
    <select class="fsel" id="fsel" onchange="loadFiles()">
      <option value="">-- Select Reciter Folder --</option>
    </select>
    <div class="frow">
      <input id="nfld" type="text" placeholder="New folder name...">
      <button class="btn-sm btn-blue" onclick="mkDir()">+ Create</button>
    </div>
    <div class="flist" id="flist" style="display:none"></div>
    <div id="uarea" style="display:none">
      <label class="pick" for="mp3f">Choose MP3 File</label>
      <input type="file" id="mp3f" accept=".mp3,.MP3" style="display:none">
      <div class="fname" id="mp3n"></div>
      <button class="upbtn" id="mp3b" onclick="upFile()">Upload to Device</button>
      <div class="bar-wrap" id="mp3p"><div class="bar" id="mp3bar"></div></div>
      <div class="msg" id="mp3m"></div>
    </div>
  </div>
</div>

<!-- FIRMWARE TAB -->
<div id="update" class="panel">
  <div class="card">
    <div class="ct">Firmware Update</div>
    <p style="color:#888;font-size:.82em;text-align:center;margin-bottom:16px">
      Installed: <strong id="cur-ver" style="color:#2563eb">-</strong>
    </p>
    <label class="pick" for="fw-file">Choose firmware.bin</label>
    <input type="file" id="fw-file" accept=".bin">
    <div class="fname" id="fw-fname"></div>
    <button class="upbtn" id="fw-btn" onclick="doFwUpload()">Upload and Install</button>
    <div class="bar-wrap" id="fw-prog"><div class="bar" id="fw-bar"></div></div>
    <div class="msg" id="fw-msg"></div>
  </div>
</div>

<script>
function show(id,btn){
  document.querySelectorAll('.panel').forEach(p=>p.classList.remove('active'));
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  btn.classList.add('active');
}

// ── About ─────────────────────────────────────────────────────────────────
fetch('/api/info').then(r=>r.json()).then(d=>{
  document.getElementById('d-name').textContent  = d.device  ||'-';
  document.getElementById('d-model').textContent = d.model   ||'-';
  document.getElementById('d-user').textContent  = d.user    ||'-';
  document.getElementById('d-ver').textContent   = 'v'+(d.version||'-');
  document.getElementById('d-disp').textContent  = d.display ||'-';
  document.getElementById('d-chip').textContent  = d.chip    ||'-';
  document.getElementById('d-ip').textContent    = d.ip      ||'-';
  document.getElementById('cur-ver').textContent = 'v'+(d.version||'-');
}).catch(()=>{});

// ── File Manager ──────────────────────────────────────────────────────────
function loadFolders(){
  fetch('/api/folders').then(r=>r.json()).then(d=>{
    var s=document.getElementById('fsel');
    s.innerHTML='<option value="">-- Select Reciter Folder --</option>';
    d.forEach(f=>s.innerHTML+='<option value="'+f+'">'+f+'</option>');
  }).catch(()=>{});
}
function loadFiles(){
  var f=document.getElementById('fsel').value;
  var fl=document.getElementById('flist');
  var ua=document.getElementById('uarea');
  if(!f){fl.style.display='none';ua.style.display='none';return;}
  ua.style.display='block';
  fl.style.display='block';
  fl.innerHTML='<div style="padding:10px;color:#888">Loading...</div>';
  fetch('/api/files?f='+encodeURIComponent(f)).then(r=>r.json()).then(d=>{
    if(!d.length){fl.innerHTML='<div style="padding:10px;color:#aaa;font-size:.85em">No files in this folder</div>';return;}
    fl.innerHTML=d.map(x=>'<div class="fitem"><span>'+x.name+'</span>'
      +'<div><span class="fsize">'+fmtSz(x.size)+'</span>'
      +'<button class="btn-sm btn-red" onclick="delFile(\''+f+'\',\''+x.name+'\')">Del</button>'
      +'</div></div>').join('');
  }).catch(()=>{fl.innerHTML='<div style="padding:10px;color:red">Error</div>';});
}
function fmtSz(b){return b>1048576?(b/1048576).toFixed(1)+' MB':b>1024?Math.round(b/1024)+' KB':b+' B';}
function mkDir(){
  var n=document.getElementById('nfld').value.trim();
  if(!n)return;
  fetch('/api/mkdir',{method:'POST',headers:{'Content-Type':'text/plain'},body:n})
    .then(r=>{if(r.ok){document.getElementById('nfld').value='';loadFolders();}
              else alert('Failed to create folder');})
    .catch(()=>alert('Error'));
}
function delFile(folder,name){
  if(!confirm('Delete '+name+'?'))return;
  fetch('/api/file?f='+encodeURIComponent(folder)+'&n='+encodeURIComponent(name),{method:'DELETE'})
    .then(r=>{if(r.ok)loadFiles();else alert('Delete failed');}).catch(()=>{});
}
document.getElementById('mp3f').addEventListener('change',function(){
  var f=this.files[0];
  document.getElementById('mp3n').textContent=f?f.name+' ('+(f.size/1048576).toFixed(1)+' MB)':'';
  document.getElementById('mp3b').style.display=f?'block':'none';
  document.getElementById('mp3m').textContent='';
  document.getElementById('mp3p').style.display='none';
  document.getElementById('mp3bar').style.width='0%';
});
function upFile(){
  var folder=document.getElementById('fsel').value;
  var f=document.getElementById('mp3f').files[0];
  if(!folder||!f)return;
  document.getElementById('mp3m').textContent='';
  var fd=new FormData();fd.append('file',f);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/upload?f='+encodeURIComponent(folder));
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      var p=Math.round(e.loaded/e.total*100);
      document.getElementById('mp3p').style.display='block';
      document.getElementById('mp3bar').style.width=p+'%';
      document.getElementById('mp3m').textContent='Uploading '+p+'%...';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){
      document.getElementById('mp3m').innerHTML='<b style="color:#16a34a">Done! File saved to SD card.</b>';
      document.getElementById('mp3bar').style.width='100%';
      document.getElementById('mp3f').value='';
      document.getElementById('mp3n').textContent='';
      document.getElementById('mp3b').style.display='none';
      loadFiles();
    }else{
      document.getElementById('mp3m').innerHTML='<b style="color:red">Upload failed: '+xhr.responseText+'</b>';
    }
  };
  xhr.onerror=function(){
    document.getElementById('mp3m').innerHTML='<b style="color:red">Connection error</b>';
  };
  xhr.send(fd);
}

// ── Firmware Upload ───────────────────────────────────────────────────────
document.getElementById('fw-file').addEventListener('change',function(){
  var f=this.files[0];
  document.getElementById('fw-fname').textContent=f?f.name+' ('+(f.size/1024).toFixed(1)+' KB)':'';
  document.getElementById('fw-btn').style.display=f?'block':'none';
});
function doFwUpload(){
  var f=document.getElementById('fw-file').files[0];
  if(!f)return;
  var fd=new FormData();fd.append('firmware',f);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/do_update');
  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      var p=Math.round(e.loaded/e.total*100);
      document.getElementById('fw-prog').style.display='block';
      document.getElementById('fw-bar').style.width=p+'%';
      document.getElementById('fw-msg').textContent='Uploading '+p+'%...';
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){
      document.getElementById('fw-msg').innerHTML='<b style="color:#16a34a">Done! Device is rebooting...</b>';
      document.getElementById('fw-bar').style.width='100%';
    }else{
      document.getElementById('fw-msg').innerHTML='<b style="color:red">Update failed. Try again.</b>';
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
  json += "\"device\":\""   + String(DEVICE_NAME)           + "\",";
  json += "\"model\":\""    + String(DEVICE_MODEL)          + "\",";
  json += "\"user\":\""     + String(DEVICE_USER)           + "\",";
  json += "\"version\":\""  + _fwVersion                    + "\",";
  json += "\"display\":\"4.2\\\" E-Ink 400x300\",";
  json += "\"chip\":\"ESP32-S3 N16R8\",";
  json += "\"ip\":\""       + _ip                           + "\"";
  json += "}";
  _server.send(200, "application/json", json);
}

// ── SD file manager endpoints ─────────────────────────────────────────────────
static void handleGetFolders() {
  String json = "[";
  bool first = true;
  File32 root = sd.open("/");
  if (root) {
    File32 entry;
    while (entry.openNext(&root, O_RDONLY)) {
      if (entry.isDirectory()) {
        char name[64] = {};
        entry.getName(name, sizeof(name) - 1);
        if (name[0] != '.') {
          if (!first) json += ",";
          json += "\"" + String(name) + "\"";
          first = false;
        }
      }
      entry.close();
    }
    root.close();
  }
  json += "]";
  _server.send(200, "application/json", json);
}

static void handleGetFiles() {
  String folder = _server.arg("f");
  if (folder.isEmpty()) { _server.send(400, "text/plain", "Missing folder"); return; }
  String json = "[";
  bool first = true;
  File32 dir = sd.open(("/" + folder).c_str());
  if (dir) {
    File32 entry;
    while (entry.openNext(&dir, O_RDONLY)) {
      if (!entry.isDirectory()) {
        char name[64] = {};
        entry.getName(name, sizeof(name) - 1);
        uint32_t sz = entry.fileSize();
        if (!first) json += ",";
        json += "{\"name\":\"" + String(name) + "\",\"size\":" + String(sz) + "}";
        first = false;
      }
      entry.close();
    }
    dir.close();
  }
  json += "]";
  _server.send(200, "application/json", json);
}

static void handleMkdir() {
  String name = _server.arg("plain");
  name.trim();
  if (name.isEmpty()) { _server.send(400, "text/plain", "Missing name"); return; }
  String path = "/" + name;
  if (sd.mkdir(path.c_str())) _server.send(200, "text/plain", "OK");
  else                         _server.send(500, "text/plain", "Failed to create folder");
}

static void handleDeleteFile() {
  String folder = _server.arg("f");
  String name   = _server.arg("n");
  if (folder.isEmpty() || name.isEmpty()) { _server.send(400, "text/plain", "Missing params"); return; }
  String path = "/" + folder + "/" + name;
  if (sd.remove(path.c_str())) _server.send(200, "text/plain", "OK");
  else                          _server.send(500, "text/plain", "Delete failed");
}

static File32   _sdUploadFile;
static String   _sdUploadFolder;

static void handleSdUploadChunk() {
  HTTPUpload& up = _server.upload();
  if (up.status == UPLOAD_FILE_START) {
    _sdUploadFolder = _server.arg("f");
    String path = "/" + _sdUploadFolder + "/" + up.filename;
    sd.remove(path.c_str());   // overwrite if exists
    _sdUploadFile.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    Serial.printf("[SD-up] %s\n", path.c_str());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (_sdUploadFile.isOpen())
      _sdUploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (_sdUploadFile.isOpen()) {
      _sdUploadFile.close();
      Serial.printf("[SD-up] Done %u bytes\n", up.totalSize);
    }
  }
}

static void handleSdUploadDone() {
  bool ok = _sdUploadFile.isOpen() ? false : true;  // closed = success
  if (_sdUploadFolder.isEmpty()) {
    _server.send(400, "text/plain", "No folder specified");
  } else {
    _server.send(200, "text/plain", "OK");
  }
}

// ── Firmware OTA upload ───────────────────────────────────────────────────────
static uint32_t _otaWritten = 0;

static void handleDoUpdate() {
  HTTPUpload& up = _server.upload();
  if (up.status == UPLOAD_FILE_START) {
    _otaWritten = 0;
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
    _otaWritten += up.currentSize;
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
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
    MDNS.addService("http", "tcp", 80);
  }

  _server.on("/",             HTTP_GET,    handleRoot);
  _server.on("/update",       HTTP_GET,    handleDashboard);
  _server.on("/api/info",     HTTP_GET,    handleApiInfo);
  _server.on("/api/folders",  HTTP_GET,    handleGetFolders);
  _server.on("/api/files",    HTTP_GET,    handleGetFiles);
  _server.on("/api/mkdir",    HTTP_POST,   handleMkdir);
  _server.on("/api/file",     HTTP_DELETE, handleDeleteFile);
  _server.on("/api/upload",   HTTP_POST,   handleSdUploadDone, handleSdUploadChunk);
  _server.on("/do_update",    HTTP_POST,   handleUpdateDone,   handleDoUpdate);
  _server.onNotFound([]() {
    _server.sendHeader("Location", "/update", true);
    _server.send(302, "text/plain", "");
  });

  _server.begin();
  _serverUp = true;
  Serial.printf("[WiFi] Server up — http://%s/update\n", _ip.c_str());
}

// ── Background WiFi task ──────────────────────────────────────────────────────
static void wifiTask(void*)
{
  _wm.setConfigPortalBlocking(false);
  _wm.setConnectTimeout(20);
  _wm.setConnectRetries(3);
  _wm.setConfigPortalTimeout(0);

  _wm.setAPCallback([](WiFiManager*) {
    Serial.println("[WiFi] Hotspot: IQRA-PAD-SETUP");
  });

  if (_wm.autoConnect("IQRA-PAD-SETUP")) {
    _connected = true;
    _ip = WiFi.localIP().toString();
    Serial.printf("[WiFi] Connected — %s\n", _ip.c_str());
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
  xTaskCreatePinnedToCore(wifiTask, "wifi_init", 8192, nullptr, 1, nullptr, 1);
}

void wifiOtaHandle()
{
  if (!_initDone) return;

  if (!_connected) {
    _wm.process();
    if (WiFi.status() == WL_CONNECTED) {
      _connected = true;
      _ip = WiFi.localIP().toString();
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
  delay(300);
  ESP.restart();
}
