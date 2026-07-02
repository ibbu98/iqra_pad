#include "ui.h"
#include "settings.h"
#include "quran_data.h"
#include "quran_sd.h"
#include <algorithm>  // std::find
#include <math.h>     // cosf / sinf for WiFi arc icon

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

// WiFi fan icon — 3 arcs pointing upward, 2 px thick each for crispness on e-ink.
// Angles 210-330 deg sweep the upper semicircle (screen coords: y increases down).
// Compact radii (4,7,10) keep it the same height as the battery icon.
static void drawWifiIcon(MyDisplay &d, int cx, int cy)
{
    d.fillCircle(cx, cy, 2, GxEPD_BLACK);           // dot
    const int radii[3] = {4, 7, 10};
    for (int i = 0; i < 3; i++) {
        for (int t = 0; t <= 1; t++) {              // draw r and r+1 for thickness
            int r = radii[i] + t;
            for (int deg = 215; deg <= 325; deg += 2) {
                float ang = deg * 3.14159265f / 180.0f;
                int px = cx + (int)(r * cosf(ang) + 0.5f);
                int py = cy + (int)(r * sinf(ang) + 0.5f);
                d.drawPixel(px, py, GxEPD_BLACK);
            }
        }
    }
}

void drawHomePage(MyDisplay &display, int selectedItem, bool wifiOn) {
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

  // WiFi symbol — centred between time and battery, aligned with battery icon height
  if (wifiOn) drawWifiIcon(display, 275, 20);

  display.drawLine(0, 40, 400, 40, GxEPD_BLACK);

  // --- BUTTON GRID ---
  // Using the ComicNeue font we set up earlier
  const GFXfont* btnFont = &FreeMonoBold12pt7b;

  // I kept YOUR strings exactly as you wrote them (with spaces)
  drawTile(display, 0, 40, 130, 130, "13 Line\n  Quran", btnFont, (selectedItem == 0));
  drawTile(display, 0, 170, 130, 130, "15 Line\n  Quran", btnFont, (selectedItem == 1));

  drawTile(display, 130, 40, 130, 86, "Adhkar", btnFont, (selectedItem == 2));
  // "Quran MP3 & Bayaan" — two lines drawn manually so they stay inside the tile
  {
    int tx = 130, ty = 126, tw = 130, th = 87;
    display.drawRect(tx, ty, tw, th, GxEPD_BLACK);
    if (selectedItem == 3) {
      display.drawRect(tx+1, ty+1, tw-2, th-2, GxEPD_BLACK);
      display.drawRect(tx+2, ty+2, tw-4, th-4, GxEPD_BLACK);
      display.drawRect(tx+3, ty+3, tw-6, th-6, GxEPD_BLACK);
    }
    display.setFont(&FreeSansBold9pt7b);
    int16_t x1, y1; uint16_t w1, h1;
    display.getTextBounds("Quran MP3", 0, 0, &x1, &y1, &w1, &h1);
    display.setCursor(tx + (tw - (int)w1)/2 - x1, ty + th/2 - 2);
    display.print("Quran MP3");
    display.getTextBounds("& Bayaan", 0, 0, &x1, &y1, &w1, &h1);
    display.setCursor(tx + (tw - (int)w1)/2 - x1, ty + th/2 + 16);
    display.print("& Bayaan");
  }
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

// --- RECITER SELECTION PAGE ---
void drawReciterPage(MyDisplay &display, std::vector<String> &folders, int selectedIndex)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(10, PG_HEADER_Y);
  display.print("Quran MP3");
  display.setCursor(290, PG_HEADER_Y);
  display.print("BACK = exit");
  display.drawLine(0, PG_LINE_Y, 400, PG_LINE_Y, GxEPD_BLACK);

  if (folders.empty()) {
    display.setCursor(20, 130);
    display.print("No folders found on SD card");
    return;
  }

  for (int i = 0; i < (int)folders.size(); i++) {
    int y = PG_LIST_TOP + i * PG_ITEM_H;
    if (y + PG_ITEM_H > 300) break;

    if (i == selectedIndex) {
      display.fillRect(0, y, 400, PG_ITEM_H, GxEPD_BLACK);
      display.setTextColor(GxEPD_WHITE);
    } else {
      display.setTextColor(GxEPD_BLACK);
      display.drawLine(0, y + PG_ITEM_H, 400, y + PG_ITEM_H, GxEPD_BLACK);
    }

    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(8, y + PG_ITEM_H - 7);
    display.print(folders[i]);
    display.setCursor(386, y + PG_ITEM_H - 7);
    display.print(">");

    display.setTextColor(GxEPD_BLACK);
  }
}

