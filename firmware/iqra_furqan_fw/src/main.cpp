#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <GxEPD2_BW.h>

#include "ui.h"
#include "buttons.h"
#include "sd_card.h"

// Quran pages header (contains QURAN_W, QURAN_H, QURAN13_PAGES, QURAN13_PAGE_COUNT)
#include "13_line_quran_pages.h"

// --- PINS ---
#define PIN_CS   10
#define PIN_DC   9
#define PIN_RST  5
#define PIN_BUSY 4
#define PIN_SCK  12
#define PIN_MOSI 11
#define PIN_MISO 13

MyDisplay display(GxEPD2_420_GDEY042T81(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

// --- STATE MANAGEMENT ---
enum Page {
  PAGE_HOME,
  PAGE_MUSIC,
  PAGE_QURAN13
};

Page currentPage = PAGE_HOME;

// Selection Variables
int homeSelection = 0;
int musicSelection = 0;
std::vector<String> songList;

// Quran page index (0..QURAN13_PAGE_COUNT-1)
int quran13Index = 0;

// =======================
// Helpers to draw screens
// =======================
void drawHome()
{
  display.setFullWindow();
  display.firstPage();
  do {
    drawHomePage(display, homeSelection);
  } while (display.nextPage());
}

void drawMusic()
{
  display.setFullWindow();
  display.firstPage();
  do {
    drawMusicPage(display, songList, musicSelection);
  } while (display.nextPage());
}

void drawQuran13(int idx)
{
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Use table from 13_line_quran_pages.cpp
    display.drawBitmap(0, 0, QURAN13_PAGES[idx], QURAN_W, QURAN_H, GxEPD_BLACK);

  } while (display.nextPage());
}

void setup()
{
  Serial.begin(115200);
  delay(2000);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  setupButtons();

  // Initialize Display
  display.init(115200, true, 2, false);
  display.setRotation(0);

  // Initialize SD Card (optional)
  if (setupSD()) {
    songList = getMusicFiles();
  }

  // Draw Initial Home Page
  drawHome();
}

void loop()
{
  int btn = readButtons();
  if (btn == 0) return;

  // ==========================================
  // HOME PAGE LOGIC
  // ==========================================
  if (currentPage == PAGE_HOME)
  {
    int oldSel = homeSelection;

    if (btn == BTN_DOWN) homeSelection = (homeSelection + 1) % 9;
    else if (btn == BTN_UP) homeSelection = (homeSelection - 1 < 0) ? 8 : (homeSelection - 1);
    else if (btn == BTN_RIGHT) homeSelection = (homeSelection + 1) % 9;

    // ENTER QURAN 13-LINE MODE
    // NOTE: change 0 to the correct index if "13 line Quran" is not the first item.
    else if (btn == BTN_SELECT && homeSelection == 0)
    {
      currentPage = PAGE_QURAN13;
      quran13Index = 0;
      drawQuran13(quran13Index);
      delay(120);
      return;
    }

    // ENTER MP3 PLAYER (your existing item 3)
    else if (btn == BTN_SELECT && homeSelection == 3)
    {
      currentPage = PAGE_MUSIC;
      musicSelection = 0;
      drawMusic();
      delay(120);
      return;
    }

    // Update Home Screen if changed
    if (oldSel != homeSelection)
    {
      display.setPartialWindow(0, 0, display.width(), display.height());
      display.firstPage();
      do { drawHomePage(display, homeSelection); } while (display.nextPage());
    }
  }

  // ==========================================
  // MUSIC PAGE LOGIC
  // ==========================================
  else if (currentPage == PAGE_MUSIC)
  {
    int oldSel = musicSelection;

    if (btn == BTN_DOWN) {
      musicSelection++;
      if (songList.size() > 0 && musicSelection >= (int)songList.size()) musicSelection = 0;
    }
    else if (btn == BTN_UP) {
      musicSelection--;
      if (songList.size() > 0 && musicSelection < 0) musicSelection = (int)songList.size() - 1;
    }
    else if (btn == BTN_LEFT) {
      currentPage = PAGE_HOME;
      drawHome();
      delay(120);
      return;
    }

    if (oldSel != musicSelection)
    {
      display.setPartialWindow(0, 0, display.width(), display.height());
      display.firstPage();
      do { drawMusicPage(display, songList, musicSelection); } while (display.nextPage());
    }
  }

  // ==========================================
  // QURAN 13-LINE PAGE LOGIC (RTL)
  // RIGHT = PREVIOUS, LEFT = NEXT
  // Wrap around after 5th page -> back to 1st
  // ==========================================
  else if (currentPage == PAGE_QURAN13)
  {
    // RIGHT = Previous page
    if (btn == BTN_RIGHT)
    {
      quran13Index--;
      if (quran13Index < 0) quran13Index = (int)QURAN13_PAGE_COUNT - 1;
      drawQuran13(quran13Index);
      delay(120);
      return;
    }

    // LEFT = Next page
    if (btn == BTN_LEFT)
    {
      quran13Index = (quran13Index + 1) % QURAN13_PAGE_COUNT;
      drawQuran13(quran13Index);
      delay(120);
      return;
    }

    // BACK to Home
    if (btn == BTN_SELECT)
    {
      currentPage = PAGE_HOME;
      drawHome();
      delay(120);
      return;
    }
  }

  delay(120);
}
