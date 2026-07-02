#pragma once
#include <Arduino.h>

// Pixel dimensions — must match the .bin files you generated
static constexpr uint16_t QURAN_PX_W    = 400;
static constexpr uint16_t QURAN_PX_H    = 300;
static constexpr uint32_t QURAN_BUF_SZ  = (uint32_t)QURAN_PX_W * QURAN_PX_H / 8;  // 15000

// Returns a pointer to the loaded page bitmap (PSRAM), or nullptr on failure.
// The pointer is valid until the next call to quranLoadPage().
// pageNum is 1-based (page 1 = "0001.bin").
const uint8_t* quranLoadPage(const char* sdFolder, int pageNum);

// Release the PSRAM buffer (call only when completely done with the viewer).
void quranFreeBuffer();
