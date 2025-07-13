// Simple test of USB Host
//
// This example is in the public domain

#include "USBHost_t36.h"
#include <synth_karplusstrong.h>
#include <output_i2s.h>
#include <SD.h>
#include <ILI9341_t3.h>
#include <font_Arial.h>
#include <SPI.h>
#include "HybridSynthesizer.h"  // Use the new hybrid synthesizer

/**
 * Pin usage:
 *
 * TFT Display:
 * CS: 10
 * DC: 9
 * SDI/MOSI: 11
 * SCK: 13
 * SDO/MISO: 12
 * 
 * 
 * Audio:
 * WSEL/LRCLK: 20
 * BCLK: 21
 * DIN: 7
 */

// TFT Display pins
#define TFT_CS   10
#define TFT_DC   9

// Global TFT object
ILI9341_t3 tft(TFT_CS, TFT_DC);

HybridSynthesizer synth;  // Use the new hybrid synthesizer

USBHost myusb;
KeyboardController keyboard1(myusb);
MIDIDevice midi1(myusb);

// Function prototypes
void OnPress(int key);
void OnRawPress(uint8_t keycode);
void OnRawRelease(uint8_t keycode);
void OnNoteOn(byte channel, byte note, byte velocity);
void OnNoteOff(byte channel, byte note, byte velocity);
void OnControlChange(byte channel, byte control, byte value);
void OnPitchChange(byte channel, int bend); 

void setup()
{
    while (!Serial) ; 
    myusb.begin();
    keyboard1.attachPress(OnPress);
    keyboard1.attachRawPress(OnRawPress);
    keyboard1.attachRawRelease(OnRawRelease);
    midi1.setHandleNoteOff(OnNoteOff);
    midi1.setHandleNoteOn(OnNoteOn);
    midi1.setHandleControlChange(OnControlChange);
    midi1.setHandlePitchChange(OnPitchChange); 
    AudioMemory(20);  // Increased for soundfont player

    Serial.println("Hello, world!");
    Serial8.begin(400000, SERIAL_8N1);

    // Initialize TFT Display
    Serial.println("Initializing TFT display...");
    tft.begin();
    tft.setRotation(3); // Landscape mode (320x240)
    tft.fillScreen(ILI9341_BLACK);
    tft.setTextColor(ILI9341_WHITE);
    tft.setFont(Arial_12);
    tft.setCursor(0, 0);
    tft.println("Karplus-Strong Buffer Visualizer");
    tft.drawLine(0, 20, tft.width()-1, 20, ILI9341_WHITE);
    tft.setCursor(0, 25);
    tft.setFont(Arial_10);
    tft.println("String 0 will display buffer graphs");
    Serial.println("TFT display initialized");
    
    // Set the first string synthesizer to use the TFT display
    synth.getString(0).setTFTDisplay(&tft);
    Serial.println("TFT display assigned to string synthesizer 0");

    // Initialize SD card for soundfont loading
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SD card initialization failed - soundfonts will not be available");
    } else {
        Serial.println("SD card initialized");
        
        // Try to load a soundfont file
        if (synth.loadSoundfont("piano.sf2")) {
            Serial.println("Soundfont loaded successfully");
            // Switch to soundfont mode (or use layered/split mode)
            // synth.setSynthMode(HybridSynthesizer::SOUNDFONT_ONLY);
            // synth.setSynthMode(HybridSynthesizer::LAYERED);
        } else {
            Serial.println("Failed to load soundfont, using string synthesis only");
        }
    }
}

void loop()
{
    static uint64_t i = 0;
    if (++i % 1000000 == 0) {
        Serial.print("Alive ");
        Serial.println(i / 1000000);
    }
    myusb.Task();
    midi1.read();
}


void OnPress(int key)
{
	Serial.print("key '");
	Serial.print((char)key);
	Serial.print("'  ");
	Serial.println(key);
	//Serial.print("key ");
	//Serial.print((char)keyboard1.getKey());
	//Serial.print("  ");
	//Serial.print((char)keyboard2.getKey());
	//Serial.println();
}

void OnRawPress(uint8_t keycode)
{
	Serial.print("raw key press: ");
	Serial.println((int)keycode);
}

void OnRawRelease(uint8_t keycode)
{
	Serial.print("raw key release: ");
	Serial.println((int)keycode);
}

void OnNoteOn(byte channel, byte note, byte velocity)
{
    Serial.print("Note On, ch=");
    Serial.print(channel);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.print(velocity);
    Serial.println();

    float freq = 440.0f * exp2f((float)(note - 69) * 0.0833333f);
    float vel = (float)velocity/128.0f;
    Serial.print("Synthesizer.noteOn(");
    Serial.print(freq);
    Serial.print(", ");
    Serial.print(vel);
    Serial.print(")");
    Serial.println();
    synth.noteOn(note, freq, vel);
}

void OnNoteOff(byte channel, byte note, byte velocity)
{
	Serial.print("Note Off, ch=");
	Serial.print(channel);
	Serial.print(", note=");
	Serial.print(note);
	Serial.println(); 
	synth.noteOff(note); 

}

void OnControlChange(byte channel, byte control, byte value)
{
    Serial.print("Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", value=");
    Serial.print(value);
    Serial.println();

    if(channel == 1 && control == 21) {
        synth.setAttenuation(value);
    }
    if(channel == 1 && control == 22) {
        synth.setFilterStrength(value);
    }

    if(channel == 1 && control == 7) {
        synth.setMasterVolume((float)value / 127.0f);  // Updated method name
    }
    
    // New controls for hybrid synthesizer
    if(channel == 1 && control == 23) {
        synth.setStringVolume((float)value / 127.0f);
    }
    
    if(channel == 1 && control == 24) {
        synth.setSoundfontVolume((float)value / 127.0f);
    }
    
    // Synthesis mode switching (CC 25)
    if(channel == 1 && control == 25) {
        if (value < 32) {
            synth.setSynthMode(HybridSynthesizer::STRINGS_ONLY);
            Serial.println("Switched to Strings Only mode");
        } else if (value < 64) {
            synth.setSynthMode(HybridSynthesizer::SOUNDFONT_ONLY);
            Serial.println("Switched to Soundfont Only mode");
        } else if (value < 96) {
            synth.setSynthMode(HybridSynthesizer::LAYERED);
            Serial.println("Switched to Layered mode");
        } else {
            synth.setSynthMode(HybridSynthesizer::SPLIT);
            Serial.println("Switched to Split mode");
        }
    }
    
    // Split point control (CC 26)
    if(channel == 1 && control == 26) {
        uint8_t splitNote = 36 + (value * 48 / 127); // Map to C2-C6 range
        synth.setSplitPoint(splitNote);
        Serial.print("Split point set to note ");
        Serial.println(splitNote);
    }

    if(channel == 1 && control == 51 && value == 127) {
        Serial.print("Max CPU Usage = ");
        Serial.print(AudioProcessorUsageMax(), 1);
        Serial.print("% Max memory usage = ");
		Serial.print(AudioMemoryUsageMax(), 1);
		Serial.print("%");
		Serial.println();
    }
}

void OnPitchChange(byte channel, int bend) // <-- Changed function name
{
    Serial.print("Pitch Bend, ch=");
    Serial.print(channel);
    Serial.print(", bend=");
    Serial.println(bend);
    
    // Convert MIDI pitch bend (-8192 to +8191) to -4.0-4.0 range
    float bendAmount = (float)bend / 8192.0f * 4.0f;
    synth.setPitchBend(bendAmount);
}

