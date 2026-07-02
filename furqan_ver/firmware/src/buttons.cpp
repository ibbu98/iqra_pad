#include "buttons.h"

static const uint32_t DEBOUNCE_MS     = 12;   // snappier
static const uint32_t REPEAT_DELAY_MS = 180;
static const uint32_t REPEAT_RATE_MS  = 55;   // faster scroll repeat

static int stableAct = 0;
static uint32_t lastChangeTime = 0;
static uint32_t pressStartTime = 0;
static uint32_t lastRepeatTime = 0;

void setupButtons()
{
  pinMode(BTN_UP,     INPUT_PULLUP);  // GPIO 1  -> PREV
  pinMode(BTN_DOWN,   INPUT_PULLUP);  // GPIO 2  -> NEXT
  pinMode(BTN_SELECT, INPUT_PULLUP);  // GPIO 41 -> SELECT
  pinMode(BTN_BACK,   INPUT_PULLUP);  // GPIO 40 -> BACK
}

static int readRawAction()
{
  if (digitalRead(BTN_UP)     == LOW) return ACT_PREV;
  if (digitalRead(BTN_DOWN)   == LOW) return ACT_NEXT;
  if (digitalRead(BTN_SELECT) == LOW) return ACT_SELECT;
  if (digitalRead(BTN_BACK)   == LOW) return ACT_BACK;
  return 0;
}

int readButtons()
{
  const uint32_t now    = millis();
  const int      rawAct = readRawAction();

  if (rawAct != stableAct)
  {
    if ((now - lastChangeTime) < DEBOUNCE_MS) return 0;
    lastChangeTime = now;
    stableAct      = rawAct;

    if (stableAct != 0)
    {
      pressStartTime = now;
      lastRepeatTime = now;
      return stableAct;
    }
    else
    {
      pressStartTime = 0;
      lastRepeatTime = 0;
      return 0;
    }
  }

  // Auto-repeat only for NEXT/PREV — SELECT and BACK fire once per press
  if (stableAct == ACT_NEXT || stableAct == ACT_PREV)
  {
    if ((now - pressStartTime) >= REPEAT_DELAY_MS)
    {
      if ((now - lastRepeatTime) >= REPEAT_RATE_MS)
      {
        lastRepeatTime = now;
        return stableAct;
      }
    }
  }

  return 0;
}
