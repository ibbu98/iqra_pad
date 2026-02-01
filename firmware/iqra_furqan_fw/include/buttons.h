#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

// --- NEW PINS (Right Side Header) ---
// These are much safer and rarely conflict with anything!
#define BTN_UP      1   
#define BTN_DOWN    2   
#define BTN_LEFT    41  
#define BTN_RIGHT   40  
#define BTN_SELECT  42  

void setupButtons();
int readButtons();

#endif