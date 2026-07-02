#include "tasbeeh.h"

TasbeehApp tasbeeh;

static const int DW = 400;

// Layout
static const int HDR_H  = 50;
static const int BOX_X  = 75;
static const int BOX_Y  = 58;
static const int BOX_W  = 250;
static const int BOX_H  = 115;
static const int BEAD_Y = 194;
static const int DIV_Y  = 250;
static const int HINT_Y = 263;
static const int HINT2_Y= 283;

// ---- Decorative helpers ----

static void drawDiamond(MyDisplay &d, int cx, int cy, int s, uint16_t col)
{
  d.drawLine(cx,     cy - s, cx + s, cy,     col);
  d.drawLine(cx + s, cy,     cx,     cy + s, col);
  d.drawLine(cx,     cy + s, cx - s, cy,     col);
  d.drawLine(cx - s, cy,     cx,     cy - s, col);
}

static void drawHeader(MyDisplay &d)
{
  d.fillRect(0, 0, DW, HDR_H, GxEPD_BLACK);

  // Corner bracket lines
  d.drawLine(0,      0,       18,     0,        GxEPD_WHITE);
  d.drawLine(0,      0,       0,      18,       GxEPD_WHITE);
  d.drawLine(DW - 1, 0,       DW - 19, 0,       GxEPD_WHITE);
  d.drawLine(DW - 1, 0,       DW - 1,  18,      GxEPD_WHITE);
  d.drawLine(0,      HDR_H-1, 18,      HDR_H-1, GxEPD_WHITE);
  d.drawLine(0,      HDR_H-1, 0,       HDR_H-19,GxEPD_WHITE);
  d.drawLine(DW - 1, HDR_H-1, DW - 19, HDR_H-1, GxEPD_WHITE);
  d.drawLine(DW - 1, HDR_H-1, DW - 1,  HDR_H-19,GxEPD_WHITE);

  // Diamond accents
  drawDiamond(d, 30,  25, 9, GxEPD_WHITE);
  drawDiamond(d, 370, 25, 9, GxEPD_WHITE);
  drawDiamond(d, 53,  25, 5, GxEPD_WHITE);
  drawDiamond(d, 347, 25, 5, GxEPD_WHITE);

  // Title
  const char* title = "T A S B E E H";
  d.setFont(&FreeSansBold12pt7b);
  d.setTextSize(1);
  d.setTextColor(GxEPD_WHITE);
  int16_t tbx, tby; uint16_t tbw, tbh;
  d.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
  d.setCursor((DW - tbw) / 2 - tbx, 33);
  d.print(title);
}

// ---- Counter box ----

static void drawCounterBox(MyDisplay &d, int count)
{
  d.fillRect(BOX_X, BOX_Y, BOX_W, BOX_H, GxEPD_WHITE);

  // Triple border
  d.drawRect(BOX_X,     BOX_Y,     BOX_W,     BOX_H,     GxEPD_BLACK);
  d.drawRect(BOX_X + 2, BOX_Y + 2, BOX_W - 4, BOX_H - 4, GxEPD_BLACK);
  d.drawRect(BOX_X + 4, BOX_Y + 4, BOX_W - 8, BOX_H - 8, GxEPD_BLACK);

  // Filled corner squares (frame effect)
  d.fillRect(BOX_X,                BOX_Y,                6, 6, GxEPD_BLACK);
  d.fillRect(BOX_X + BOX_W - 6,    BOX_Y,                6, 6, GxEPD_BLACK);
  d.fillRect(BOX_X,                BOX_Y + BOX_H - 6,    6, 6, GxEPD_BLACK);
  d.fillRect(BOX_X + BOX_W - 6,    BOX_Y + BOX_H - 6,    6, 6, GxEPD_BLACK);

  // Large number (built-in font, textSize 7)
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", count);

  d.setFont(NULL);
  d.setTextSize(7);
  d.setTextColor(GxEPD_BLACK);

  int16_t tbx, tby; uint16_t tbw, tbh;
  d.getTextBounds(buf, 0, 0, &tbx, &tby, &tbw, &tbh);
  int nx = BOX_X + (BOX_W - tbw) / 2 - tbx;
  int ny = BOX_Y + (BOX_H - tbh) / 2 - tby;
  d.setCursor(nx, ny);
  d.print(buf);
}

