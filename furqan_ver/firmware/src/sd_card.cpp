#include "sd_card.h"
#include <algorithm>

SPIClass spiSD(HSPI);
SdFat32  sd;

bool setupSD()
{
  spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, -1);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(100);

  SdSpiConfig cfg(SD_CS_PIN, DEDICATED_SPI, 25000000UL, &spiSD);
  Serial.print("[SD] Mounting ... ");
  if (!sd.begin(cfg)) {
    Serial.println("FAILED — CS=18 SCK=14 MOSI=21 MISO=47 VCC=5V");
    return false;
  }
  Serial.println("OK");
  uint32_t szMB = (uint32_t)(sd.card()->sectorCount() / 2048UL);
  Serial.printf("[SD] %lu MB\n", szMB);
  return true;
}

bool testSD()
{
  const char* path = "iqra_test.txt";
  File32 f;
  if (!f.open(path, O_WRITE | O_CREAT | O_TRUNC)) return false;
  f.println("iqra_pad OK");
  f.close();

  if (!f.open(path, O_READ)) return false;
  char buf[32] = {};
  f.read(buf, sizeof(buf) - 1);
  f.close();
  sd.remove(path);

  bool ok = (strncmp(buf, "iqra_pad OK", 11) == 0);
  Serial.printf("[SD] read test %s\n", ok ? "PASS" : "FAIL");
  return ok;
}

std::vector<String> getReciterFolders()
{
  std::vector<String> v;
  File32 root;
  if (!root.open("/")) return v;
  root.rewindDirectory();
  File32 entry;
  while (entry.openNext(&root, O_RDONLY)) {
    char name[64];
    entry.getName(name, sizeof(name));
    String n = String(name);
    if (entry.isDirectory() && name[0] != '.' &&
        n != "System Volume Information" &&
        n != "13line" && n != "15line")
      v.push_back(n);
    entry.close();
  }
  root.close();
  std::sort(v.begin(), v.end());
  return v;
}

std::vector<String> getSurahFiles(const String& folder)
{
  std::vector<String> v;
  File32 dir;
  if (!dir.open(("/" + folder).c_str())) return v;
  File32 entry;
  while (entry.openNext(&dir, O_RDONLY)) {
    if (!entry.isDirectory()) {
      char name[128];
      entry.getName(name, sizeof(name));
      String n = String(name);
      if (n.endsWith(".mp3") || n.endsWith(".MP3"))
        v.push_back(n.substring(0, n.lastIndexOf('.')));
    }
    entry.close();
  }
  dir.close();
  std::sort(v.begin(), v.end());
  return v;
}
