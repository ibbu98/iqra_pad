#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

// --- PINS ---
#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_LEFT    41
#define BTN_RIGHT   40
#define BTN_SELECT  42

void setupButtons();

// Returns: BTN_UP / BTN_DOWN / BTN_LEFT / BTN_RIGHT / BTN_SELECT
// or 0 if nothing new was pressed
int readButtons();

#endif
