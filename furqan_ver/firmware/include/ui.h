#ifndef UI_H
#define UI_H

#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

// IMPORTANT: This file must exist in your 'src' folder
#include "FreeMonoBold12pt7b.h"

#include <vector> // Needed for the list

// Define Display Driver (GDEY042T81)
typedef GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> MyDisplay;

void drawHomePage(MyDisplay &display, int selectedItem, bool wifiOn = false);
void drawMusicPage(MyDisplay &display, std::vector<String> &files, int selectedIndex);
void drawReciterPage(MyDisplay &display, std::vector<String> &folders, int selectedIndex);
void drawSurahPage(MyDisplay &display, const String &reciterName, std::vector<String> &surahs, int selectedIndex, int topIndex);
// Volume region coordinates (used for partial-refresh updates)
#define VOL_REGION_Y  104
#define VOL_REGION_H  144

// Layout constants (shared between ui.cpp and main.cpp)
#define PG_HEADER_Y   22
#define PG_LINE_Y     28
#define PG_LIST_TOP   32
#define PG_ITEM_H     26
#define PG_ITEMS       9
#define PG_SCROLL_W   10

void drawVolRegion(MyDisplay &display, float vol);
void drawNowPlayingPage(MyDisplay &display, const String &reciter, const String &surahName, int surahNum, int totalSurahs, float vol = 0.5f);

// ── Quran viewer pages ────────────────────────────────────────────────────────
void drawQuranMenuPage(MyDisplay &display, int quranType, int selItem);
void drawSurahIndexPage(MyDisplay &display, int quranType, int selIdx, int topIdx);
void drawJuzIndexPage(MyDisplay &display, int quranType, int selIdx, int topIdx);
void drawBookmarksPage(MyDisplay &display, const std::vector<int> &bookmarks, int selIdx, int topIdx);
void drawQuranViewPage(MyDisplay &display, int quranType, int pageNum, bool isBookmarked);

void drawSplashPage(MyDisplay &display, const char* fwVersion = "");
void drawBtInfoPage(MyDisplay &display);
void drawAboutDevicePage(MyDisplay &display, const String& wifiIp, const char* sdSize);
void drawSdBusyPage(MyDisplay &display);

// ── Settings pages ────────────────────────────────────────────────────────────
void drawSettingsPage(MyDisplay &display, int selItem);
void drawWifiSettingsPage(MyDisplay &display, bool connected,
                          const String &ssid, const String &ip);
// otaState matches OtaState enum values; pass progress 0-100 when downloading.
void drawOtaPage(MyDisplay &display, const char* currentVer,
                 const char* latestVer, int otaState, int progress,
                 const char* errMsg = "");

#endif