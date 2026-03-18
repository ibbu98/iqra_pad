#include "13_quran_sm.h"
#include <GxEPD2_BW.h>
#include "13_line_quran_pages.h"

// ---------------- Display size ----------------
static const int DISP_W = 400;
static const int DISP_H = 300;

// ---------------- Layout ----------------
static const int TITLE_Y = 42;

// Menu layout (FIXED: larger spacing + proper row clearing)
static const int MENU_ROW_H   = 30;   // each menu row height (more spacing)
static const int MENU_ROW_PAD = 6;    // extra clear padding
static const int MENU_FIRST_ROW_TOP = 100; // top of the first row box

// text baseline inside a row box
static const int MENU_TEXT_BASELINE_OFFSET = 20; // where cursor baseline sits inside row

// Surah/Juz layout
static const int LIST_VALUE_Y = 150;
static const int LIST_BOX_H   = 34;

// Reader refresh strategy
static int g_flipCount = 0;
static const int FULL_REFRESH_EVERY_N_FLIPS = 12;

// ---------------- Text center helpers ----------------
static int textWidthPx(const char* s, int textSize)
{
  int len = (int)strlen(s);
  int charW = 6 * textSize; // default GFX font approx
  return len * charW;
}

static int centerX(const char* s, int textSize)
{
  int w = textWidthPx(s, textSize);
  return (DISP_W - w) / 2;
}

static void setPartialSafe(MyDisplay &d, int x, int y, int w, int h)
{
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  if (x + w > d.width())  w = d.width() - x;
  if (y + h > d.height()) h = d.height() - y;
  d.setPartialWindow(x, y, w, h);
}

// ---------------- Drawing ----------------

static void drawTitleCentered(MyDisplay &d, const char *title)
{
  d.setTextColor(GxEPD_BLACK);
  d.setTextSize(2);
  d.setCursor(centerX(title, 2), TITLE_Y);
  d.print(title);
}

static int menuRowTop(int idx)
{
  return MENU_FIRST_ROW_TOP + idx * MENU_ROW_H;
}

static void drawMenuLineCentered(MyDisplay &d, int idx, bool selected)
{
  const char *items[] = {"Surah", "Juz", "Bookmark", "Last Read"};

  char buf[32];
  snprintf(buf, sizeof(buf), "%s%s", selected ? "> " : "  ", items[idx]);

  int top = menuRowTop(idx);

  // ✅ Clear full row box (prevents leftovers/overwriting)
  d.fillRect(0, top, DISP_W, MENU_ROW_H, GxEPD_WHITE);

  // Print centered inside the row
  d.setTextColor(GxEPD_BLACK);
  d.setTextSize(1);
  d.setCursor(centerX(buf, 1), top + MENU_TEXT_BASELINE_OFFSET);
  d.print(buf);
}

static void drawListValueCentered(MyDisplay &d, const char *label, int value, int maxVal)
{
  char buf[40];
  snprintf(buf, sizeof(buf), "%s: %d / %d", label, value, maxVal);

  // ✅ Clear full value row box
  d.fillRect(0, LIST_VALUE_Y - 22, DISP_W, LIST_BOX_H, GxEPD_WHITE);

  d.setTextColor(GxEPD_BLACK);
  d.setTextSize(1);
  d.setCursor(centerX(buf, 1), LIST_VALUE_Y);
  d.print(buf);
}

// ---------------------- Quran13SM ----------------------

void Quran13SM::init(MyDisplay *disp)
{
  display = disp;
  exitToHome = false;

  state = Q13_MENU;
  menuIndex = 0;
  listIndex = 0;
  pageIndex = 0;
  lastReadIndex = 0;
  openedFromSurah = true;

  g_flipCount = 0;
}

void Quran13SM::enter()
{
  exitToHome = false;
  state = Q13_MENU;
  menuIndex = 0;
  renderMenuFull();
}

// ---------------- Clamp ----------------

void Quran13SM::clampMenu(){ if(menuIndex<0)menuIndex=0; if(menuIndex>3)menuIndex=3; }
void Quran13SM::clampSurah(){ if(listIndex<0)listIndex=0; if(listIndex>113)listIndex=113; }
void Quran13SM::clampJuz(){ if(listIndex<0)listIndex=0; if(listIndex>29)listIndex=29; }
void Quran13SM::clampPage(){
  if(pageIndex<0) pageIndex = (int)QURAN13_PAGE_COUNT-1;
  if(pageIndex>=(int)QURAN13_PAGE_COUNT) pageIndex=0;
}

// ---------------- Full screens ----------------

void Quran13SM::renderMenuFull()
{
  display->setFullWindow();
  display->firstPage();
  do {
    display->fillScreen(GxEPD_WHITE);
    drawTitleCentered(*display, "13-Line Quran");
    for(int i=0;i<4;i++)
      drawMenuLineCentered(*display, i, i==menuIndex);
  } while(display->nextPage());
}

void Quran13SM::renderSurahFull()
{
  display->setFullWindow();
  display->firstPage();
  do {
    display->fillScreen(GxEPD_WHITE);
    drawTitleCentered(*display, "Surah");
    drawListValueCentered(*display, "Surah", listIndex+1, 114);
  } while(display->nextPage());
}

