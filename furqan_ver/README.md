# Iqra Pad — Furqan Version

ESP32-S3 N16R8 Islamic device (e-ink UI, SD card audio, Quran reader, Tasbeeh counter).

---

## Hardware

| Component | Part |
|---|---|
| MCU | ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM) |
| Display | Waveshare 4.2inch e-Paper Module (400×300, B/W) Rev 2.2 |
| SD Card | SD module via HSPI (SPI3) |

### Display Wiring (FSPI / SPI2)

| Display Pin | ESP32-S3 GPIO |
|---|---|
| DIN (MOSI) | 11 |
| CLK (SCK) | 12 |
| CS | 10 |
| DC | 9 |
| RST | 5 |
| BUSY | 4 |

> The e-ink display is **write-only** — no MISO pin. Pass `-1` for MISO in `SPI.begin()`.

### SD Card Wiring (HSPI / SPI3)

| SD Pin | ESP32-S3 GPIO |
|---|---|
| MOSI | 35 |
| MISO | 37 |
| SCK | 36 |
| CS | 34 |

---

## Known Hardware Issues & Fixes

### 1. E-Ink Display — Blank Screen / BUSY Timeout

**Symptom:** Display flashes (RST pulse visible) but nothing is drawn. Serial shows `BUSY Timeout!` on every `_Update_Full` call.

**Root cause A — RST duration too long:**

The Waveshare 4.2" module has an onboard power-off circuit. If RST is held LOW for too long, the circuit cuts power to the display driver, causing the reset to fail and BUSY to stay stuck HIGH indefinitely. Waveshare's own FAQ warns about this explicitly.

Additionally, with an RST pulse longer than ~2ms, the display's controller performs a **full hardware reset including OTP waveform reload**. When `SWRESET` (SPI command 0x12) is then sent, the controller triggers a second OTP reload — this takes ~4 seconds, far exceeding the library's 10-second busy timeout when combined with the subsequent refresh.

**Fix:** Use `display.init(115200, true, 2, false)` — **2ms RST pulse**. This is short enough to avoid the power-off circuit and avoid triggering the full OTP reload, so SWRESET completes in ~10ms and fast refresh works normally.

```cpp
// WRONG — triggers 4s SWRESET hang:
display.init(115200, true, 50, false);  // 50ms
display.init(115200, true, 10, false);  // 10ms

// CORRECT:
display.init(115200, true, 2, false);   // 2ms
```

**Root cause B — SWRESET busy-wait missing in GxEPD2 driver:**

The `GxEPD2_420_GDEY042T81` driver originally used `delay(10)` after sending SWRESET. The SSD1683 controller asserts BUSY HIGH during software reset, so 10ms was not always enough.

**Fix:** Patched `.pio/libdeps/.../GxEPD2/src/gdey/GxEPD2_420_GDEY042T81.cpp` — replaced `delay(10)` after SWRESET with `_waitWhileBusy("SWRESET", 200)`.

> **Note:** This library patch lives inside `.pio/` which is gitignored. Re-apply it after a clean install:
> In `_InitDisplay()`, change:
> ```cpp
> _writeCommand(0x12);  //SWRESET
> delay(10);
> ```
> to:
> ```cpp
> _writeCommand(0x12);  //SWRESET
> _waitWhileBusy("SWRESET", 200);
> ```

---

### 2. SD Card — Not Initializing / Card Reads as 0 MB

**Symptom:** `sd.begin()` fails or card reports 0 MB, type SD1, after working fine at lower SPI speeds.

**Root cause A — No 5V reaching the SD card module:**

The SD card breakout module requires **5V on its VCC pin** (it has an onboard LDO that regulates down to 3.3V for the card itself). The ESP32-S3 DevKit's 5V pin is connected directly to USB VBUS — it only provides 5V when the board is powered via USB. If the devkit is powered from any other source (battery, 3.3V rail, etc.), the 5V pin has no output and the SD module receives no power at all.

**Fix:** Always power the SD card module from the USB VBUS 5V pin, and ensure the device is USB-powered during development. For battery-powered operation, use a boost converter to generate 5V for the SD module.

**Root cause B — HSPI pin mapping lost after `sd.end()`:**

In the SD speed-sweep test, calling `sd.end()` inside the sweep loop also called `spi.end()` internally, clearing the HSPI pin mapping. The subsequent `sd.begin()` at the end of the test (for the final info dump) used a fresh SPI object with no pin mapping set.

**Fix:** Re-call `spi.begin(SCK, MISO, MOSI, -1)` with explicit pins before the final `sd.begin()` call, and add a short `delay(20)` to let the bus settle.

**Root cause C — SPI speed too conservative:**

Initial mount attempts at 400kHz or 4MHz failed due to timing sensitivity with DEDICATED_SPI mode. Confirmed working at **25MHz** using `SdSpiConfig` with `DEDICATED_SPI`.

```cpp
spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1);
delay(20);
SdSpiConfig cfg(SD_CS_PIN, DEDICATED_SPI, 25000000UL, &spiSD);
sd.begin(cfg);
```

---

## SPI Bus Separation

Two completely separate SPI buses are used to avoid conflicts:

| Bus | ESP-IDF Name | Arduino Name | Used For |
|---|---|---|---|
| SPI2 | FSPI | `SPI` (default) | E-ink display |
| SPI3 | HSPI | `SPIClass spiSD(HSPI)` | SD card |

This is critical — sharing a single SPI bus between the display and SD card causes both to fail.

---

## Build

```bash
# Main firmware
pio run -e esp32-s3-n16r8 --target upload

# SD card speed test (spare devkit, no display needed)
pio run -e sd-test --target upload
```
