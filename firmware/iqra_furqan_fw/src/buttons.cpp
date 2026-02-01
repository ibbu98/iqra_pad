#include "buttons.h"

void setupButtons() {
    // Configure pins as Inputs with Pull-ups
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
}

int readButtons() {
    // Check each button (Active LOW)
    if (digitalRead(BTN_UP) == LOW)     return BTN_UP;
    if (digitalRead(BTN_DOWN) == LOW)   return BTN_DOWN;
    if (digitalRead(BTN_LEFT) == LOW)   return BTN_LEFT;
    if (digitalRead(BTN_RIGHT) == LOW)  return BTN_RIGHT;
    if (digitalRead(BTN_SELECT) == LOW) return BTN_SELECT;
    
    return 0; 
}