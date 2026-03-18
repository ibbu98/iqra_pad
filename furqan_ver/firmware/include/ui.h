#ifndef UI_H
#define UI_H

#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h> 

// IMPORTANT: This file must exist in your 'src' folder
#include "FreeMonoBold12pt7b.h"

#include <vector> // Needed for the list

// Define Display Driver (GDEY042T81)
typedef GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> MyDisplay;

// Function to draw the page with selection
void drawHomePage(MyDisplay &display, int selectedItem);

// NEW: Function to draw the Music List
// Takes the list of files and the currently selected index
void drawMusicPage(MyDisplay &display, std::vector<String> &files, int selectedIndex);

#endif