// ---- 33-bead row ----

static void drawBeads(MyDisplay &d, int count)
{
  const int r    = 3;
  const int step = 2 * r + 2;
  const int ggap = 14;
  const int gw   = (11 - 1) * step + 2*r;
  const int span = 3 * gw + 2 * ggap;
  const int x0   = (DW - span) / 2;

  int filled = (count % 33 == 0 && count > 0) ? 33 : (count % 33);

  for (int i = 0; i < 33; i++) {
    int g   = i / 11;
    int pos = i % 11;
    int cx  = x0 + g * (gw + ggap) + pos * step + r;

    if (i < filled)
      d.fillCircle(cx, BEAD_Y, r, GxEPD_BLACK);
    else
      d.drawCircle(cx, BEAD_Y, r, GxEPD_BLACK);
  }

  int d1x = x0 + gw + ggap / 2;
  int d2x = x0 + 2 * (gw + ggap) - ggap / 2;
  d.drawLine(d1x, BEAD_Y - 7, d1x, BEAD_Y + 7, GxEPD_BLACK);
  d.drawLine(d2x, BEAD_Y - 7, d2x, BEAD_Y + 7, GxEPD_BLACK);
}

// ---- Static hints (drawn once on full render) ----

static void drawHints(MyDisplay &d)
{
  d.drawLine(5, DIV_Y, DW - 5, DIV_Y, GxEPD_BLACK);

  d.setFont(&FreeSansBold9pt7b);
  d.setTextSize(1);
  d.setTextColor(GxEPD_BLACK);

  d.setCursor(10, HINT_Y);
  d.print("UP: Count");

  const char* bk = "BACK: Exit";
  int16_t tbx, tby; uint16_t tbw, tbh;
  d.getTextBounds(bk, 0, 0, &tbx, &tby, &tbw, &tbh);
  d.setCursor(DW - tbw - tbx - 10, HINT_Y);
  d.print(bk);

  // Second hints line — centered
  const char* rst = "Hold SELECT 3s: Reset";
  d.getTextBounds(rst, 0, 0, &tbx, &tby, &tbw, &tbh);
  d.setCursor((DW - tbw) / 2 - tbx, HINT2_Y);
  d.print(rst);
}

// ==== TasbeehApp ====

void TasbeehApp::init(MyDisplay *disp)
{
  display      = disp;
  count        = 0;
  _partialUsed = false;
}

void TasbeehApp::enter()
{
  // count is intentionally NOT reset here — persists across back/re-enter
  renderFull();
}

void TasbeehApp::onUp()
{
  count++;
  if (count % 100 == 0)
    renderFull();        // periodic full refresh to prevent ghosting
  else
    renderCountRegion();
}

void TasbeehApp::resetCount()
{
  count = 0;
  renderFull();
}

void TasbeehApp::renderFull()
{
  _partialUsed = false;  // full refresh returns SSD1683 to clean state
  display->setFullWindow();
  display->firstPage();
  do {
    display->fillScreen(GxEPD_WHITE);
    drawHeader(*display);
    drawCounterBox(*display, count);
    drawBeads(*display, count);
    drawHints(*display);
  } while (display->nextPage());
}

void TasbeehApp::renderCountRegion()
{
  _partialUsed = true;  // SSD1683 is now in partial-active state (0xfc leaves power on)
  display->setPartialWindow(0, BOX_Y, DW, DIV_Y - BOX_Y);
  display->firstPage();
  do {
    display->fillRect(0, BOX_Y, DW, DIV_Y - BOX_Y, GxEPD_WHITE);
    drawCounterBox(*display, count);
    drawBeads(*display, count);
  } while (display->nextPage());
}

void TasbeehApp::exitPage()
{
  if (_partialUsed) {
    // Partial refresh left SSD1683 with oscillator+analog still on (_power_is_on=true).
    // A full refresh here puts the controller back into a clean known state so the
    // next page (home) can do its own full refresh without corruption.
    renderFull();
  }
}
