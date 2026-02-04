#pragma once
#include <Arduino.h>

static const uint16_t QURAN_W = 400;
static const uint16_t QURAN_H = 300;
static const uint32_t QURAN_BYTES = (QURAN_W * QURAN_H) / 8; // 15000

// Your 5 embedded pages (must match the array names inside quran_page_0001.cpp ... )
extern const uint8_t quran_page_0001[] PROGMEM;
extern const uint8_t quran_page_0002[] PROGMEM;
extern const uint8_t quran_page_0003[] PROGMEM;
extern const uint8_t quran_page_0004[] PROGMEM;
extern const uint8_t quran_page_0005[] PROGMEM;

// Table + count
extern const uint8_t* const QURAN13_PAGES[];
extern const uint8_t QURAN13_PAGE_COUNT;
