#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <Arduino.h>

// MAX98357A I2S pins
#define I2S_BCLK_PIN   6
#define I2S_LRCK_PIN   7
#define I2S_DOUT_PIN   8

// Volume pot wiper
#define VOL_ADC_PIN   17

void audioInit();
void audioPlay(const String& folder, const String& fileNoExt);
void audioStop();
bool audioIsPlaying();
bool audioJustFinished();   // true once when track ends naturally
void audioSetVolume(float v);  // 0.0–1.0
void audioLoop();              // call every loop() iteration

#endif
