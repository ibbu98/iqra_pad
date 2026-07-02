#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_SELECT  41
#define BTN_BACK    40

#define ACT_PREV    201
#define ACT_NEXT    202
#define ACT_SELECT  203
#define ACT_BACK    204

void setupButtons();
int readButtons();

#endif
