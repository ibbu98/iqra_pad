#pragma once
#include <Arduino.h>

enum OtaState {
  OTA_IDLE,
  OTA_CHECKING,
  OTA_UP_TO_DATE,
  OTA_AVAILABLE,
  OTA_DOWNLOADING,
  OTA_INSTALLING,
  OTA_DONE,
  OTA_ERROR
};

struct OtaInfo {
  OtaState state     = OTA_IDLE;
  String   latest    = "";   // version string from server
  String   dlUrl     = "";   // download URL from server
  int      progress  = 0;    // 0-100 during download
  String   errorMsg  = "";
};

// Check server for latest version. Fills info.state + info.latest + info.dlUrl.
// Blocking (HTTP request). Call only when WiFi is connected.
void otaCheck(OtaInfo& info);

// Download and flash new firmware. info must be in OTA_AVAILABLE state.
// progressCb(pct) is called every ~5% so caller can update the display.
// Device restarts automatically on success.
void otaDownload(OtaInfo& info, void (*progressCb)(int pct));
