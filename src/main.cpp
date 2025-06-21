// Simple test of USB Host
//
// This example is in the public domain
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "ILI9341_t3.h"
#include "USBHost_t36.h"
#include <synth_karplusstrong.h>
#include <output_i2s.h>
#include "Synthesizer.h"

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

Synthesizer synth;

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

unsigned long testText();

#define TFT_DC 9
#define TFT_CS 10
#define TFT_RST 255
#define TFT_MOSI 11
#define TFT_SCLK 13
#define TFT_MISO 12

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
// Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
ILI9341_t3 tft = ILI9341_t3(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);
// If using the breakout, change pins as desired

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
    AudioMemory(15);

    tft.begin();
}


void loop()
{
    static uint8_t rotation = 0;
    static unsigned long lastUpdate = 0;
    unsigned long currentTime = millis();
    
    myusb.Task();
    midi1.read();
    
    // Run rotation and display updates at most once per second
    if (currentTime - lastUpdate >= 1000) {
        tft.setRotation(rotation);
        rotation = (rotation + 1) % 4;
        testText();
        Serial.println("Loop");
        lastUpdate = currentTime;
    }
}

unsigned long testText()
{
    tft.fillScreen(ILI9341_BLACK);
    unsigned long start = micros();
    tft.setCursor(0, 0);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(1);
    tft.println("Hello World!");
    tft.setTextColor(ILI9341_YELLOW);
    tft.setTextSize(2);
    tft.println(1234.56);
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(3);
    tft.println(0xDEADBEEF, HEX);
    tft.println();
    tft.setTextColor(ILI9341_GREEN);
    tft.setTextSize(5);
    tft.println("Groop");
    tft.setTextSize(2);
    tft.println("I implore thee,");
    tft.setTextSize(1);
    tft.println("my foonting turlingdromes.");
    tft.println("And hooptiously drangle me");
    tft.println("with crinkly bindlewurdles,");
    tft.println("Or I will rend thee");
    tft.println("in the gobberwarts");
    tft.println("with my blurglecruncheon,");
    tft.println("see if I don't!");
    return micros() - start;
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
        synth.setVolume((float)value / 127.0f);
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

