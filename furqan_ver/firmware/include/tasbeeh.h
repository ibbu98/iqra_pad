#pragma once
#include "ui.h"

class TasbeehApp {
public:
  void init(MyDisplay *disp);
  void enter();
  void onUp();
  void resetCount();
  void exitPage();   // call before returning to home; cleans partial-refresh state

private:
  MyDisplay *display      = nullptr;
  int        count        = 0;
  bool       _partialUsed = false;  // true when partial refresh happened since last full

  void renderFull();
  void renderCountRegion();
};

extern TasbeehApp tasbeeh;
