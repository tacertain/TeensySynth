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
     if (channel == 1 && control == 55 && value == 127) // CC 55 - String Pads
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        Serial.println("Mode: STRING_PADS (CC 55)");
    }/MOSI: 11
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

HybridSynthesizer synth; // Use the new hybrid synthesizer

USBHost myusb;
MIDIDevice midi1(myusb);

// Function prototypes
void OnNoteOn(byte channel, byte note, byte velocity);
void OnNoteOff(byte channel, byte note, byte velocity);
void OnControlChange(byte channel, byte control, byte value);
void OnPitchChange(byte channel, int bend);

uint32_t count = 0;
uint16_t fb[240 * 320];
DMAMEM uint16_t fb_internal[240 * 320];
ILI9341_T4::DiffBuffStatic<4096> diff1; // a first diff buffer with 4K memory (statically allocated)
ILI9341_T4::DiffBuffStatic<4096> diff2;
FramebufferGFX gfx(fb, 320, 240);
#define SPI_SPEED 40000000

void setup()
{
    // while (!Serial);
    myusb.begin();
    midi1.setHandleNoteOff(OnNoteOff);
    midi1.setHandleNoteOn(OnNoteOn);
    midi1.setHandleControlChange(OnControlChange);
    midi1.setHandlePitchChange(OnPitchChange);
    AudioMemory(20); // Memory for audio processing

    Serial.println("Hello, world!");
    Serial8.begin(400000, SERIAL_8N1);

    // Initialize TFT Display
    Serial.println("Initializing TFT display...");
    if (!tft.begin(SPI_SPEED))
        Serial.println("failed");
    tft.setRotation(3);
    tft.setFramebuffer(fb_internal);
    tft.setDiffBuffers(&diff1, &diff2);
    tft.setRefreshRate(60);
    tft.setVSyncSpacing(2);

    gfx.fillScreen(BLACK);
    gfx.setTextColor(WHITE);
    gfx.setCursor(10, 10);
    gfx.print("Hello from GFX!");
    Serial.println("TFT display initialized");

    // Set the first string synthesizer to use the TFT display
    synth.getString(0).setTFTDisplay(&gfx);
    Serial.println("TFT display assigned to string synthesizer 0");

    Serial.println("Setup complete - ready for input");
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
    
    // Update synthesizer envelopes and other processing
    synth.update();
    
    // tft.overlayFPS(fb); // optional: draw the current FPS on the top right corner of the framebuffer
    if (gfx.updated())
    {
        Serial.println("Start frame update");
        tft.update(fb);
        last = micros();
        Serial.printf("Frame update took %dus\n", last - now);
        gfx.clearUpdate();
    }
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
    float vel = (float)velocity / 128.0f;
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

    if (channel == 1 && control == 21)
    {
        synth.setAttenuation(value);
    }
    if (channel == 1 && control == 22)
    {
        synth.setFilterStrength(value);
    }

    if (channel == 1 && control == 7)
    {
        synth.setMasterVolume((float)value / 127.0f); // Updated method name
    }

    // Drone-specific controls
    if (channel == 1 && control == 23) // CC 23 - Drone Volume
    {
        synth.setDroneVolume((float)value / 127.0f);
        Serial.printf("Drone volume: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 24) // CC 24 - Filter Cutoff
    {
        synth.getDrone().setFilterCutoff((float)value / 127.0f);
        Serial.printf("Filter cutoff: %.2f\n", (float)value / 127.0f);
    }

    if (channel == 1 && control == 25) // CC 25 - LFO Rate
    {
        float lfoRate = 0.1f + ((float)value / 127.0f) * 9.9f; // 0.1 to 10 Hz
        synth.getDrone().setLFORate(lfoRate);
        Serial.printf("LFO rate: %.2f Hz\n", lfoRate);
    }

    if (channel == 1 && control == 26) // CC 26 - Oscillator Detune
    {
        float detune = ((float)value / 127.0f - 0.5f) * 2.0f; // -1.0 to +1.0
        synth.getDrone().setOscillatorDetune(detune);
        Serial.printf("Oscillator detune: %.3f semitones\n", detune);
    }

    if (channel == 1 && control == 27) // CC 27 - LFO Depth
    {
        synth.getDrone().setLFODepth((float)value / 127.0f);
        Serial.printf("LFO depth: %.2f\n", (float)value / 127.0f);
    }

    // String pad-specific controls
    if (channel == 1 && control == 41) // CC 41 - String Pad Volume
    {
        synth.setStringPadVolume((float)value / 127.0f);
        Serial.printf("String pad volume: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 42) // CC 42 - String Pad Filter Cutoff
    {
        synth.getStringPad().setFilterCutoff((float)value / 127.0f);
        Serial.printf("String pad filter cutoff: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 43) // CC 43 - String Pad Filter Resonance
    {
        synth.getStringPad().setFilterResonance((float)value / 127.0f);
        Serial.printf("String pad filter resonance: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 44) // CC 44 - String Pad Detune Amount
    {
        synth.getStringPad().setDetuneAmount((float)value / 127.0f);
        Serial.printf("String pad detune amount: %.2f\n", (float)value / 127.0f);
    }

    if (channel == 1 && control == 51 && value == 127) // CC 51 - Plucked Strings
    {
        synth.setSynthMode(HybridSynthesizer::PLUCKED_STRINGS);
        Serial.println("Mode: PLUCKED_STRINGS (CC 51)");
    }
    
    if (channel == 1 && control == 54 && value == 127) // CC 54 - Drone
    {
        synth.setSynthMode(HybridSynthesizer::DRONE);
        Serial.println("Mode: DRONE (CC 54)");
    }
    
    if (channel == 1 && control == 55 && value == 127) // CC 55 - String Pads
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        Serial.println("Mode: STRING_PADS (CC 47)");
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
