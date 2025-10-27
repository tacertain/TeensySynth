#pragma once

// Standard library
#include <map>

// Third-party libraries
#include <Audio.h>
#include <output_i2s.h>

// Project headers
#include "AudioPeakMonitor.h"
#include "DroneSynthesizer.h"
#include "StringsSynthesizer.h"
#include "StringPadSynthesizer.h"
#include "SoundfontSynthesizer.h"

class HybridSynthesizer {
public:
    enum SynthMode {
        PLUCKED_STRINGS,    // Karplus-Strong plucked strings (renamed from STRINGS_ONLY)
        DRONE,              // Analog-style drone synthesizer  
        STRING_PADS,        // Classic 80s string pads
        SOUNDFONT,          // SoundFont (.sf2) synthesizer
        SPLIT,              // Split mode: drone below split point, string pads above
        IRAN                // Special mode for I Ran
    };

private:
    // Karplus-Strong string synthesis
    StringsSynthesizer strings;
    
    // Drone synthesizer
    DroneSynthesizer drone;
    
    // String pad synthesizer
    StringPadSynthesizer stringPad;
    
    // Soundfont synthesizer
    SoundfontSynthesizer soundfont;
    
    // Audio mixing and output
    AudioMixer4 mixerL4, mixerR4;  // Drone mixers
    AudioMixer4 mixerL5, mixerR5;  // String pad mixers
    AudioMixer4 mixerL6, mixerR6;  // Soundfont mixers
    AudioMixer4 sumL, sumR;
    AudioPeakMonitor peakMonitorL, peakMonitorR;  // Peak monitors for left and right channels
    AudioOutputI2S i2s1;
    
    // Audio connections
    AudioConnection* stringPatchCordL;     // Strings to mixer L
    AudioConnection* stringPatchCordR;     // Strings to mixer R
    AudioConnection* dronePatchCordL;      // Drone to mixer
    AudioConnection* dronePatchCordR;      // Drone to mixer
    AudioConnection* stringPadPatchCordL;  // String pad to mixer
    AudioConnection* stringPadPatchCordR;  // String pad to mixer
    AudioConnection* soundfontPatchCordL;  // Soundfont to mixer
    AudioConnection* soundfontPatchCordR;  // Soundfont to mixer
    AudioConnection* patchCordSumL1, *patchCordSumL4, *patchCordSumL5, *patchCordSumL6;
    AudioConnection* patchCordSumR1, *patchCordSumR4, *patchCordSumR5, *patchCordSumR6;
    AudioConnection* patchCordMonitorL, *patchCordMonitorR;  // Connections to peak monitors
    AudioConnection* patchCordL;
    AudioConnection* patchCordR;

    std::map<int, int> keyToString; // key -> string index (deprecated - managed internally now)
    std::map<int, float> keyToBaseFreq; // key -> base frequency (deprecated - managed internally now)
    float pitchBendFactor = 1.0f; // Current pitch bend multiplier (deprecated - managed internally now)
    float masterVolume = 1.0f;
    float stringVolume = 1.0f;
    float droneVolume = 1.0f;
    float stringPadVolume = 1.0f;
    float soundfontVolume = 1.0f;
    
    SynthMode currentMode = IRAN; 
    uint8_t splitPoint = 48; // C3 

public:
    HybridSynthesizer() :
        stringPatchCordL(nullptr),
        stringPatchCordR(nullptr),
        dronePatchCordL(nullptr),
        dronePatchCordR(nullptr),
        stringPadPatchCordL(nullptr),
        stringPadPatchCordR(nullptr),
        soundfontPatchCordL(nullptr),
        soundfontPatchCordR(nullptr),
        patchCordSumL1(nullptr), patchCordSumL4(nullptr), patchCordSumL5(nullptr), patchCordSumL6(nullptr),
        patchCordSumR1(nullptr), patchCordSumR4(nullptr), patchCordSumR5(nullptr), patchCordSumR6(nullptr),
        patchCordMonitorL(nullptr), patchCordMonitorR(nullptr),
        patchCordL(nullptr),
        patchCordR(nullptr)
    {
        // Connect strings to sum mixers
        stringPatchCordL = new AudioConnection(*strings.getLeftOutput(), 0, sumL, 0);
        stringPatchCordR = new AudioConnection(*strings.getRightOutput(), 0, sumR, 0);
        
        // Connect drone to mixerL4 and mixerR4
        dronePatchCordL = new AudioConnection(*drone.getLeftOutput(), 0, mixerL4, 0);
        dronePatchCordR = new AudioConnection(*drone.getRightOutput(), 0, mixerR4, 0);
        
        // Connect string pad to mixerL5 and mixerR5 (mono to both channels)
        stringPadPatchCordL = new AudioConnection(*stringPad.getOutput(), 0, mixerL5, 0);
        stringPadPatchCordR = new AudioConnection(*stringPad.getOutput(), 0, mixerR5, 0);
        
        // Connect soundfont to mixerL6 and mixerR6 (mono to both channels)
        soundfontPatchCordL = new AudioConnection(*soundfont.getLeftOutput(), 0, mixerL6, 0);
        soundfontPatchCordR = new AudioConnection(*soundfont.getRightOutput(), 0, mixerR6, 0);
        
        // Sum all mixers
        patchCordSumL1 = new AudioConnection(mixerL4, 0, sumL, 1);
        patchCordSumL4 = new AudioConnection(mixerL5, 0, sumL, 2);
        patchCordSumL6 = new AudioConnection(mixerL6, 0, sumL, 3);
        
        patchCordSumR1 = new AudioConnection(mixerR4, 0, sumR, 1);
        patchCordSumR4 = new AudioConnection(mixerR5, 0, sumR, 2);
        patchCordSumR6 = new AudioConnection(mixerR6, 0, sumR, 3);
        
        // Set initial mixer gains
        updateMixerGains();
        
        // Connect sum outputs to peak monitors
        patchCordMonitorL = new AudioConnection(sumL, 0, peakMonitorL, 0);
        patchCordMonitorR = new AudioConnection(sumR, 0, peakMonitorR, 0);
        
        // Final output to I2S
        patchCordL = new AudioConnection(sumL, 0, i2s1, 0);
        patchCordR = new AudioConnection(sumR, 0, i2s1, 1);
    }