// --- SURAH LIST PAGE (scrollable, 9 visible, scrollbar on right) ---
void drawSurahPage(MyDisplay &display, const String &reciterName,
                   std::vector<String> &surahs, int selectedIndex, int topIndex)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  int total = (int)surahs.size();
  int listW = 400 - PG_SCROLL_W;

  // Header (all 9pt now)
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(10, PG_HEADER_Y);
  display.print(reciterName);
  display.setCursor(300, PG_HEADER_Y);
  display.printf("%d / %d", selectedIndex + 1, total);
  display.drawLine(0, PG_LINE_Y, listW, PG_LINE_Y, GxEPD_BLACK);

  // List rows
  for (int i = 0; i < PG_ITEMS; i++) {
    int idx = topIndex + i;
    if (idx >= total) break;

    int y = PG_LIST_TOP + i * PG_ITEM_H;

    if (idx == selectedIndex) {
      display.fillRect(0, y, listW, PG_ITEM_H, GxEPD_BLACK);
      display.setTextColor(GxEPD_WHITE);
    } else {
      display.setTextColor(GxEPD_BLACK);
      display.drawLine(0, y + PG_ITEM_H, listW, y + PG_ITEM_H, GxEPD_BLACK);
    }

    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(8, y + PG_ITEM_H - 7);
    display.printf("%3d.  %s", idx + 1, surahs[idx].c_str());

    display.setTextColor(GxEPD_BLACK);
  }

  // Scrollbar
  int sbX = 400 - PG_SCROLL_W;
  int sbH = PG_ITEMS * PG_ITEM_H;
  display.drawRect(sbX, PG_LIST_TOP, PG_SCROLL_W, sbH, GxEPD_BLACK);
  if (total > PG_ITEMS) {
    int thumbH = max(10, sbH * PG_ITEMS / total);
    int maxTop = total - PG_ITEMS;
    int thumbY = PG_LIST_TOP + (sbH - thumbH) * topIndex / maxTop;
    display.fillRect(sbX + 1, thumbY + 1, PG_SCROLL_W - 2, thumbH - 2, GxEPD_BLACK);
  }
}

// --- VOLUME BAR (standalone so it can also be called in partial-refresh mode) ---
void drawVolRegion(MyDisplay &display, float vol)
{
  display.fillRect(0, VOL_REGION_Y, 400, VOL_REGION_H, GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold9pt7b);

  int pct = (int)(vol * 100.0f + 0.5f);
  char buf[20];
  snprintf(buf, sizeof(buf), "Volume  %d%%", pct);
  display.setCursor(10, VOL_REGION_Y + 18);
  display.print(buf);

  // bar outline + fill
  int bx = 10, by = VOL_REGION_Y + 26, bw = 380, bh = 44;
  display.drawRect(bx, by, bw, bh, GxEPD_BLACK);
  int fill = (int)(bw * vol + 0.5f);
  if (fill > 0)
    display.fillRect(bx, by, fill, bh, GxEPD_BLACK);
}

// --- NOW PLAYING PAGE ---
void drawNowPlayingPage(MyDisplay &display, const String &reciter,
                        const String &surahName, int surahNum, int totalSurahs,
                        float vol)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  // ── Top bar (compact 28px) ────────────────────────────────────────────────
  display.fillRect(0, 0, 400, 28, GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(10, 20);
  display.print("Now Playing");
  display.setCursor(238, 20);
  display.print("BACK = stop");
  display.setTextColor(GxEPD_BLACK);

  // ── Surah number / total ─────────────────────────────────────────────────
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(10, 46);
  display.printf("%d  /  %d", surahNum, totalSurahs);

  // ── Surah name ───────────────────────────────────────────────────────────
  display.setCursor(10, 62);
  display.print(surahName);
  display.drawLine(0, 70, 400, 70, GxEPD_BLACK);

  // ── Reciter ──────────────────────────────────────────────────────────────
  display.setCursor(10, 86);
  display.print(reciter);
  display.drawLine(0, 94, 400, 94, GxEPD_BLACK);

  // ── Volume bar ───────────────────────────────────────────────────────────
  drawVolRegion(display, vol);

  // ── Bottom hint ──────────────────────────────────────────────────────────
  display.drawLine(0, 252, 400, 252, GxEPD_BLACK);
  display.setCursor(10, 270);
  display.print("PREV = prev surah      NEXT = next surah");
}

