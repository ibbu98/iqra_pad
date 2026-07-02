#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <algorithm>
#include <GxEPD2_BW.h>
#include <freertos/task.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "quran_data.h"

#include "ui.h"
#include "buttons.h"
#include "tasbeeh.h"
#include "sd_card.h"
#include "audio_player.h"
#include "wifi_ota.h"
#include "ota_update.h"
#include "settings.h"

// ── Core-0 audio task ─────────────────────────────────────────────────────────
// Runs audioLoop() on Core 0 (PRO_CPU) so display refreshes on Core 1 (APP_CPU)
// never interrupt audio decoding and output.
static TaskHandle_t _audioTaskHandle = nullptr;

static void audioTaskFn(void*)
{
  for (;;) {
    audioLoop();
    vTaskDelay(pdMS_TO_TICKS(1));  // yield; when playing, i2s_write() also yields naturally
  }
}

// --- Display pins (FSPI / SPI2) ---
#define PIN_CS   10
#define PIN_DC   9
#define PIN_RST  5
#define PIN_BUSY 4
#define PIN_SCK  12
#define PIN_MOSI 11

MyDisplay display(GxEPD2_420_GDEY042T81(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

// --- Pages ---
enum Page {
  PAGE_HOME,
  PAGE_RECITER, PAGE_SURAH, PAGE_PLAYING,   // audio
  PAGE_TASBEEH,
  PAGE_QURAN_MENU,                           // 4-tile Quran menu
  PAGE_SURAH_INDEX, PAGE_JUZ_INDEX,          // indexes
  PAGE_BOOKMARKS, PAGE_QURAN_VIEW,           // bookmarks & viewer
  PAGE_SETTINGS,                             // settings menu
  PAGE_WIFI_SETTINGS,                        // wifi status / reset
  PAGE_BT_INFO,                              // bluetooth info
  PAGE_ABOUT_DEVICE,                         // about device info
  PAGE_OTA_UPDATE,                           // software update
  PAGE_SD_BUSY                               // shown while WiFi SD upload is running
};
Page currentPage = PAGE_HOME;

// --- Home ---
int homeSelection = 0;

// --- Reciter browser ---
std::vector<String> reciterList;
int reciterSel = 0;

// --- Surah browser ---
std::vector<String> surahList;
int surahSel = 0;
int surahTop = 0;
String currentReciter;

// --- Quran viewer ---
static int              quranType      = 0;      // 0=13-line  1=15-line
static int              quranMenuSel   = 0;
static int              surahIdxSel    = 0;
static int              surahIdxTop    = 0;
static int              juzIdxSel      = 0;
static int              juzIdxTop      = 0;
static int              bmSel          = 0;
static int              bmTop          = 0;
static int              quranPage      = 1;
static int              quranLastRead[2] = { 1, 1 };
static std::vector<int> quranBookmarks;
static const int        QURAN_PAGES    = 604;

// --- Settings ---
static int     settingsSel = 0;
static OtaInfo otaInfo;

// --- Power hold: only arm after buttons have been released once ---
static bool g_powerArmed = false;

// --- Volume ---
static uint32_t lastVolUpdate    = 0;
static float    currentVolume    = 0.5f;
static float    lastDisplayedVol = 0.5f;
static uint32_t lastVolDisplay   = 0;

// ── Draw helpers ─────────────────────────────────────────────────────────────
static void drawHome(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do { drawHomePage(display, homeSelection, wifiIsConnected()); } while (display.nextPage());
}

static void drawReciters(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do { drawReciterPage(display, reciterList, reciterSel); } while (display.nextPage());
}

static void drawSurahs(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do { drawSurahPage(display, currentReciter, surahList, surahSel, surahTop); } while (display.nextPage());
}

static void drawQMenu(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, 400, 300);
  display.firstPage();
  do { drawQuranMenuPage(display, quranType, quranMenuSel); } while (display.nextPage());
}

static void drawSurahIdx(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, 400, 300);
  display.firstPage();
  do { drawSurahIndexPage(display, quranType, surahIdxSel, surahIdxTop); } while (display.nextPage());
}

