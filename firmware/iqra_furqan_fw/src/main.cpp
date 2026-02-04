#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <GxEPD2_BW.h>

#include "ui.h"
#include "buttons.h"
#include "sd_card.h"
#include "13_line_quran_pages.h"
#include "13_quran_sm.h"

// --- PINS ---
#define PIN_CS   10
#define PIN_DC   9
#define PIN_RST  5
#define PIN_BUSY 4
#define PIN_SCK  12
#define PIN_MOSI 11
#define PIN_MISO 13

MyDisplay display(GxEPD2_420_GDEY042T81(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

enum Page {
  PAGE_HOME,
  PAGE_MUSIC,
  PAGE_QURAN13
};

Page currentPage = PAGE_HOME;

int homeSelection = 0;
int musicSelection = 0;
std::vector<String> songList;

Quran13SM q13sm;

static void drawHome()
{
  display.setFullWindow();
  display.firstPage();
  do { drawHomePage(display, homeSelection); } while (display.nextPage());
}

static void drawMusic()
{
  display.setFullWindow();
  display.firstPage();
  do { drawMusicPage(display, songList, musicSelection); } while (display.nextPage());
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  setupButtons();

  display.init(115200, true, 2, false);
  display.setRotation(0);

  if (setupSD()) {
    songList = getMusicFiles();
  }

  q13sm.init(&display);
  drawHome();
}

void loop()
{
  int act = readButtons();
  if (act == 0) return;

  // ---------------- HOME ----------------
  if (currentPage == PAGE_HOME)
  {
    int oldSel = homeSelection;

    // With your 4-button mapping:
    // PREV/NEXT scroll home items
    if (act == ACT_NEXT) homeSelection = (homeSelection + 1) % 9;
    else if (act == ACT_PREV) homeSelection = (homeSelection - 1 < 0) ? 8 : (homeSelection - 1);

    // SELECT opens apps
    else if (act == ACT_SELECT && homeSelection == 0)
    {
      currentPage = PAGE_QURAN13;
      q13sm.enter();
      delay(60);
      return;
    }
    else if (act == ACT_SELECT && homeSelection == 3)
    {
      currentPage = PAGE_MUSIC;
      musicSelection = 0;
      drawMusic();
      delay(60);
      return;
    }

    // BACK can be ignored on home (or later show a popup)
    // if (act == ACT_BACK) { }

    if (oldSel != homeSelection)
    {
      display.setPartialWindow(0, 0, display.width(), display.height());
      display.firstPage();
      do { drawHomePage(display, homeSelection); } while (display.nextPage());
    }
  }

  // ---------------- MUSIC ----------------
  else if (currentPage == PAGE_MUSIC)
  {
    int oldSel = musicSelection;

    if (act == ACT_NEXT) {
      musicSelection++;
      if (songList.size() > 0 && musicSelection >= (int)songList.size()) musicSelection = 0;
    }
    else if (act == ACT_PREV) {
      musicSelection--;
      if (songList.size() > 0 && musicSelection < 0) musicSelection = (int)songList.size() - 1;
    }
    else if (act == ACT_BACK) {
      currentPage = PAGE_HOME;
      drawHome();
      delay(60);
      return;
    }

    if (oldSel != musicSelection)
    {
      display.setPartialWindow(0, 0, display.width(), display.height());
      display.firstPage();
      do { drawMusicPage(display, songList, musicSelection); } while (display.nextPage());
    }
  }

  // ---------------- QURAN 13-LINE ----------------
  else if (currentPage == PAGE_QURAN13)
  {
    if (act == ACT_NEXT) q13sm.onNext();
    else if (act == ACT_PREV) q13sm.onPrev();
    else if (act == ACT_SELECT) q13sm.onSelect();
    else if (act == ACT_BACK) q13sm.onBack();

    if (q13sm.shouldExitToHome())
    {
      q13sm.clearExitToHome();
      currentPage = PAGE_HOME;
      drawHome();
      delay(60);
      return;
    }
  }

  delay(30);
}
