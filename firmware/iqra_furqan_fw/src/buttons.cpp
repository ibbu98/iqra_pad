#include "buttons.h"

static const uint32_t DEBOUNCE_MS = 120;

// Track last stable button
static int lastBtn = 0;
static uint32_t lastEventTime = 0;

void setupButtons()
{
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
}

// Reads hardware and returns currently pressed button (or 0)
static int readRaw()
{
    // Active LOW
    if (digitalRead(BTN_UP) == LOW)     return BTN_UP;
    if (digitalRead(BTN_DOWN) == LOW)   return BTN_DOWN;
    if (digitalRead(BTN_LEFT) == LOW)   return BTN_LEFT;
    if (digitalRead(BTN_RIGHT) == LOW)  return BTN_RIGHT;
    if (digitalRead(BTN_SELECT) == LOW) return BTN_SELECT;
    return 0;
}

int readButtons()
{
    int nowBtn = readRaw();
    uint32_t now = millis();

    // If button state changed (press or release), debounce it
    if (nowBtn != lastBtn)
    {
        if ((now - lastEventTime) >= DEBOUNCE_MS)
        {
            lastEventTime = now;
            lastBtn = nowBtn;

            // We only generate an event on PRESS (not release)
            if (nowBtn != 0)
                return nowBtn;
        }
    }

    return 0;
}