// ═══════════════════════════════════════════════════════════════════════════════
// QURAN VIEWER PAGES
// ═══════════════════════════════════════════════════════════════════════════════

// ── 2×2 tile menu: Surah Index / Juz Index / Bookmarks / Last Read ────────────
void drawQuranMenuPage(MyDisplay &display, int quranType, int selItem)
{
  display.fillScreen(GxEPD_WHITE);

  // Header bar
  display.fillRect(0, 0, 400, 28, GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(10, 20);
  display.print(quranType == 0 ? "13-Line Quran" : "15-Line Quran");
  display.setCursor(270, 20);
  display.print("BACK = home");
  display.setTextColor(GxEPD_BLACK);

  // Tile geometry: 2 columns × 2 rows below the 28px header
  static const int TW = 200, TH = 136;
  static const int TX[4] = { 0, 200,   0, 200 };
  static const int TY[4] = { 28,  28, 164, 164 };
  static const char* LINE1[4] = { "Surah",  "Juz",   "Book-", "Last" };
  static const char* LINE2[4] = { "Index",  "Index", "marks", "Read" };

  for (int i = 0; i < 4; i++) {
    int x = TX[i], y = TY[i];

    // Tile border — thick (3 px) when selected
    display.drawRect(x, y, TW, TH, GxEPD_BLACK);
    if (i == selItem) {
      display.drawRect(x+1, y+1, TW-2, TH-2, GxEPD_BLACK);
      display.drawRect(x+2, y+2, TW-4, TH-4, GxEPD_BLACK);
      display.drawRect(x+3, y+3, TW-6, TH-6, GxEPD_BLACK);
    }

    // Two-line centred label
    display.setFont(&FreeMonoBold12pt7b);
    int16_t tbx, tby; uint16_t tbw, tbh;

    display.getTextBounds(LINE1[i], 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(x + (TW - (int)tbw) / 2 - tbx, y + TH/2 - 8);
    display.print(LINE1[i]);

    display.getTextBounds(LINE2[i], 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(x + (TW - (int)tbw) / 2 - tbx, y + TH/2 + 20);
    display.print(LINE2[i]);
  }
}

// ── Shared header + scrollbar helper used by index pages ─────────────────────
static void _drawIndexHeader(MyDisplay &d, const char* title)
{
  d.setFont(&FreeSansBold9pt7b);
  d.setCursor(10, PG_HEADER_Y);
  d.print(title);
  d.setCursor(278, PG_HEADER_Y);
  d.print("BACK = menu");
  d.drawLine(0, PG_LINE_Y, 390, PG_LINE_Y, GxEPD_BLACK);
}

static void _drawScrollbar(MyDisplay &d, int total, int topIdx)
{
  int sbX = 390;
  int sbH = PG_ITEMS * PG_ITEM_H;
  d.drawRect(sbX, PG_LIST_TOP, 10, sbH, GxEPD_BLACK);
  if (total > PG_ITEMS) {
    int thumbH = max(10, sbH * PG_ITEMS / total);
    int maxTop = total - PG_ITEMS;
    int thumbY = PG_LIST_TOP + (sbH - thumbH) * topIdx / maxTop;
    d.fillRect(sbX + 1, thumbY + 1, 8, thumbH - 2, GxEPD_BLACK);
  }
}

// ── Surah Index (scrollable 114 rows) ────────────────────────────────────────
void drawSurahIndexPage(MyDisplay &display, int quranType, int selIdx, int topIdx)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  _drawIndexHeader(display, "Surah Index");

  for (int i = 0; i < PG_ITEMS; i++) {
    int idx = topIdx + i;
    if (idx >= 114) break;

    int y = PG_LIST_TOP + i * PG_ITEM_H;

    if (idx == selIdx) {
      display.fillRect(0, y, 390, PG_ITEM_H, GxEPD_BLACK);
      display.setTextColor(GxEPD_WHITE);
    } else {
      display.setTextColor(GxEPD_BLACK);
      display.drawLine(0, y + PG_ITEM_H, 390, y + PG_ITEM_H, GxEPD_BLACK);
    }

    display.setFont(&FreeSansBold9pt7b);
    int rowY = y + PG_ITEM_H - 7;

    // Surah number + name left-aligned
    display.setCursor(6, rowY);
    display.printf("%3d  %s", SURAH_TABLE[idx].num, SURAH_TABLE[idx].name);

    // Page number right-aligned at x=382
    uint16_t page = (quranType == 0) ? SURAH_TABLE[idx].page13
                                     : SURAH_TABLE[idx].page15;
    char pgStr[6];
    if (page == 0) strncpy(pgStr, "---", sizeof(pgStr));
    else           snprintf(pgStr, sizeof(pgStr), "%d", page);

    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(pgStr, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(382 - (int)tbw - tbx, rowY);
    display.print(pgStr);

    display.setTextColor(GxEPD_BLACK);
  }

  _drawScrollbar(display, 114, topIdx);
}

// ── Juz Index (scrollable 30 rows) ───────────────────────────────────────────
void drawJuzIndexPage(MyDisplay &display, int quranType, int selIdx, int topIdx)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  _drawIndexHeader(display, "Juz Index");

  for (int i = 0; i < PG_ITEMS; i++) {
    int idx = topIdx + i;
    if (idx >= 30) break;

    int y = PG_LIST_TOP + i * PG_ITEM_H;

    if (idx == selIdx) {
      display.fillRect(0, y, 390, PG_ITEM_H, GxEPD_BLACK);
      display.setTextColor(GxEPD_WHITE);
    } else {
      display.setTextColor(GxEPD_BLACK);
      display.drawLine(0, y + PG_ITEM_H, 390, y + PG_ITEM_H, GxEPD_BLACK);
    }

    display.setFont(&FreeSansBold9pt7b);
    int rowY = y + PG_ITEM_H - 7;

    display.setCursor(6, rowY);
    display.printf("Juz  %2d", idx + 1);

    uint16_t page = (quranType == 0) ? JUZ_PAGE13[idx] : JUZ_PAGE15[idx];
    char pgStr[6];
    if (page == 0) strncpy(pgStr, "---", sizeof(pgStr));
    else           snprintf(pgStr, sizeof(pgStr), "%d", page);

    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(pgStr, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(382 - (int)tbw - tbx, rowY);
    display.print(pgStr);

    display.setTextColor(GxEPD_BLACK);
  }

  _drawScrollbar(display, 30, topIdx);
}

// ── Bookmarks page ───────────────────────────────────────────────────────────
void drawBookmarksPage(MyDisplay &display, const std::vector<int> &bookmarks,
                       int selIdx, int topIdx)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  _drawIndexHeader(display, "Bookmarks");

  if (bookmarks.empty()) {
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(20, 130);
    display.print("No bookmarks yet.");
    display.setCursor(20, 155);
    display.print("Press SELECT on a page to add one.");
    return;
  }

  int total = (int)bookmarks.size();
  for (int i = 0; i < PG_ITEMS; i++) {
    int idx = topIdx + i;
    if (idx >= total) break;

    int y = PG_LIST_TOP + i * PG_ITEM_H;

    if (idx == selIdx) {
      display.fillRect(0, y, 390, PG_ITEM_H, GxEPD_BLACK);
      display.setTextColor(GxEPD_WHITE);
    } else {
      display.setTextColor(GxEPD_BLACK);
      display.drawLine(0, y + PG_ITEM_H, 390, y + PG_ITEM_H, GxEPD_BLACK);
    }

    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(6, y + PG_ITEM_H - 7);
    display.printf("Page  %d", bookmarks[idx]);

    display.setTextColor(GxEPD_BLACK);
  }

  _drawScrollbar(display, total, topIdx);
}

// ── Quran page viewer ──────────────────────────────────────────────────────────
void drawQuranViewPage(MyDisplay &display, int quranType, int pageNum, bool isBookmarked)
{
  // ── Step 1: fill the full 400×300 display with the page bitmap ───────────
  // Bitmap convention (from the conversion tool):
  //   bit = 1  →  white paper background
  //   bit = 0  →  black ink text
  // The 6-arg drawBitmap(... fg, bg) covers every pixel, so no fillScreen needed.
  const char* folder = (quranType == 0) ? "13line" : "15line";
  const uint8_t* bmp = quranLoadPage(folder, pageNum);

  if (bmp) {
    display.drawBitmap(0, 0, bmp, QURAN_PX_W, QURAN_PX_H,
                       GxEPD_WHITE,   // bit=1 → white (paper)
                       GxEPD_BLACK);  // bit=0 → black (ink)
  } else {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(30, 150);
    display.printf("Page %d not on SD card", pageNum);
    display.setCursor(30, 172);
    display.printf("Folder: SD:/%s/", folder);
  }

  // No status overlay — full display is the Quran page image.
}

// ── Power splash (boot / shutdown) ────────────────────────────────────────────
void drawSplashPage(MyDisplay &display, const char* fwVersion)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  // Outer border
  display.drawRect(6, 6, 388, 288, GxEPD_BLACK);
  display.drawRect(9, 9, 382, 282, GxEPD_BLACK);

  // "IQRA PAD" — large, centred
  display.setFont(&FreeSansBold18pt7b);
  const char* line1 = "IQRA PAD";
  int16_t x1, y1; uint16_t w1, h1;
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
  display.setCursor((400 - w1) / 2 - x1, 138);
  display.print(line1);

  // Divider line under title
  display.drawLine(80, 150, 320, 150, GxEPD_BLACK);

  // "FURQAN VER x.x.x" — version from settings.h, centred
  display.setFont(&FreeSansBold9pt7b);
  char line2[32];
  if (fwVersion && fwVersion[0] != '\0')
    snprintf(line2, sizeof(line2), "FURQAN VER %s", fwVersion);
  else
    strncpy(line2, "FURQAN VER 1.0", sizeof(line2));
  int16_t x2, y2; uint16_t w2, h2;
  display.getTextBounds(line2, 0, 0, &x2, &y2, &w2, &h2);
  display.setCursor((400 - w2) / 2 - x2, 172);
  display.print(line2);
}

// ── Bluetooth info page (hardware not installed — future feature) ─────────────
void drawBtInfoPage(MyDisplay &display)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);

  // Header
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(8, PG_HEADER_Y);
  display.print("Bluetooth");
  display.drawLine(0, PG_LINE_Y, 400, PG_LINE_Y, GxEPD_BLACK);

  display.setFont(&FreeSansBold9pt7b);
  int y = PG_LIST_TOP + 14;

  display.setCursor(12, y);  display.print("Status:");
  display.setCursor(110, y); display.print("Not installed");

  y += PG_ITEM_H;
  display.setCursor(12, y);  display.print("Module:");
  display.setCursor(110, y); display.print("KCX_BT_EMITTER");

  y += PG_ITEM_H + 4;
  display.drawLine(12, y, 388, y, GxEPD_BLACK);
  y += 14;
  display.setCursor(12, y);
  display.print("Add a KCX_BT_EMITTER module to");
  y += PG_ITEM_H - 4;
  display.setCursor(12, y);
  display.print("connect Bluetooth earphones.");
  y += PG_ITEM_H - 4;
  display.setCursor(12, y);
  display.print("Audio will stream automatically");
  y += PG_ITEM_H - 4;
  display.setCursor(12, y);
  display.print("once the module is wired up.");

  display.setCursor(8, 290);
  display.print("BACK = return");
}

