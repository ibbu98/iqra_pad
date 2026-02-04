#pragma once
#include <Arduino.h>
#include "ui.h"
#include "13_line_quran_pages.h"

enum Q13State
{
  Q13_MENU = 0,
  Q13_SURAH_LIST,
  Q13_JUZ_LIST,
  Q13_BOOKMARK,
  Q13_READER
};

class Quran13SM
{
public:
  void init(MyDisplay *disp);
  void enter();

  // 4-action navigation
  void onNext();
  void onPrev();
  void onSelect();
  void onBack();

  bool shouldExitToHome() const { return exitToHome; }
  void clearExitToHome() { exitToHome = false; }

private:
  MyDisplay *display = nullptr;
  bool exitToHome = false;

  Q13State state = Q13_MENU;

  // Menu cursor: 0..3 (Surah, Juz, Bookmark, Last Read)
  int menuIndex = 0;

  // List cursor for Surah/Juz
  int listIndex = 0;

  // Reader page index (bitmap index)
  int pageIndex = 0;

  // RAM last read page index
  int lastReadIndex = 0;

  // remember source list (optional)
  bool openedFromSurah = true;

private:
  // Full renders (only on entering a screen)
  void renderMenuFull();
  void renderSurahFull();
  void renderJuzFull();
  void renderBookmarkFull();
  void renderReader();

  // Partial updates (fast)
  void updateMenuCursor(int oldSel, int newSel);
  void updateSurahNumber(int newVal);
  void updateJuzNumber(int newVal);

  // Helpers
  void clampMenu();
  void clampSurah();
  void clampJuz();
  void clampPage();
};
