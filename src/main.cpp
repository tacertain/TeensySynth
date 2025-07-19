#include "USBHost_t36.h"
#include <synth_karplusstrong.h>
#include <output_i2s.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <ILI9341_t4.h>
#include <SPI.h>
#include "HybridSynthesizer.h"
#include "FrameBufferGFX.h"

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
#define TFT_TIRQ 6
#define TFT_TCS 8
#define TFT_DC 9
#define TFT_CS 10
#define TFT_MOSI 11
#define TFT_MISO 12
#define TFT_SCK 13
#define TFT_RST 255


// Global TFT object
ILI9341_T4::ILI9341Driver tft(TFT_CS, TFT_DC, TFT_SCK, TFT_MOSI, TFT_MISO, TFT_RST, TFT_TCS, TFT_TIRQ);

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

uint32_t count = 0;
uint16_t fb[240 * 320];
DMAMEM uint16_t fb_internal[240*320];
ILI9341_T4::DiffBuffStatic<4096> diff1; // a first diff buffer with 4K memory (statically allocated)
ILI9341_T4::DiffBuffStatic<4096> diff2;
FramebufferGFX gfx(fb, 240, 320);

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
    tft.setRotation(0); // Landscape mode (320x240)
    tft.setFramebuffer(fb_internal); // registers the internal framebuffer
    tft.setDiffBuffers(&diff1, &diff2); // registering the 2 diff buffers. This activates differential update mode
    tft.setRefreshRate(10);            // set the display refresh rate around 120Hz
    tft.setVSyncSpacing(2);            // enable vsync and set framerate = refreshrate/2 (typical choice)

    gfx.fillScreen(BLACK);
    gfx.setTextColor(WHITE);
    gfx.setCursor(10, 10);
    gfx.print("Hello from GFX!");
    Serial.println("TFT display initialized");
    
    // Set the first string synthesizer to use the TFT display
    synth.getString(0).setTFTDisplay(&gfx);
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
    static uint32_t last = micros();
    uint32_t now = micros();
#if 0
    static uint64_t i = 0;
    if (++i % 1000000 == 0) {
        Serial.print("Alive ");
        Serial.print(i / 1000000);
        Serial.print(" ");
        Serial.println(count);
    }
#endif
    myusb.Task();
    midi1.read();
    //tft.overlayFPS(fb); // optional: draw the current FPS on the top right corner of the framebuffer
    if (gfx.updated()) {
        Serial.println("Start frame update");
        tft.update(fb);
        last = micros();
        Serial.printf("Frame update took %dus\n", last - now);
        gfx.clearUpdate();
    }
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