static void drawJuzIdx(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, 400, 300);
  display.firstPage();
  do { drawJuzIndexPage(display, quranType, juzIdxSel, juzIdxTop); } while (display.nextPage());
}

static void drawBkmks(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, 400, 300);
  display.firstPage();
  do { drawBookmarksPage(display, quranBookmarks, bmSel, bmTop); } while (display.nextPage());
}

static void drawSettings(bool full = true)
{
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, 400, 300);
  display.firstPage();
  do { drawSettingsPage(display, settingsSel); } while (display.nextPage());
}

static void drawAboutDevice()
{
  // SD card total size — no freeClusterCount() as it scans the full FAT (too slow)
  char sdBuf[16] = "No card";
  if (sd.card()) {
    uint32_t sectors = sd.card()->sectorCount();
    if (sectors > 0) {
      uint32_t mb = (uint32_t)(((uint64_t)sectors * 512) >> 20);
      if (mb >= 1000) snprintf(sdBuf, sizeof(sdBuf), "%u GB", mb / 1024);
      else            snprintf(sdBuf, sizeof(sdBuf), "%u MB", mb);
    }
  }

  String ip = wifiIP();
  display.setFullWindow();
  display.firstPage();
  do { drawAboutDevicePage(display, ip, sdBuf); } while (display.nextPage());
}

static void drawBtInfo()
{
  display.setFullWindow();
  display.firstPage();
  do { drawBtInfoPage(display); } while (display.nextPage());
}

static void drawSdBusy()
{
  display.setFullWindow();
  display.firstPage();
  do { drawSdBusyPage(display); } while (display.nextPage());
}

static void drawWifiSettings()
{
  display.setFullWindow();
  display.firstPage();
  do {
    drawWifiSettingsPage(display, wifiIsConnected(), wifiSSID(), wifiIP());
  } while (display.nextPage());
}

static void drawOta()
{
  display.setFullWindow();
  display.firstPage();
  do {
    drawOtaPage(display, FW_VERSION,
                otaInfo.latest.c_str(), (int)otaInfo.state,
                otaInfo.progress, otaInfo.errorMsg.c_str());
  } while (display.nextPage());
}

static void drawOtaProgress()  // partial refresh — covers all state-content area
{
  display.setPartialWindow(0, 40, 400, 260);
  display.firstPage();
  do {
    drawOtaPage(display, FW_VERSION,
                otaInfo.latest.c_str(), (int)otaInfo.state,
                otaInfo.progress, otaInfo.errorMsg.c_str());
  } while (display.nextPage());
}

static void drawQView(bool full = true)
{
  bool bm = std::find(quranBookmarks.begin(), quranBookmarks.end(), quranPage)
            != quranBookmarks.end();
  if (full) display.setFullWindow();
  else      display.setPartialWindow(0, 0, 400, 300);
  display.firstPage();
  do { drawQuranViewPage(display, quranType, quranPage, bm); } while (display.nextPage());
}

static void drawPlaying()
{
  display.setFullWindow();
  display.firstPage();
  do {
    drawNowPlayingPage(display, currentReciter,
                       surahList[surahSel], surahSel + 1, surahList.size(),
                       currentVolume);
  } while (display.nextPage());
  lastDisplayedVol = currentVolume;
  lastVolDisplay   = millis();
}

static void updateScroll(int sel, int total, int visible, int &top)
{
  if (sel < top) top = sel;
  if (sel >= top + visible) top = sel - visible + 1;
  top = constrain(top, 0, max(0, total - visible));
}

// ── Start playback of current surahSel ───────────────────────────────────────
static void startPlaying()
{
  audioPlay(currentReciter, surahList[surahSel]);
  currentPage = PAGE_PLAYING;
  drawPlaying();
}

// ── Volume: read pot, update audio gain ──────────────────────────────────────
static void updateVolume()
{
  int raw = analogRead(VOL_ADC_PIN);
  float v = raw / 4095.0f;
  audioSetVolume(v);
  currentVolume = v;
}