// ── About Device page ─────────────────────────────────────────────────────────
void drawAboutDevicePage(MyDisplay &display, const String& wifiIp, const char* sdSize)
{
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeSansBold12pt7b);
  display.setCursor(8, PG_HEADER_Y);
  display.print("About");
  display.drawLine(0, PG_LINE_Y, 400, PG_LINE_Y, GxEPD_BLACK);

  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  const int LX = 12;
  const int VX = 132;   // value column — wide enough for "Device Name"
  const int LH = 26;    // row height
  int y = PG_LIST_TOP + 14;   // first baseline = 46

  auto row = [&](const char* label, const char* value) {
    display.setCursor(LX, y); display.print(label);
    display.setCursor(VX, y); display.print(value);
    display.drawLine(0, y + 8, 400, y + 8, GxEPD_BLACK);
    y += LH;
  };

  row("User Name",    DEVICE_USER);
  row("Device Name",  DEVICE_NAME);
  row("Model Name",   DEVICE_MODEL);

  char fwBuf[12];
  snprintf(fwBuf, sizeof(fwBuf), "v%s", FW_VERSION);
  row("Firmware",     fwBuf);

  row("Display",      "4.2\" E-Ink 400x300");
  row("Chip",         "ESP32-S3 N16R8");
  row("SD Card",      sdSize);
  row("WiFi IP",      wifiIp.isEmpty() ? "Not connected" : wifiIp.c_str());

  // Bluetooth — last row, no divider
  display.setCursor(LX, y); display.print("Bluetooth");
  display.setCursor(VX, y); display.print("Coming Soon");

  display.setCursor(8, 293);
  display.print("BACK = return");
}

