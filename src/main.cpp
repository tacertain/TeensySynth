#include "USBHost_t36.h"
#include <synth_karplusstrong.h>
#include <output_i2s.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <ILI9341_t4.h>
#include <SPI.h>
#include "HybridSynthesizer.h"
#include "FrameBufferGFX.h"

// Uncomment to disable TFT completely if it causes hanging
// #define DISABLE_TFT

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

HybridSynthesizer synth; // Use the new hybrid synthesizer

USBHost myusb;
MIDIDevice midi1(myusb);

// TFT initialization flag
bool tftAvailable = false;

// Function prototypes
void OnNoteOn(byte channel, byte note, byte velocity);
void OnNoteOff(byte channel, byte note, byte velocity);
void OnControlChange(byte channel, byte control, byte value);
void OnPitchChange(byte channel, int bend);
void queryUSBDeviceInfo();

uint32_t count = 0;
uint16_t fb[240 * 320];
DMAMEM uint16_t fb_internal[240 * 320];
ILI9341_T4::DiffBuffStatic<4096> diff1; // a first diff buffer with 4K memory (statically allocated)
ILI9341_T4::DiffBuffStatic<4096> diff2;
FramebufferGFX gfx(fb, 320, 240);
#define SPI_SPEED 40000000

void setup()
{
    //while (!Serial);
    myusb.begin();
    midi1.setHandleNoteOff(OnNoteOff);
    midi1.setHandleNoteOn(OnNoteOn);
    midi1.setHandleControlChange(OnControlChange);
    midi1.setHandlePitchChange(OnPitchChange);
    AudioMemory(20); // Memory for audio processing

    Serial.println("Hello, world!");
    Serial8.begin(400000, SERIAL_8N1);

    // Initialize TFT Display
#ifdef TFT_DISPLAY

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
    tftAvailable = true;
#else
    Serial.println("TFT disabled by compile flag - skipping initialization");
#endif

    if (!tftAvailable) {
        Serial.println("Running without TFT display - audio functionality will work normally");
    }

    queryUSBDeviceInfo();
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
    if (tftAvailable && gfx.updated())
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
    synth.noteOn(note, freq, 1.0f);
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

    // Mode Switching Controls
    if (channel == 1 && control == 51 && value == 127) // CC 51 - Plucked Strings
    {
        synth.setSynthMode(HybridSynthesizer::PLUCKED_STRINGS);
        Serial.println("Mode: PLUCKED_STRINGS (CC 51)");
    }
    
    if (channel == 1 && control == 52 && value == 127) // CC 52 - Drone
    {
        synth.setSynthMode(HybridSynthesizer::DRONE);
        Serial.println("Mode: DRONE (CC 52)");
    }
    
    // String Pad Mode with Presets (CC 53-57) - Switch to String Pads and load preset
    if (channel == 1 && control == 53 && value == 127) // CC 53 - String Pads: Lush Pads
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        synth.getStringPad().loadPreset(StringPadSynthesizer::PRESET_LUSH_PADS);
        Serial.println("Mode: STRING_PADS + Lush Pads preset (CC 53)");
    }
    
    if (channel == 1 && control == 54 && value == 127) // CC 54 - String Pads: Bright Strings
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        synth.getStringPad().loadPreset(StringPadSynthesizer::PRESET_BRIGHT_STRINGS);
        Serial.println("Mode: STRING_PADS + Bright Strings preset (CC 54)");
    }
    
    if (channel == 1 && control == 55 && value == 127) // CC 55 - String Pads: Soft Ensemble
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        synth.getStringPad().loadPreset(StringPadSynthesizer::PRESET_SOFT_ENSEMBLE);
        Serial.println("Mode: STRING_PADS + Soft Ensemble preset (CC 55)");
    }
    
    if (channel == 1 && control == 56 && value == 127) // CC 56 - String Pads: Analog Warmth
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        synth.getStringPad().loadPreset(StringPadSynthesizer::PRESET_ANALOG_WARMTH);
        Serial.println("Mode: STRING_PADS + Analog Warmth preset (CC 56)");
    }
    
    if (channel == 1 && control == 57 && value == 127) // CC 57 - String Pads: Shimmer
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        synth.getStringPad().loadPreset(StringPadSynthesizer::PRESET_SHIMMER);
        Serial.println("Mode: STRING_PADS + Shimmer preset (CC 57)");
    }
    
    if (channel == 1 && control == 58 && value == 127) // CC 58 - Split Mode
    {
        synth.setSynthMode(HybridSynthesizer::SPLIT);
        Serial.println("Mode: SPLIT - Drone below C5, String Pads above (CC 58)");
    }
    
    if (channel == 1 && control == 59 && value == 127) // CC 59 - Cycle String Pad Chord Mode
    {
        StringPadSynthesizer::ChordMode currentMode = synth.getStringPad().getChordMode();
        switch (currentMode) {
            case StringPadSynthesizer::CHORD_MODE_OFF:
                synth.getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_MAJOR);
                Serial.println("String Pad Chord Mode: MAJOR - Playing major chords (CC 59)");
                break;
            case StringPadSynthesizer::CHORD_MODE_MAJOR:
                synth.getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_OCTAVE);
                Serial.println("String Pad Chord Mode: OCTAVE - Playing octave notes (CC 59)");
                break;
            case StringPadSynthesizer::CHORD_MODE_OCTAVE:
                synth.getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_OFF);
                Serial.println("String Pad Chord Mode: OFF - Playing single notes (CC 59)");
                break;
        }
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

