#include "ui.h"

// --- Helper: Draw Tile + Selection Cursor ---
void drawTile(MyDisplay &display, int x, int y, int w, int h, const char* text, const GFXfont* font, bool isSelected) {
  // 1. Draw Normal Box
  display.drawRect(x, y, w, h, GxEPD_BLACK);
  
  // 2. IF SELECTED: Draw a Thick Border
  if (isSelected) {
      display.drawRect(x+1, y+1, w-2, h-2, GxEPD_BLACK);
      display.drawRect(x+2, y+2, w-4, h-4, GxEPD_BLACK);
      display.drawRect(x+3, y+3, w-6, h-6, GxEPD_BLACK);
  }
  
  // 3. Set Font & Center Text
  display.setFont(font);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  int textX = x + (w - tbw) / 2 - tbx;
  int textY = y + (h - tbh) / 2 - tby;
  
  display.setCursor(textX, textY);
  display.print(text);
}

// --- Helper: Vertical Text ---
void drawVerticalText(MyDisplay &display, int x, int y, int h, const char* text) {
    display.setFont(&FreeSansBold9pt7b); 
    int startY = y + 25;
    for (int i = 0; text[i] != '\0'; i++) {
        char buf[2] = {text[i], '\0'};
        int16_t tbx, tby; uint16_t tbw, tbh;
        display.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
        int textX = x + (25 - tbw) / 2 - tbx;
        display.setCursor(textX, startY);
        display.print(buf);
        startY += 18; 
    }
}

void drawHomePage(MyDisplay &display, int selectedItem) {
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  // --- HEADER ---
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(5, 25); display.print("1 Feb / 13 Rajab"); 

  display.setFont(&FreeSansBold12pt7b); 
  display.setCursor(175, 28); display.print("12:45");

  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(320, 25); display.print("85%");
  display.drawRect(375, 12, 20, 12, GxEPD_BLACK);
  display.fillRect(377, 14, 12, 8, GxEPD_BLACK); 
  display.fillRect(395, 15, 2, 6, GxEPD_BLACK); 
  display.drawLine(0, 40, 400, 40, GxEPD_BLACK);

  // --- BUTTON GRID ---
  // Using the ComicNeue font we set up earlier
  const GFXfont* btnFont = &FreeMonoBold12pt7b;

  // I kept YOUR strings exactly as you wrote them (with spaces)
  drawTile(display, 0, 40, 130, 130, "13 Line\n  Quran", btnFont, (selectedItem == 0));
  drawTile(display, 0, 170, 130, 130, "15 Line\n  Quran", btnFont, (selectedItem == 1));

  drawTile(display, 130, 40, 130, 86, "Adhkar", btnFont, (selectedItem == 2));
  drawTile(display, 130, 126, 130, 87, "     Quran\n            MP3", btnFont, (selectedItem == 3));
  drawTile(display, 130, 213, 130, 87, "     Voice\n            Memo", btnFont, (selectedItem == 4));
  
  drawTile(display, 260, 40, 115, 86, "Alarm", btnFont, (selectedItem == 5));
  drawTile(display, 260, 126, 115, 87, "Timer", btnFont, (selectedItem == 6));
  drawTile(display, 260, 213, 115, 87, "Tasbeeh", btnFont, (selectedItem == 7));

  // --- SETTINGS STRIP (Item 8) ---
  int sx = 375, sy = 40, sw = 25, sh = 260;
  display.drawRect(sx, sy, sw, sh, GxEPD_BLACK);

  // The logic to make it thick if selected
  if (selectedItem == 8) {
      display.drawRect(sx+1, sy+1, sw-2, sh-2, GxEPD_BLACK);
      display.drawRect(sx+2, sy+2, sw-4, sh-4, GxEPD_BLACK);
      display.drawRect(sx+3, sy+3, sw-6, sh-6, GxEPD_BLACK);
  }

  drawVerticalText(display, sx, sy + 30, sh, "SETTINGS");
}

// ... (Keep all your existing helper functions and drawHomePage) ...

// --- NEW: DRAW MUSIC LIST PAGE ---
void drawMusicPage(MyDisplay &display, std::vector<String> &files, int selectedIndex) {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    
    // 1. Header
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(10, 30);
    display.print("Select Recitation:");
    display.drawLine(0, 45, 400, 45, GxEPD_BLACK);

    // 2. Draw the Files
    display.setFont(&FreeSansBold9pt7b);
    int startY = 75;
    
    // If no files found
    if (files.size() == 0) {
        display.setCursor(20, 100);
        display.print("No MP3 Files Found!");
        return;
    }

    // Loop through files (Show max 8 to fit screen)
    for (int i = 0; i < files.size(); i++) {
        // Stop if we run out of screen space
        if (startY > 280) break;

        int cursorY = startY;
        
        // Highlight the selected file
        if (i == selectedIndex) {
            // Draw a black box behind the text (Inverted look)
            display.fillRect(0, cursorY - 15, 300, 25, GxEPD_BLACK);
            display.setTextColor(GxEPD_WHITE); // Text becomes White
        } else {
            display.setTextColor(GxEPD_BLACK); // Normal Text
        }

        display.setCursor(20, cursorY);
        display.print(files[i]);
        
        startY += 30; // Move down for next line
    }
}