    ~HybridSynthesizer() {
        delete stringPatchCordL;
        delete stringPatchCordR;
        delete dronePatchCordL;
        delete dronePatchCordR;
        delete stringPadPatchCordL;
        delete stringPadPatchCordR;
        delete soundfontPatchCordL;
        delete soundfontPatchCordR;
        delete patchCordSumL1;
        delete patchCordSumL4;
        delete patchCordSumL5;
        delete patchCordSumL6;
        delete patchCordSumR1;
        delete patchCordSumR4;
        delete patchCordSumR5;
        delete patchCordSumR6;
        delete patchCordMonitorL;
        delete patchCordMonitorR;
        delete patchCordL;
        delete patchCordR;
    }
    
    // Synthesis mode control
    void setSynthMode(SynthMode mode) { 
        // Print current peak levels before switching modes
        const char* modeNames[] = {"PLUCKED_STRINGS", "DRONE", "STRING_PADS", "SPLIT"};
        const char* currentModeName = (currentMode >= 0 && currentMode < 4) ? modeNames[currentMode] : "UNKNOWN";
        const char* newModeName = (mode >= 0 && mode < 4) ? modeNames[mode] : "UNKNOWN";
        
        Serial.print("Switching from ");
        Serial.print(currentModeName);
        Serial.print(" to ");
        Serial.print(newModeName);
        Serial.print(" - ");
        printPeakLevels();
        
        currentMode = mode; 
        resetPeakMonitors(); // Reset peak monitors when mode changes
    }
    
    void setSplitPoint(uint8_t note) { splitPoint = note; }
    
    // Volume controls
    void setMasterVolume(float volume) {
        masterVolume = volume;
        updateMixerGains();
    }
    
    void setStringVolume(float volume) {
        stringVolume = volume;
        updateMixerGains();
    }
    
    void setDroneVolume(float volume) {
        drone.setVolume(volume);
    }
    
    void setStringPadVolume(float volume) {
        stringPadVolume = volume;
        updateMixerGains();
    }
    
    void setSoundfontVolume(float volume) {
        soundfontVolume = volume;
        updateMixerGains();
    }
    
    // Soundfont synthesizer access
    SoundfontSynthesizer& getSoundfont() { return soundfont; }
    bool initializeSoundfont() { return soundfont.begin(); }
    bool loadSoundfontInstrument(const char* filename, int instrumentIndex) {
        return soundfont.loadInstrument(filename, instrumentIndex);
    }
    
    // Synthesizer access
    DroneSynthesizer& getDrone() { return drone; }
    StringPadSynthesizer& getStringPad() { return stringPad; }

    // Processing - call this regularly from main loop
    void update() {
        drone.processEnvelopes();
        stringPad.processEnvelope();
    }

    HybridSynthesizer::SynthMode modeFromChannel(byte channel, HybridSynthesizer::SynthMode defaultMode)
    {
        auto mode = currentMode;
        switch (channel)
        {
        case 2:
            mode = PLUCKED_STRINGS;
            break;
        case 3:
            mode = DRONE;
            break;
        case 4:
            mode = STRING_PADS;
            break;
        case 5:
            mode = SOUNDFONT;
            break;
        case 6:
            mode = SPLIT;
            break;
        case 7:
            mode = IRAN;
            break;
        default:
            break;
        }
        return mode;
    }

