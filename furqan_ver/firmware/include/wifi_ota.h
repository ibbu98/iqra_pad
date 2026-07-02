#pragma once
#include <Arduino.h>

// Connect to saved WiFi (or open setup hotspot on first run).
void wifiOtaInit(const char* deviceName = "iqra-pad", const char* fwVersion = "");

// Call every loop() — handles incoming web upload requests.
void wifiOtaHandle();

bool   wifiIsConnected();
String wifiIP();
String wifiSSID();

// Clear saved credentials and reboot — device re-opens setup hotspot.
void wifiReset();

// True while an SD file upload is in progress via the web dashboard.
bool wifiSdBusy();
