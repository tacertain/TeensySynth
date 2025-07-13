// Example usage of KarplusStrongStringSynth with external TFT initialization

#include <Arduino.h>
#include <ILI9341_t3.h>
#include <font_Arial.h>
#include <SPI.h>
#include "karplus_strong_string_synth.h"

// TFT Display pins
#define TFT_CS   10
#define TFT_DC   9

// Global TFT object - initialize once and share
ILI9341_t3 tft(TFT_CS, TFT_DC);

// Synthesizer objects
KarplusStrongStringSynth synth1;  // Will use default constructor (no display)
KarplusStrongStringSynth synth2(&tft);  // Pass TFT pointer directly

void setup() {
  Serial.begin(115200);
  
  // Initialize TFT display once
  tft.begin();
  tft.setRotation(3); // Landscape mode
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setFont(Arial_12);
  tft.setCursor(0, 0);
  tft.println("Karplus-Strong Buffer Visualizer");
  tft.drawLine(0, 20, tft.width()-1, 20, ILI9341_WHITE);
  
  // Option 1: Pass TFT to constructor (synth2 already has it)
  
  // Option 2: Set TFT after construction
  synth1.setTFTDisplay(&tft);
  
  // Option 3: No display (pass nullptr or don't call setTFTDisplay)
  // KarplusStrongStringSynth synth3;  // No display, no graphing
  
  // Start synthesizer
  synth1.noteOn(440.0, 0.8);  // A4 note
}

void loop() {
  // Audio processing happens in interrupt
  delay(1000);
}

// For arrays (like in HybridSynthesizer), you would do:
void setupSynthArray() {
  KarplusStrongStringSynth strings[8];
  
  // Set display for all synthesizers (they will share the same display)
  for (int i = 0; i < 8; i++) {
    strings[i].setTFTDisplay(&tft);
  }
  
  // Or set display only for the first one to avoid overlapping graphs
  strings[0].setTFTDisplay(&tft);
  // Others will have no display (tftInitialized = false)
}
