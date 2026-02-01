### Jan 31st, 2025 
- Checked the E ink DIsplay with a first Page of Quran 

### Feb 1, 2025 — Home UI, Button Navigation, and SD Card SPI Debug

## Home Screen UI Work
- Created the basic home screen layout for the IQRA Pad E-Ink display.
- Set up the main menu navigation structure.
- Connected UI flow with button-based navigation.
- Built a simple state-based menu logic to move between screens.

---

## Button Connections (GPIO Mapping)

Directional buttons are connected to ESP32-S3 GPIO pins for UI navigation control.

```c
#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_LEFT    41
#define BTN_RIGHT   40
#define BTN_SELECT  42
```

**Notes**
- Configured buttons as digital input pins.
- Verified button press detection.
- Planning to add software debouncing in the input handler.

---

## E-Ink Display SPI Pins

Configured the E-Ink display using SPI interface with control pins for command/data, reset, and busy status.

```c
// --- EINK PINS ---
#define PIN_CS    10
#define PIN_DC     9
#define PIN_RST    5
#define PIN_BUSY   4
#define PIN_SCK   12
#define PIN_MOSI  11
#define PIN_MISO  13
```

**Notes**
- SPI lines connected and verified.
- BUSY pin used to check when display refresh is complete.
- Reset and DC control signals tested during display init.
- SPI bus shared with other devices using separate CS pins.

---

## SD Card SPI Setup and Troubleshooting

Connected SD card reader in SPI mode and tested communication.

```c
// --- SD CARD PINS ---
#define SD_CS    18
#define SD_SCK   12
#define SD_MOSI  11
#define SD_MISO  13
```

**Debug Steps Done**
- Checked SPI wiring and pin mapping.
- Verified separate chip select line for SD card.
- Confirmed power and ground connections.
- Tested SPI signals (MOSI/MISO/SCK).
- Checked SD card initialization behavior.
- Reviewing SPI clock speed settings for stability.

---

## Current Status
- Home screen UI ready.
- Button navigation working.
- E-Ink SPI interface configured.
- SD card SPI under testing and debug.