    void noteOn(byte channel, int key, float freq, float velocity) {
        auto mode = modeFromChannel(channel, currentMode);

        switch (mode) {
            case PLUCKED_STRINGS:
                playStringNote(key, freq, velocity);
                break;
            case DRONE:
                // Convert key to MIDI note and use polyphonic interface
                drone.noteOn(key, velocity);
                break;
            case STRING_PADS:
                // Pass the current chord mode directly
                stringPad.noteOn(key, velocity, StringPadSynthesizer::CHORD_MODE_OFF);
                break;
            case SOUNDFONT:
                // Use soundfont synthesizer
                soundfont.noteOn(key, velocity);
                break;
            case SPLIT:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOn(key, velocity);
                }
                else
                {
                    stringPad.noteOn(key, velocity, StringPadSynthesizer::CHORD_MODE_OFF);
                }
                break;
            case IRAN:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOn(key, 1.0f);
                }
                else
                {
                    // Use chord mode for specific keys in split mode
                    StringPadSynthesizer::ChordMode chordMode = StringPadSynthesizer::CHORD_MODE_OFF;
                    if (key >= 53 && key <= 57)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_MAJOR;
                    }
                    else if (key == 60)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_OCTAVE;
                    }
                    key += 24;
                    stringPad.noteOn(key, 1.0f, chordMode);
                }
                break;
            }
    }

    void noteOff(byte channel, int key) {
        auto mode = modeFromChannel(channel, currentMode);

        switch (mode) {
            case PLUCKED_STRINGS:
                // Delegate to StringsSynthesizer
                strings.noteOff(key);
                break;
            case DRONE:
                drone.noteOff(key);
                break;
            case STRING_PADS:
                // Pass the current chord mode directly
                stringPad.noteOff(key, StringPadSynthesizer::CHORD_MODE_OFF);
                break;
            case SOUNDFONT:
                soundfont.noteOff(key);
                break;
            case SPLIT:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOff(key);
                }
                else
                {

                    stringPad.noteOff(key, StringPadSynthesizer::CHORD_MODE_OFF);
                }
                break;
            case IRAN:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOff(key);
                }
                else
                {
                    // Use chord mode for specific keys in split mode
                    StringPadSynthesizer::ChordMode chordMode = StringPadSynthesizer::CHORD_MODE_OFF;
                    if (key >= 53 && key <= 57)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_MAJOR;
                    }
                    else if (key == 60)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_OCTAVE;
                    }
                    key += 24;
                    stringPad.noteOff(key, chordMode);
                }
                break;
            }
    }

    void setPitchBend(float bendAmount) {
        // bendAmount is in semitones
        // Delegate to StringsSynthesizer
        strings.setPitchBend(bendAmount);
    }

    // String-specific controls (for backwards compatibility)
    void setAttenuation(uint16_t newAttenuation) {
        strings.setAttenuation(newAttenuation);
    }

    void setFilterStrength(uint16_t newStrength) {
        strings.setFilterStrength(newStrength);
    }
    
    // Direct access to components
    KarplusStrongStringSynth& getString(int index) { 
        return strings.getVoice(index);
    }
    
    // Peak monitoring methods
    int16_t getOutputMinL() const { return peakMonitorL.getMin(); }
    int16_t getOutputMaxL() const { return peakMonitorL.getMax(); }
    int16_t getOutputMinR() const { return peakMonitorR.getMin(); }
    int16_t getOutputMaxR() const { return peakMonitorR.getMax(); }
    
    void resetPeakMonitors() {
        peakMonitorL.reset();
        peakMonitorR.reset();
    }
    
    void printPeakLevels() const {
        Serial.print("Peak Levels - L: min=");
        Serial.print(peakMonitorL.getMin());
        Serial.print(", max=");
        Serial.print(peakMonitorL.getMax());
        Serial.print(" | R: min=");
        Serial.print(peakMonitorR.getMin());
        Serial.print(", max=");
        Serial.println(peakMonitorR.getMax());
    }

private:
    void playStringNote(int key, float freq, float velocity) {
        // Delegate to StringsSynthesizer (it handles voice allocation internally)
        // The freq parameter is ignored as StringsSynthesizer calculates it from MIDI note
        strings.noteOn(key, velocity);
    }
    
    void updateMixerGains() {
        float stringGain = masterVolume * stringVolume;
        float droneGain = masterVolume * droneVolume;
        float stringPadGain = masterVolume * stringPadVolume;
        float soundfontGain = masterVolume * soundfontVolume;

        // Apply string gains via StringsSynthesizer
        strings.setVolume(stringGain);
        
        // Apply drone gains
        mixerL4.gain(0, droneGain);
        mixerR4.gain(0, droneGain);
        
        // Apply string pad gains
        mixerL5.gain(0, stringPadGain);
        mixerR5.gain(0, stringPadGain);
        
        // Apply soundfont gains
        mixerL6.gain(0, soundfontGain);
        mixerR6.gain(0, soundfontGain);
        
        // Sum mixer gains (4 inputs now: strings, drone, string pads, soundfont)
        sumL.gain(0, 1.0f); // strings
        sumL.gain(1, 1.0f); // drone
        sumL.gain(2, 1.0f); // string pads
        sumL.gain(3, 1.0f); // soundfont
        
        sumR.gain(0, 1.0f); // strings
        sumR.gain(1, 1.0f); // drone
        sumR.gain(2, 1.0f); // string pads
        sumR.gain(3, 1.0f); // soundfont
    }
};