// ── Shared settings header ────────────────────────────────────────────────────
static void _drawSettingsHeader(MyDisplay &d, const char* title)
{
  d.fillScreen(GxEPD_WHITE);
  d.setTextColor(GxEPD_BLACK);
  d.setFont(&FreeSansBold12pt7b);
  d.setCursor(8, PG_HEADER_Y);
  d.print(title);
  d.drawLine(0, PG_LINE_Y, 400, PG_LINE_Y, GxEPD_BLACK);
}

// ── Settings main menu ────────────────────────────────────────────────────────
void drawSettingsPage(MyDisplay &display, int selItem)
{
  _drawSettingsHeader(display, "Settings");

  const char* items[] = { "WiFi", "Bluetooth", "Software Update", "About" };
  for (int i = 0; i < 4; i++) {
    int y = PG_LIST_TOP + i * PG_ITEM_H;
    if (i == selItem) {
      display.fillRect(0, y, 400, PG_ITEM_H, GxEPD_BLACK);
      display.setTextColor(GxEPD_WHITE);
    } else {
      display.setTextColor(GxEPD_BLACK);
      display.drawLine(0, y + PG_ITEM_H, 400, y + PG_ITEM_H, GxEPD_BLACK);
    }
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(12, y + PG_ITEM_H - 7);
    display.print(items[i]);
    display.setTextColor(GxEPD_BLACK);
  }

  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.setCursor(8, 290);
  display.print("BACK = return");
}

