#include "ota_update.h"
#include "settings.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>

// ── Tiny JSON field extractor (avoids ArduinoJson dependency) ────────────────
static String jsonField(const String& json, const char* key)
{
  // Handles both "key":"value" and "key": "value" (space after colon is fine)
  String search = String("\"") + key + "\":";
  int start = json.indexOf(search);
  if (start < 0) return "";
  start += search.length();
  // Skip any whitespace between : and "
  while (start < (int)json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
  if (start >= (int)json.length() || json[start] != '"') return "";
  start++;  // skip opening quote
  int end = json.indexOf('"', start);
  if (end < 0) return "";
  return json.substring(start, end);
}

// ── Check for update ──────────────────────────────────────────────────────────
void otaCheck(OtaInfo& info)
{
  info.state    = OTA_CHECKING;
  info.latest   = "";
  info.dlUrl    = "";
  info.errorMsg = "";

  WiFiClientSecure client;
  client.setInsecure();   // skip cert check — acceptable for family OTA

  HTTPClient http;
  http.begin(client, OTA_VERSION_URL);
  http.setTimeout(10000);

  int code = http.GET();
  if (code != 200) {
    info.state    = OTA_ERROR;
    info.errorMsg = "Server error " + String(code);
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  info.latest = jsonField(body, "version");
  info.dlUrl  = jsonField(body, "url");

  if (info.latest.isEmpty()) {
    info.state    = OTA_ERROR;
    info.errorMsg = "Bad version file";
    return;
  }

  if (info.latest == FW_VERSION) {
    info.state = OTA_UP_TO_DATE;
  } else {
    info.state = OTA_AVAILABLE;
  }
}

// ── Download and flash ────────────────────────────────────────────────────────
void otaDownload(OtaInfo& info, void (*progressCb)(int pct))
{
  if (info.state != OTA_AVAILABLE || info.dlUrl.isEmpty()) return;

  info.state    = OTA_DOWNLOADING;
  info.progress = 0;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, info.dlUrl);
  http.setTimeout(60000);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub → CDN redirect

  int code = http.GET();
  if (code != 200) {
    info.state    = OTA_ERROR;
    info.errorMsg = "Download error " + String(code);
    http.end();
    return;
  }

  int total = http.getSize();
  if (total <= 0) {
    info.state    = OTA_ERROR;
    info.errorMsg = "Unknown file size";
    http.end();
    return;
  }

  if (!Update.begin(total)) {
    info.state    = OTA_ERROR;
    info.errorMsg = "Not enough flash";
    http.end();
    return;
  }

  WiFiClient* stream  = http.getStreamPtr();
  uint8_t     buf[1024];
  int         written = 0;
  int         lastPct = -1;

  while (http.connected() && written < total) {
    int avail = stream->available();
    if (!avail) { delay(1); continue; }

    int chunk = stream->readBytes(buf, min(avail, (int)sizeof(buf)));
    if (chunk <= 0) break;

    if (Update.write(buf, chunk) != (size_t)chunk) {
      info.state    = OTA_ERROR;
      info.errorMsg = "Flash write error";
      Update.abort();
      http.end();
      return;
    }

    written += chunk;
    int pct = (written * 100) / total;

    // Call progress callback only on every 5% step to limit display refreshes
    if (pct / 5 != lastPct / 5) {
      lastPct = pct;
      info.progress = pct;
      if (progressCb) progressCb(pct);
    }
  }
  http.end();

  if (!Update.end(true)) {
    info.state    = OTA_ERROR;
    info.errorMsg = "Flash finalise error";
    return;
  }

  info.state    = OTA_DONE;
  info.progress = 100;
  if (progressCb) progressCb(100);

  delay(1500);
  ESP.restart();
}
