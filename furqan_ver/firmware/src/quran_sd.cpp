#include "quran_sd.h"
#include "sd_card.h"
#include <esp_heap_caps.h>

// Single PSRAM buffer shared across page loads.
// Allocated once on first use; lives in the 8 MB OPI PSRAM (not SRAM).
static uint8_t* _buf = nullptr;

const uint8_t* quranLoadPage(const char* sdFolder, int pageNum)
{
  // Allocate the buffer once (15 KB from PSRAM — trivial for 8 MB PSRAM)
  if (!_buf) {
    _buf = (uint8_t*)heap_caps_malloc(QURAN_BUF_SZ, MALLOC_CAP_SPIRAM);
    if (!_buf) {
      Serial.println("[QURAN] FAIL — could not allocate PSRAM buffer");
      return nullptr;
    }
    Serial.printf("[QURAN] PSRAM buffer allocated  %lu bytes\n", (unsigned long)QURAN_BUF_SZ);
  }

  // Build path:  /13line/page_0042.bin  or  /15line/page_0042.bin
  char path[40];
  snprintf(path, sizeof(path), "/%s/page_%04d.bin", sdFolder, pageNum);

  File32 f;
  if (!f.open(path, O_RDONLY)) {
    Serial.printf("[QURAN] FAIL — cannot open %s\n", path);
    return nullptr;
  }

  uint32_t got = f.read(_buf, QURAN_BUF_SZ);
  f.close();

  if (got != QURAN_BUF_SZ) {
    Serial.printf("[QURAN] FAIL — read %lu bytes from %s (expected %lu)\n",
                  (unsigned long)got, path, (unsigned long)QURAN_BUF_SZ);
    return nullptr;
  }

  Serial.printf("[QURAN] loaded %s\n", path);
  return _buf;
}

void quranFreeBuffer()
{
  if (_buf) {
    heap_caps_free(_buf);
    _buf = nullptr;
  }
}
