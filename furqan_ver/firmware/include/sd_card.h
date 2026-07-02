#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <vector>

#define SD_CS_PIN   18
#define SD_SCK_PIN  14
#define SD_MOSI_PIN 21
#define SD_MISO_PIN 47

extern SPIClass spiSD;
extern SdFat32  sd;

bool setupSD();
bool testSD();
std::vector<String> getReciterFolders();
std::vector<String> getSurahFiles(const String& folder);

#endif
