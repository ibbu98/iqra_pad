#include "buttons.h"

// Smooth navigation (4-action)
// - Debounce for clean presses
// - Auto-repeat only for NEXT/PREV
// - SELECT/BACK do not repeat

static const uint32_t DEBOUNCE_MS      = 30;
static const uint32_t REPEAT_DELAY_MS  = 250;
static const uint32_t REPEAT_RATE_MS   = 90;

static int stableAct = 0;
static uint32_t lastChangeTime = 0;
static uint32_t pressStartTime = 0;
static uint32_t lastRepeatTime = 0;

void setupButtons()
{
  // NOTE: these macros are pin numbers in your project
  pinMode(BTN_UP, INPUT_PULLUP);      // PIN 1 -> PREV
  pinMode(BTN_DOWN, INPUT_PULLUP);    // PIN 2 -> NEXT
  pinMode(BTN_LEFT, INPUT_PULLUP);    // PIN 41 -> SELECT
  pinMode(BTN_RIGHT, INPUT_PULLUP);   // PIN 40 -> BACK
  pinMode(BTN_SELECT, INPUT_PULLUP);  // PIN 42 -> unused now (but safe)
}

// Active LOW
static int readRawAction()
{
  // EXACT mapping you asked:
  // PIN 1 (BTN_UP)    -> PREV
  // PIN 2 (BTN_DOWN)  -> NEXT
  // PIN 41 (BTN_LEFT) -> SELECT
  // PIN 40 (BTN_RIGHT)-> BACK

  if (digitalRead(BTN_UP) == LOW)      return ACT_PREV;
  if (digitalRead(BTN_DOWN) == LOW)    return ACT_NEXT;
  if (digitalRead(BTN_LEFT) == LOW)    return ACT_SELECT;
  if (digitalRead(BTN_RIGHT) == LOW)   return ACT_BACK;

  // BTN_SELECT (42) ignored intentionally for now
  return 0;
}

int readButtons()
{
  const uint32_t now = millis();
  const int rawAct = readRawAction();

  // Debounce stable transitions
  if (rawAct != stableAct)
  {
    if ((now - lastChangeTime) < DEBOUNCE_MS)
      return 0;

    lastChangeTime = now;
    stableAct = rawAct;

    // Press event only
    if (stableAct != 0)
    {
      pressStartTime = now;
      lastRepeatTime = now;
      return stableAct;
    }
    else
    {
      // release
      pressStartTime = 0;
      lastRepeatTime = 0;
      return 0;
    }
  }

  // Auto-repeat ONLY for NEXT/PREV
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
