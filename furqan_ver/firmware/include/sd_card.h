#ifndef SD_CARD_H
#define SD_CARD_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <vector> // We use a "vector" to store the list of filenames

// We use Pin 18 for SD CS
#define SD_CS_PIN 18

// Initialize the SD Card
bool setupSD();

// Get a list of all .mp3 files
std::vector<String> getMusicFiles();

#endif