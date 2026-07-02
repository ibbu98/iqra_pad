#pragma once
#include <stdint.h>

struct SurahEntry {
  uint8_t     num;
  const char* name;
  uint16_t    page15;   // 15-line Madinah Mushaf (604 pages)
  uint16_t    page13;   // 13-line Pakistani Quran — fill in when images are mapped
};

extern const SurahEntry SURAH_TABLE[114];
extern const uint16_t   JUZ_PAGE15[30];   // first page of each juz, 15-line
extern const uint16_t   JUZ_PAGE13[30];   // first page of each juz, 13-line (fill in later)