// ── WiFi status page ──────────────────────────────────────────────────────────
void drawWifiSettingsPage(MyDisplay &display, bool connected,
                          const String &ssid, const String &ip)
{
  _drawSettingsHeader(display, "WiFi");
  display.setTextColor(GxEPD_BLACK);

  const int LX = 12;   // left margin
  const int VX = 115;  // value column
  const int LH = 24;   // line height
  int y = PG_LIST_TOP + 14;

  // ── Status row ──────────────────────────────────────────────────────────────
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(LX, y); display.print("Status:");
  display.setCursor(VX, y);
  display.print(connected ? "Connected" : "Not connected");

  if (connected) {
    // ── Network + IP ──────────────────────────────────────────────────────────
    y += LH;
    display.setCursor(LX, y); display.print("Network:");
    display.setCursor(VX, y);
    display.print(ssid.isEmpty() ? "-" : ssid.c_str());

    y += LH;
    display.setCursor(LX, y); display.print("IP:");
    display.setCursor(VX, y);
    display.print(ip.isEmpty() ? "-" : ip.c_str());

    // ── Divider ───────────────────────────────────────────────────────────────
    y += LH + 6;
    display.drawLine(LX, y, 388, y, GxEPD_BLACK);
    y += 14;

    // ── Update URL (9pt so it fits on one line) ───────────────────────────────
    display.setCursor(LX, y);
    display.print("Firmware update - open in browser:");

    y += LH;
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(LX, y);
    // Show IP-based URL — always works (mDNS may not on all phones)
    String url = "http://" + ip + "/update";
    display.print(url.c_str());

    y += LH + 2;
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(LX, y);
    display.print("or: iqra-pad.local/update");

    // Footer
    display.setCursor(LX, 290);
    display.print("SELECT=forget WiFi   BACK=return");

  } else {
    // ── Not connected — hotspot instructions ──────────────────────────────────
    y += LH + 4;
    display.setCursor(LX, y);
    display.print("Hotspot active:");

    y += LH - 4;
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(LX, y);
    display.print("IQRA-PAD-SETUP");

    display.setFont(&FreeSansBold9pt7b);
    y += LH + 8;
    display.drawLine(LX, y, 388, y, GxEPD_BLACK);

    y += 14;
    display.setCursor(LX, y);
    display.print("1. Connect phone to IQRA-PAD-SETUP");

    y += LH;
    display.setCursor(LX, y);
    display.print("2. Browser opens - enter your WiFi");

    y += LH;
    display.setCursor(LX, y);
    display.print("   No popup? Open: 192.168.4.1");

    // Footer
    display.setCursor(LX, 290);
    display.print("BACK = return");
  }
}