void queryUSBDeviceInfo() 
{
    Serial.println("=== USB DEVICE INFORMATION ===");
    
    // The MIDIDevice doesn't have isConnected(), so we'll check if we can get device info
    Serial.println("MIDI Device: Checking connection...");
    
    // Get USB device descriptor information
    uint16_t vid = midi1.idVendor();
    uint16_t pid = midi1.idProduct();
    
    if (vid == 0 && pid == 0) {
        Serial.println("No USB MIDI device detected or device not ready");
        Serial.println("==============================");
        return;
    }
    
    Serial.println("USB MIDI Device Detected: Yes");
    
    Serial.print("Vendor ID (VID): 0x");
    if (vid < 0x1000) Serial.print("0");
    if (vid < 0x100) Serial.print("0");
    if (vid < 0x10) Serial.print("0");
    Serial.print(vid, HEX);
    Serial.print(" (");
    Serial.print(vid);
    Serial.println(")");
    
    Serial.print("Product ID (PID): 0x");
    if (pid < 0x1000) Serial.print("0");
    if (pid < 0x100) Serial.print("0");
    if (pid < 0x10) Serial.print("0");
    Serial.print(pid, HEX);
    Serial.print(" (");
    Serial.print(pid);
    Serial.println(")");
    
    // Try to identify common manufacturers by VID
    Serial.print("Manufacturer: ");
    switch (vid) {
        case 0x0944: Serial.println("Korg"); break;
        case 0x0499: Serial.println("Yamaha"); break;
        case 0x0582: Serial.println("Roland"); break;
        case 0x0763: Serial.println("M-Audio"); break;
        case 0x09E8: Serial.println("AKAI Professional"); break;
        case 0x0A4E: Serial.println("Native Instruments"); break;
        case 0x1C75: Serial.println("Novation"); break;
        case 0x2011: Serial.println("MIDI Solutions"); break;
        case 0x133E: Serial.println("Access Music"); break;
        case 0x15CA: Serial.println("Textech Int'l"); break;
        case 0x0543: Serial.println("Zoom"); break;
        case 0x0644: Serial.println("TASCAM"); break;
        case 0x0A92: Serial.println("Presonus"); break;
        case 0x194F: Serial.println("PreSonus Audio Electronics"); break;
        case 0x0A46: Serial.println("Daisy Chain"); break;
        case 0x041E: Serial.println("Creative Technology"); break;
        case 0x1397: Serial.println("BEHRINGER"); break;
        case 0x09DA: Serial.println("A4Tech"); break;
        case 0x0955: Serial.println("NVIDIA"); break;
        case 0x17CC: Serial.println("Native Instruments (Alt)"); break;
        case 0x2573: Serial.println("Arturia"); break;
        default: 
            Serial.print("Unknown (VID: 0x");
            Serial.print(vid, HEX);
            Serial.println(")");
            break;
    }
    
    // Get manufacturer and product strings if available
    const uint8_t* mfgStr = midi1.manufacturer();
    const uint8_t* prodStr = midi1.product();
    const uint8_t* serialStr = midi1.serialNumber();
    
    if (mfgStr && mfgStr[0] != 0) {
        Serial.print("Manufacturer String: \"");
        Serial.print((char*)mfgStr);
        Serial.println("\"");
    }
    
    if (prodStr && prodStr[0] != 0) {
        Serial.print("Product String: \"");
        Serial.print((char*)prodStr);
        Serial.println("\"");
    }
    
    if (serialStr && serialStr[0] != 0) {
        Serial.print("Serial Number: \"");
        Serial.print((char*)serialStr);
        Serial.println("\"");
    }
    
    Serial.println("==============================");
}
