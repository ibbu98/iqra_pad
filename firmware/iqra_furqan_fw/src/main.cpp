#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "ui.h"
#include "buttons.h"
#include "sd_card.h" // Include SD logic

// --- PINS ---
#define PIN_CS 10
#define PIN_DC 9
#define PIN_RST 5
#define PIN_BUSY 4 
#define PIN_SCK 12
#define PIN_MOSI 11
#define PIN_MISO 13 

MyDisplay display(GxEPD2_420_GDEY042T81(PIN_CS, PIN_DC, PIN_RST, PIN_BUSY));

// --- STATE MANAGEMENT ---
enum Page { PAGE_HOME, PAGE_MUSIC };
Page currentPage = PAGE_HOME;

// Selection Variables
int homeSelection = 0;
int musicSelection = 0;
std::vector<String> songList; // Stores the loaded files

void setup() {
  Serial.begin(115200);
  delay(2000);
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
  setupButtons();

  // Initialize Display
  display.init(115200, true, 2, false); 
  display.setRotation(0);
  
  // Initialize SD Card
  if (setupSD()) {
      songList = getMusicFiles(); // Load files at startup
  }

  // Draw Initial Home Page
  display.setFullWindow();
  display.firstPage();
  do {
    drawHomePage(display, homeSelection);
  } while (display.nextPage());
}

void loop() {
  int btn = readButtons();
  
  if (btn != 0) {
      // ==========================================
      // LOGIC FOR HOME PAGE
      // ==========================================
      if (currentPage == PAGE_HOME) {
          int oldSel = homeSelection;
          
          // Navigation (Simplified for brevity)
          if (btn == BTN_DOWN) homeSelection = (homeSelection + 1) % 9;
          else if (btn == BTN_UP) homeSelection = (homeSelection - 1 < 0) ? 8 : homeSelection - 1;
          else if (btn == BTN_RIGHT) homeSelection = (homeSelection + 1) % 9; // Simple cycle for now
          
          // ENTERING THE MP3 PLAYER (Item 3)
          else if (btn == BTN_SELECT && homeSelection == 3) {
              currentPage = PAGE_MUSIC;
              musicSelection = 0;
              
              // Draw Music Page
              display.setFullWindow();
              display.firstPage();
              do {
                drawMusicPage(display, songList, musicSelection);
              } while (display.nextPage());
              return; // Exit loop to avoid double update
          }

          // Update Home Screen if changed
          if (oldSel != homeSelection) {
              display.setPartialWindow(0, 0, display.width(), display.height());
              display.firstPage();
              do { drawHomePage(display, homeSelection); } while (display.nextPage());
          }
      } 
      
      // ==========================================
      // LOGIC FOR MUSIC PAGE
      // ==========================================
      else if (currentPage == PAGE_MUSIC) {
          int oldSel = musicSelection;
          
          if (btn == BTN_DOWN) {
              musicSelection++;
              if (musicSelection >= songList.size()) musicSelection = 0;
          }
          else if (btn == BTN_UP) {
              musicSelection--;
              if (musicSelection < 0) musicSelection = songList.size() - 1;
          }
          else if (btn == BTN_LEFT) {
              // GO BACK TO HOME
              currentPage = PAGE_HOME;
              display.setFullWindow();
              display.firstPage();
              do { drawHomePage(display, homeSelection); } while (display.nextPage());
              return;
          }

          if (oldSel != musicSelection) {
              display.setPartialWindow(0, 0, display.width(), display.height());
              display.firstPage();
              do { drawMusicPage(display, songList, musicSelection); } while (display.nextPage());
          }
      }
      
      delay(200); // Debounce
  }
}