void Quran13SM::renderJuzFull()
{
  display->setFullWindow();
  display->firstPage();
  do {
    display->fillScreen(GxEPD_WHITE);
    drawTitleCentered(*display, "Juz");
    drawListValueCentered(*display, "Juz", listIndex+1, 30);
  } while(display->nextPage());
}

void Quran13SM::renderBookmarkFull()
{
  display->setFullWindow();
  display->firstPage();
  do {
    display->fillScreen(GxEPD_WHITE);
    drawTitleCentered(*display, "Bookmark");
  } while(display->nextPage());
}

// ---------------- Reader ----------------

void Quran13SM::renderReader()
{
  g_flipCount++;
  bool doFull = (g_flipCount % FULL_REFRESH_EVERY_N_FLIPS)==0;

  if(doFull) display->setFullWindow();
  else       display->setPartialWindow(0,0,QURAN_W,QURAN_H);

  display->firstPage();
  do {
    if(doFull) display->fillScreen(GxEPD_WHITE);
    display->drawBitmap(0,0,QURAN13_PAGES[pageIndex],QURAN_W,QURAN_H,GxEPD_BLACK);
  } while(display->nextPage());
}

// ---------------- Partial updates ----------------

void Quran13SM::updateMenuCursor(int oldSel,int newSel)
{
  // Redraw only old and new row boxes (full row height)
  int idxs[2] = { oldSel, newSel };

  for(int k=0;k<2;k++)
  {
    int idx = idxs[k];
    int top = menuRowTop(idx);

    // ✅ Partial window covers full row box + padding
    setPartialSafe(*display, 0, top - MENU_ROW_PAD, DISP_W, MENU_ROW_H + 2*MENU_ROW_PAD);

    display->firstPage();
    do{
      drawMenuLineCentered(*display, idx, idx==newSel);
    }while(display->nextPage());
  }
}

void Quran13SM::updateSurahNumber(int v)
{
  setPartialSafe(*display, 0, LIST_VALUE_Y - 26, DISP_W, LIST_BOX_H + 10);
  display->firstPage();
  do{ drawListValueCentered(*display,"Surah",v,114); }
  while(display->nextPage());
}

void Quran13SM::updateJuzNumber(int v)
{
  setPartialSafe(*display, 0, LIST_VALUE_Y - 26, DISP_W, LIST_BOX_H + 10);
  display->firstPage();
  do{ drawListValueCentered(*display,"Juz",v,30); }
  while(display->nextPage());
}

// ---------------- Actions ----------------

void Quran13SM::onNext()
{
  if(state==Q13_MENU){
    int o=menuIndex; menuIndex++; clampMenu();
    if(o!=menuIndex) updateMenuCursor(o,menuIndex);
    return;
  }
  if(state==Q13_SURAH_LIST){
    int o=listIndex; listIndex++; clampSurah();
    if(o!=listIndex) updateSurahNumber(listIndex+1);
    return;
  }
  if(state==Q13_JUZ_LIST){
    int o=listIndex; listIndex++; clampJuz();
    if(o!=listIndex) updateJuzNumber(listIndex+1);
    return;
  }
  if(state==Q13_READER){
    pageIndex=(pageIndex+1)%((int)QURAN13_PAGE_COUNT);
    lastReadIndex=pageIndex;
    renderReader();
  }
}

void Quran13SM::onPrev()
{
  if(state==Q13_MENU){
    int o=menuIndex; menuIndex--; clampMenu();
    if(o!=menuIndex) updateMenuCursor(o,menuIndex);
    return;
  }
  if(state==Q13_SURAH_LIST){
    int o=listIndex; listIndex--; clampSurah();
    if(o!=listIndex) updateSurahNumber(listIndex+1);
    return;
  }
  if(state==Q13_JUZ_LIST){
    int o=listIndex; listIndex--; clampJuz();
    if(o!=listIndex) updateJuzNumber(listIndex+1);
    return;
  }
  if(state==Q13_READER){
    pageIndex--; clampPage();
    lastReadIndex=pageIndex;
    renderReader();
  }
}

void Quran13SM::onSelect()
{
  if(state==Q13_READER) return;

  if(state==Q13_MENU){
    if(menuIndex==0){ state=Q13_SURAH_LIST; listIndex=0; renderSurahFull(); }
    else if(menuIndex==1){ state=Q13_JUZ_LIST; listIndex=0; renderJuzFull(); }
    else if(menuIndex==2){ state=Q13_BOOKMARK; renderBookmarkFull(); }
    else if(menuIndex==3){
      pageIndex=lastReadIndex;
      state=Q13_READER;
      renderReader();
    }
    return;
  }

  if(state==Q13_SURAH_LIST || state==Q13_JUZ_LIST){
    pageIndex=0;
    lastReadIndex=pageIndex;
    state=Q13_READER;
    renderReader();
  }
}

void Quran13SM::onBack()
{
  if(state==Q13_MENU){ exitToHome=true; return; }
  state=Q13_MENU;
  renderMenuFull();
}
