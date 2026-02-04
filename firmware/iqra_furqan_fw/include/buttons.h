#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

// --- PINS (KEEP SAME VALUES) ---
#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_LEFT    41
#define BTN_RIGHT   40
#define BTN_SELECT  42   // present but unused in your new mapping

// --- LOGICAL ACTIONS returned by readButtons() ---
#define ACT_PREV    201   // PIN 1
#define ACT_NEXT    202   // PIN 2
#define ACT_SELECT  203   // PIN 41
#define ACT_BACK    204   // PIN 40

void setupButtons();

// Returns: ACT_NEXT / ACT_PREV / ACT_SELECT / ACT_BACK
// or 0 if nothing new was pressed
int readButtons();

#endif
