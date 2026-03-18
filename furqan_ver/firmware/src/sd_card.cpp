#include "sd_card.h"

#define SCREEN_CS_PIN 10 

// --- DEBUG HELPER ---
void printDirectory(File dir, int numTabs) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        
        // Print indentation for folders
        for (uint8_t i = 0; i < numTabs; i++) {
            Serial.print("  ");
        }
        
        Serial.print(entry.name());
        
        if (entry.isDirectory()) {
            Serial.println("/");
            printDirectory(entry, numTabs + 1); // Go inside!
        } else {
            // Check file size
            Serial.print("\t\t");
            Serial.println(entry.size(), DEC);
        }
        entry.close();
    }
}

// --- SETUP ---
bool setupSD() {
    // 1. Silence Display
    pinMode(SCREEN_CS_PIN, OUTPUT);
    digitalWrite(SCREEN_CS_PIN, HIGH);
    delay(10);

    Serial.print("Mounting SD Card... ");
    // Try standard speed first
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("FAILED!");
        Serial.println(" -> Check Wiring: MISO(13), MOSI(11), SCK(12), CS(18)");
        Serial.println(" -> Check Format: Must be FAT32 (not exFAT)");
        return false;
    }
    
    Serial.println("SUCCESS!");
    Serial.println("--- STARTING FILE SCAN ---");
    
    File root = SD.open("/");
    printDirectory(root, 0); // Print EVERYTHING to Serial
    
    Serial.println("--- SCAN COMPLETE ---");
    return true;
}

// --- FIND MP3s (Recursive) ---
void findMp3s(File dir, std::vector<String> &files) {
    // Rewind directory to start
    dir.rewindDirectory();
    
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;

        String fileName = entry.name();

        if (entry.isDirectory()) {
            // RECURSION: Go deeper into the folder
            findMp3s(entry, files);
        } else {
            // CHECK EXTENSION
            if (fileName.endsWith(".mp3") || fileName.endsWith(".MP3")) {
                Serial.print("ADDED TO LIST: ");
                Serial.println(fileName);
                files.push_back(fileName);
            }
        }
        entry.close();
    }
}

std::vector<String> getMusicFiles() {
    std::vector<String> files;
    File root = SD.open("/");
    if (root) {
        findMp3s(root, files);
    }
    return files;
}