// ── OTA update page ───────────────────────────────────────────────────────────
// otaState values: 0=idle 1=checking 2=up-to-date 3=available
//                  4=downloading 5=installing 6=done 7=error
void drawOtaPage(MyDisplay &display, const char* currentVer,
                 const char* latestVer, int otaState, int progress,
                 const char* errMsg)
{
  _drawSettingsHeader(display, "Software Update");
  display.setFont(&FreeSansBold9pt7b);
  display.setTextColor(GxEPD_BLACK);

  int y = PG_LIST_TOP + 8;
  display.setCursor(12, y);
  display.print("Installed:  v");
  display.print(currentVer);

  y += PG_ITEM_H;

  switch (otaState) {
    case 0: // OTA_IDLE
      display.setCursor(12, y);
      display.print("SELECT = check for update");
      break;

    case 1: // OTA_CHECKING
      display.setCursor(12, y);
      display.print("Checking for update...");
      break;

    case 2: // OTA_UP_TO_DATE
      display.setCursor(12, y);
      display.print("You are up to date!");
      y += PG_ITEM_H;
      display.setCursor(12, y);
      display.print("v");
      display.print(currentVer);
      display.print(" is the latest version.");
      break;

    case 3: // OTA_AVAILABLE
      display.setCursor(12, y);
      display.print("New version available:");
      y += PG_ITEM_H;
      display.setFont(&FreeSansBold12pt7b);
      display.setCursor(12, y);
      display.print("v");
      display.print(latestVer);
      display.setFont(&FreeSansBold9pt7b);
      y += PG_ITEM_H + 4;
      display.setCursor(12, y);
      display.print("SELECT = install now");
      y += PG_ITEM_H - 4;
      display.setCursor(12, y);
      display.print("BACK = later");
      break;

    case 4: { // OTA_DOWNLOADING
      display.setCursor(12, y);
      display.print("Downloading v");
      display.print(latestVer);
      display.print("...");

      // Progress bar
      int barY = y + 22;
      int barW = 370;
      display.drawRect(12, barY, barW, 18, GxEPD_BLACK);
      int fill = (progress * (barW - 2)) / 100;
      if (fill > 0) display.fillRect(13, barY + 1, fill, 16, GxEPD_BLACK);

      y = barY + 28;
      display.setCursor(12, y);
      display.printf("%d%%  Please wait...", progress);
      break;
    }

    case 6: // OTA_DONE
      display.setCursor(12, y);
      display.print("Update complete!");
      y += PG_ITEM_H;
      display.setCursor(12, y);
      display.print("Device is rebooting...");
      break;

    case 7: // OTA_ERROR
      display.setCursor(12, y);
      display.print("Update failed:");
      y += PG_ITEM_H - 4;
      display.setCursor(12, y);
      display.print(errMsg);
      break;
  }

  if (otaState != 4 && otaState != 6) {
    display.setCursor(8, 290);
    display.print("BACK = return");
  }
}