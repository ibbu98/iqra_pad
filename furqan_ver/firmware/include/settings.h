#pragma once

// ── Firmware version ──────────────────────────────────────────────────────────
#define FW_VERSION    "1.0.8"

// ── Device identity ───────────────────────────────────────────────────────────
#define DEVICE_NAME   "IQRA PAD"
#define DEVICE_MODEL  "Furqan Edition"
#define DEVICE_USER   "Ibrahim"

// ── OTA update server (GitHub raw) ───────────────────────────────────────────
// 1. Create a GitHub repo (or use your existing one).
// 2. Add a file at:  ota/version.json  in that repo.
//    Contents example:
//      {"version":"1.0.1","url":"https://github.com/YOUR_USER/YOUR_REPO/releases/download/v1.0.1/firmware.bin"}
// 3. Replace YOUR_USER and YOUR_REPO below.
#define OTA_VERSION_URL \
  "https://raw.githubusercontent.com/ibbu98/iqra_pad/main/ota/version.json"