// ── Power off / on via light sleep ───────────────────────────────────────────
static void enterSleep()
{
  // Stop audio before sleeping
  audioStop();
  vTaskDelay(pdMS_TO_TICKS(120));

  // Show power-off splash, hold 3 s
  display.setFullWindow();
  display.firstPage();
  do { drawSplashPage(display, FW_VERSION); } while (display.nextPage());
  delay(3000);

  // Wait for both buttons to be physically released before sleeping,
  // otherwise the LOW level would immediately re-trigger wakeup.
  while (digitalRead(BTN_SELECT) == LOW || digitalRead(BTN_BACK) == LOW) delay(10);
  delay(200);   // debounce

  // Hibernate display — image is retained, SPI lines go idle
  display.hibernate();

  // Configure light-sleep wakeup: either SELECT or BACK button press
  gpio_wakeup_enable((gpio_num_t)BTN_SELECT, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_BACK,   GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  Serial.println("[PWR] entering light sleep");
  Serial.flush();

  esp_light_sleep_start();   // ← device sleeps here

  // ── WAKEUP POINT ─────────────────────────────────────────────────────────
  Serial.println("[PWR] woke from light sleep");

  gpio_wakeup_disable((gpio_num_t)BTN_SELECT);
  gpio_wakeup_disable((gpio_num_t)BTN_BACK);

  // Re-initialise display (was hibernated)
  SPI.begin(PIN_SCK, -1, PIN_MOSI);
  display.init(115200, false, 2, false);
  display.setRotation(0);

  // Show power-on splash for 3 s
  display.setFullWindow();
  display.firstPage();
  do { drawSplashPage(display, FW_VERSION); } while (display.nextPage());
  delay(3000);

  // Disarm power detection — loop() re-arms it once buttons are released,
  // preventing an immediate re-sleep if the wakeup button is still held.
  g_powerArmed = false;

  // Return to home
  currentPage   = PAGE_HOME;
  homeSelection = 0;
  drawHome(true);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);
  delay(2000);

  setupButtons();

  // Display (FSPI, write-only)
  SPI.begin(PIN_SCK, -1, PIN_MOSI);
  display.init(115200, true, 2, false);
  display.setRotation(0);
  tasbeeh.init(&display);

  // SD card (HSPI — SCK=14 MOSI=21 MISO=47 CS=18  VCC=5V)
  if (setupSD()) {
    testSD();
    reciterList = getReciterFolders();
    Serial.printf("[SD] %d reciter folder(s)\n", (int)reciterList.size());
  }

  // Audio (I2S — BCLK=6 LRC=7 DOUT=8  vol pot=17)
  audioInit();   // must complete before task is created (initialises I2S + mutex)
  xTaskCreatePinnedToCore(audioTaskFn, "audio", 16384, nullptr, 2,
                          &_audioTaskHandle, 0 /*Core 0*/);
  pinMode(VOL_ADC_PIN, INPUT);

  // Boot splash — 2 s then go straight to home
  display.setFullWindow();
  display.firstPage();
  do { drawSplashPage(display, FW_VERSION); } while (display.nextPage());
  delay(2000);

  // Home screen appears immediately — user is not trapped on the splash
  drawHome(true);

  // WiFi starts AFTER home is drawn (up to 5 s to find saved network,
  // then opens "IQRA-PAD-SETUP" hotspot in background).
  // loop() starts right after setup() returns, so wifiOtaHandle() runs
  // fast enough for the captive portal to respond to the phone.
  wifiOtaInit("iqra-pad", FW_VERSION);

  Serial.println("[setup] done");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop()
{
  // ── OTA web server ────────────────────────────────────────────────────────
  wifiOtaHandle();

  // ── Auto-return from SD busy screen when upload finishes ─────────────────
  if (currentPage == PAGE_SD_BUSY && !wifiSdBusy()) {
    currentPage = PAGE_HOME;
    drawHome(true);
    return;
  }

  // ── Auto-refresh home WiFi symbol when connection state changes ───────────
  {
    static bool _prevWifi = false;
    bool _nowWifi = wifiIsConnected();
    if (_nowWifi != _prevWifi) {
      _prevWifi = _nowWifi;
      if (currentPage == PAGE_HOME) drawHome(false);
    }
  }

  // ── Power button: SELECT + BACK held together for 3 s → sleep ────────────
  {
    static uint32_t _pwrHoldStart = 0;
    bool selDown  = (digitalRead(BTN_SELECT) == LOW);
    bool backDown = (digitalRead(BTN_BACK)   == LOW);

    // Arm only after both buttons are released — prevents accidental trigger
    // right after the boot splash (or wakeup splash) while user is still holding.
    if (!selDown && !backDown) g_powerArmed = true;

    if (g_powerArmed && selDown && backDown) {
      if (!_pwrHoldStart) _pwrHoldStart = millis();
      else if (millis() - _pwrHoldStart >= 3000) {
        _pwrHoldStart = 0;
        enterSleep();
        return;
      }
    } else {
      _pwrHoldStart = 0;
    }
  }

  // ── Volume pot — update audio gain every 200ms ───────────────────────────
  if (millis() - lastVolUpdate > 200) {
    updateVolume();
    lastVolUpdate = millis();
  }

  // ── Live volume bar on Now Playing ────────────────────────────────────────
  if (currentPage == PAGE_PLAYING && millis() - lastVolDisplay > 500) {
    float delta = currentVolume - lastDisplayedVol;
    if (delta < -0.03f || delta > 0.03f) {
      display.setPartialWindow(0, VOL_REGION_Y, 400, VOL_REGION_H);
      display.firstPage();
      do { drawVolRegion(display, currentVolume); } while (display.nextPage());
      lastDisplayedVol = currentVolume;
      lastVolDisplay   = millis();
    }
  }

  // ── Auto-advance when a surah finishes naturally ──────────────────────────
  if (currentPage == PAGE_PLAYING && audioJustFinished()) {
    if (surahSel < (int)surahList.size() - 1) {
      surahSel++;
      startPlaying();   // draw now-playing + start next track
    } else {
      // Last surah — return to surah list
      audioStop();
      currentPage = PAGE_SURAH;
      drawSurahs(true);
    }
    return;
  }

  int act = readButtons();

  // ── Tasbeeh hold-to-reset ─────────────────────────────────────────────────
  if (currentPage == PAGE_TASBEEH) {
    static uint32_t selHoldStart = 0;
    if (digitalRead(BTN_SELECT) == LOW) {
      if (!selHoldStart) selHoldStart = millis();
      else if (millis() - selHoldStart >= 3000) {
        selHoldStart = 0;
        tasbeeh.resetCount();
        delay(30);
        return;
      }
    } else selHoldStart = 0;
  }

  if (act == 0) return;

  // ================================================================
  // HOME
  // ================================================================
  if (currentPage == PAGE_HOME)
  {
    int old = homeSelection;
    if      (act == ACT_NEXT) homeSelection = (homeSelection + 1) % 9;
    else if (act == ACT_PREV) homeSelection = (homeSelection - 1 < 0) ? 8 : homeSelection - 1;
    else if (act == ACT_SELECT) {
      if (homeSelection == 0 || homeSelection == 1) {  // 13-line / 15-line Quran
        if (wifiSdBusy()) { currentPage = PAGE_SD_BUSY; drawSdBusy(); return; }
        quranType    = homeSelection;
        quranMenuSel = 0;
        currentPage  = PAGE_QURAN_MENU;
        drawQMenu(true);
        return;
      }
      if (homeSelection == 3) {             // Quran MP3
        if (wifiSdBusy()) { currentPage = PAGE_SD_BUSY; drawSdBusy(); return; }
        reciterList = getReciterFolders();  // reload so new folders appear without reboot
        reciterSel = 0;
        currentPage = PAGE_RECITER;
        drawReciters(true);
        return;
      }
      if (homeSelection == 7) {             // Tasbeeh
        currentPage = PAGE_TASBEEH;
        tasbeeh.enter();
        delay(60);
        return;
      }
      if (homeSelection == 8) {             // Settings
        settingsSel = 0;
        currentPage = PAGE_SETTINGS;
        drawSettings(true);
        return;
      }
    }
    if (old != homeSelection) drawHome(false);
  }

  // ================================================================
  // RECITER LIST
  // ================================================================
  else if (currentPage == PAGE_RECITER)
  {
    int old = reciterSel;
    if      (act == ACT_NEXT) reciterSel = min(reciterSel + 1, (int)reciterList.size() - 1);
    else if (act == ACT_PREV) reciterSel = max(reciterSel - 1, 0);
    else if (act == ACT_SELECT && !reciterList.empty()) {
      currentReciter = reciterList[reciterSel];
      surahList = getSurahFiles(currentReciter);
      surahSel  = 0;
      surahTop  = 0;
      Serial.printf("[SD] %s — %d surah(s)\n", currentReciter.c_str(), (int)surahList.size());
      currentPage = PAGE_SURAH;
      drawSurahs(true);
      return;
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_HOME;
      display.setRotation(0);
      drawHome(true);
      return;
    }
    if (old != reciterSel) drawReciters(false);
  }

  // ================================================================
  // SURAH LIST
  // ================================================================
  else if (currentPage == PAGE_SURAH)
  {
    int old = surahSel;
    if      (act == ACT_NEXT) surahSel = min(surahSel + 1, (int)surahList.size() - 1);
    else if (act == ACT_PREV) surahSel = max(surahSel - 1, 0);
    else if (act == ACT_SELECT) {
      if (surahList.empty()) return;
      if (wifiSdBusy()) { currentPage = PAGE_SD_BUSY; drawSdBusy(); return; }
      startPlaying();   // enter NOW PLAYING
      return;
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_RECITER;
      drawReciters(true);
      return;
    }
    if (old != surahSel) {
      updateScroll(surahSel, (int)surahList.size(), 9, surahTop);
      drawSurahs(false);
    }
  }

  // ================================================================
  // NOW PLAYING
  // ================================================================
  else if (currentPage == PAGE_PLAYING)
  {
    if (act == ACT_NEXT) {
      if (surahSel < (int)surahList.size() - 1) {
        surahSel++;
        startPlaying();
      }
    }
    else if (act == ACT_PREV) {
      if (surahSel > 0) {
        surahSel--;
        startPlaying();
      }
    }
    else if (act == ACT_BACK) {
      audioStop();
      currentPage = PAGE_SURAH;
      drawSurahs(true);
    }
    // SELECT while playing = pause/resume (future: add pause support)
  }

  // ================================================================
  // TASBEEH
  // ================================================================
  else if (currentPage == PAGE_TASBEEH)
  {
    if (act == ACT_PREV) tasbeeh.onUp();
    else if (act == ACT_BACK) {
      // exitPage() calls renderFull() if partial-refresh was used, putting the
      // SSD1683 back in a clean full-refresh state before we draw the home page.
      tasbeeh.exitPage();
      currentPage = PAGE_HOME;
      display.setRotation(0);
        drawHome(true);
      return;
    }
  }

  // ================================================================
  // QURAN MENU  (4-tile: Surah Index / Juz Index / Bookmarks / Last Read)
  // ================================================================
  else if (currentPage == PAGE_QURAN_MENU)
  {
    int old = quranMenuSel;
    if      (act == ACT_NEXT) quranMenuSel = (quranMenuSel + 1) % 4;
    else if (act == ACT_PREV) quranMenuSel = (quranMenuSel - 1 + 4) % 4;
    else if (act == ACT_SELECT) {
      if (quranMenuSel == 0) {                         // Surah Index
        surahIdxSel = 0; surahIdxTop = 0;
        currentPage = PAGE_SURAH_INDEX;
        drawSurahIdx(true);
        return;
      }
      if (quranMenuSel == 1) {                         // Juz Index
        juzIdxSel = 0; juzIdxTop = 0;
        currentPage = PAGE_JUZ_INDEX;
        drawJuzIdx(true);
        return;
      }
      if (quranMenuSel == 2) {                         // Bookmarks
        bmSel = 0; bmTop = 0;
        currentPage = PAGE_BOOKMARKS;
        drawBkmks(true);
        return;
      }
      if (quranMenuSel == 3) {                         // Last Read
        quranPage = quranLastRead[quranType];
        currentPage = PAGE_QURAN_VIEW;
        drawQView(true);
        return;
      }
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_HOME;
      drawHome(true);
      return;
    }
    if (old != quranMenuSel) drawQMenu(false);
  }

  // ================================================================
  // SURAH INDEX
  // ================================================================
  else if (currentPage == PAGE_SURAH_INDEX)
  {
    int old = surahIdxSel;
    if      (act == ACT_NEXT) surahIdxSel = min(surahIdxSel + 1, 113);
    else if (act == ACT_PREV) surahIdxSel = max(surahIdxSel - 1, 0);
    else if (act == ACT_SELECT) {
      uint16_t pg = (quranType == 0) ? SURAH_TABLE[surahIdxSel].page13
                                     : SURAH_TABLE[surahIdxSel].page15;
      quranPage = (pg == 0) ? 1 : pg;
      quranLastRead[quranType] = quranPage;
      currentPage = PAGE_QURAN_VIEW;
      drawQView(true);
      return;
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_QURAN_MENU;
      drawQMenu(true);
      return;
    }
    if (old != surahIdxSel) {
      updateScroll(surahIdxSel, 114, PG_ITEMS, surahIdxTop);
      drawSurahIdx(false);
    }
  }

  // ================================================================
  // JUZ INDEX
  // ================================================================
  else if (currentPage == PAGE_JUZ_INDEX)
  {
    int old = juzIdxSel;
    if      (act == ACT_NEXT) juzIdxSel = min(juzIdxSel + 1, 29);
    else if (act == ACT_PREV) juzIdxSel = max(juzIdxSel - 1, 0);
    else if (act == ACT_SELECT) {
      uint16_t pg = (quranType == 0) ? JUZ_PAGE13[juzIdxSel]
                                     : JUZ_PAGE15[juzIdxSel];
      quranPage = (pg == 0) ? 1 : pg;
      quranLastRead[quranType] = quranPage;
      currentPage = PAGE_QURAN_VIEW;
      drawQView(true);
      return;
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_QURAN_MENU;
      drawQMenu(true);
      return;
    }
    if (old != juzIdxSel) {
      updateScroll(juzIdxSel, 30, PG_ITEMS, juzIdxTop);
      drawJuzIdx(false);
    }
  }

  // ================================================================
  // BOOKMARKS
  // ================================================================
  else if (currentPage == PAGE_BOOKMARKS)
  {
    if (quranBookmarks.empty()) {
      if (act == ACT_BACK) {
        currentPage = PAGE_QURAN_MENU;
        drawQMenu(true);
        return;
      }
    } else {
      int old = bmSel;
      if      (act == ACT_NEXT) bmSel = min(bmSel + 1, (int)quranBookmarks.size() - 1);
      else if (act == ACT_PREV) bmSel = max(bmSel - 1, 0);
      else if (act == ACT_SELECT) {
        quranPage = quranBookmarks[bmSel];
        quranLastRead[quranType] = quranPage;
        currentPage = PAGE_QURAN_VIEW;
        drawQView(true);
        return;
      }
      else if (act == ACT_BACK) {
        bmSel = 0; bmTop = 0;
        currentPage = PAGE_QURAN_MENU;
        drawQMenu(true);
        return;
      }
      if (old != bmSel) {
        updateScroll(bmSel, (int)quranBookmarks.size(), PG_ITEMS, bmTop);
        drawBkmks(false);
      }
    }
  }

  // ================================================================
  // QURAN VIEW  (placeholder — wire image loading here later)
  // ================================================================
  else if (currentPage == PAGE_QURAN_VIEW)
  {
    if (act == ACT_NEXT) {
      if (quranPage < QURAN_PAGES) {
        quranPage++;
        quranLastRead[quranType] = quranPage;
        drawQView(false);  // always partial — fast page turn
      }
    }
    else if (act == ACT_PREV) {
      if (quranPage > 1) {
        quranPage--;
        quranLastRead[quranType] = quranPage;
        drawQView(false);  // always partial — fast page turn
      }
    }
    else if (act == ACT_SELECT) {
      // Toggle bookmark on the current page
      auto it = std::find(quranBookmarks.begin(), quranBookmarks.end(), quranPage);
      if (it != quranBookmarks.end())
        quranBookmarks.erase(it);
      else
        quranBookmarks.push_back(quranPage);
      drawQView(false);
    }
    else if (act == ACT_BACK) {
      drawQView(true);  // full refresh on exit to clear any ghosting before menu
      currentPage = PAGE_QURAN_MENU;
      drawQMenu(true);
      return;
    }
  }

  // ================================================================
  // SETTINGS MENU
  // ================================================================
  else if (currentPage == PAGE_SETTINGS)
  {
    int old = settingsSel;
    if      (act == ACT_NEXT) settingsSel = min(settingsSel + 1, 3);
    else if (act == ACT_PREV) settingsSel = max(settingsSel - 1, 0);
    else if (act == ACT_SELECT) {
      if (settingsSel == 0) {           // WiFi
        currentPage = PAGE_WIFI_SETTINGS;
        drawWifiSettings();
      } else if (settingsSel == 1) {    // Bluetooth
        currentPage = PAGE_BT_INFO;
        drawBtInfo();
      } else if (settingsSel == 2) {    // Software Update
        otaInfo = OtaInfo();
        currentPage = PAGE_OTA_UPDATE;
        drawOta();
      } else {                          // About
        currentPage = PAGE_ABOUT_DEVICE;
        drawAboutDevice();
      }
      return;
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_HOME;
      drawHome(true);
      return;
    }
    if (old != settingsSel) drawSettings(false);
  }

  // ================================================================
  // ABOUT DEVICE
  // ================================================================
  else if (currentPage == PAGE_ABOUT_DEVICE)
  {
    if (act == ACT_BACK) {
      currentPage = PAGE_SETTINGS;
      drawSettings(true);
      return;
    }
  }

  // ================================================================
  // BLUETOOTH INFO
  // ================================================================
  else if (currentPage == PAGE_BT_INFO)
  {
    if (act == ACT_BACK) {
      currentPage = PAGE_SETTINGS;
      drawSettings(true);
      return;
    }
  }

  // ================================================================
  // WIFI SETTINGS
  // ================================================================
  else if (currentPage == PAGE_WIFI_SETTINGS)
  {
    if (act == ACT_SELECT && wifiIsConnected()) {
      // Clear credentials and reboot into setup portal
      wifiReset();   // does not return — reboots
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_SETTINGS;
      drawSettings(true);
      return;
    }
  }

  // ================================================================
  // SD BUSY
  // ================================================================
  else if (currentPage == PAGE_SD_BUSY)
  {
    if (act == ACT_BACK) {
      currentPage = PAGE_HOME;
      drawHome(true);
      return;
    }
  }

  // ================================================================
  // OTA UPDATE
  // ================================================================
  else if (currentPage == PAGE_OTA_UPDATE)
  {
    if (act == ACT_BACK &&
        otaInfo.state != OTA_DOWNLOADING && otaInfo.state != OTA_DONE) {
      currentPage = PAGE_SETTINGS;
      drawSettings(true);
      return;
    }

    if (act == ACT_SELECT) {
      if (otaInfo.state == OTA_IDLE || otaInfo.state == OTA_ERROR ||
          otaInfo.state == OTA_UP_TO_DATE) {
        // Start version check
        audioStop();
        otaInfo = OtaInfo();
        drawOta();                     // show "Checking..."
        otaCheck(otaInfo);            // blocking HTTP request
        drawOta();                     // show result
      }
      else if (otaInfo.state == OTA_AVAILABLE) {
        // Start download + flash
        drawOta();
        otaDownload(otaInfo, [](int pct) {
          otaInfo.progress = pct;
          drawOtaProgress();           // partial refresh progress bar
        });
        // otaDownload reboots on success; if we're here there was an error
        drawOta();
      }
    }
  }

}
