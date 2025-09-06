#pragma once
#include <output_i2s.h>
#include <Audio.h>
#include <map>
#include "karplus_strong_string_synth.h"
#include "DroneSynthesizer.h"
#include "StringPadSynthesizer.h"

class HybridSynthesizer {
public:
    enum SynthMode {
        PLUCKED_STRINGS,    // Karplus-Strong plucked strings (renamed from STRINGS_ONLY)
        DRONE,              // Analog-style drone synthesizer  
        STRING_PADS,        // Classic 80s string pads
        SPLIT               // Split mode: drone below split point, string pads above
    };

private:
    // Karplus-Strong string synthesis
    KarplusStrongStringSynth strings[8];
    
    // Drone synthesizer
    DroneSynthesizer drone;
    
    // String pad synthesizer
    StringPadSynthesizer stringPad;
    
    // Audio mixing and output
    AudioMixer4 mixerL1, mixerL2;  // String mixers
    AudioMixer4 mixerR1, mixerR2;  // String mixers
    AudioMixer4 mixerL4, mixerR4;  // Drone mixers
    AudioMixer4 mixerL5, mixerR5;  // String pad mixers
    AudioMixer4 sumL, sumR;
    AudioOutputI2S i2s1;
    
    // Audio connections
    AudioConnection* stringPatchCords[16]; // 8 for L, 8 for R (strings)
    AudioConnection* dronePatchCordL;      // Drone to mixer
    AudioConnection* dronePatchCordR;      // Drone to mixer
    AudioConnection* stringPadPatchCordL;  // String pad to mixer
    AudioConnection* stringPadPatchCordR;  // String pad to mixer
    AudioConnection* patchCordSumL1, *patchCordSumL2, *patchCordSumL4, *patchCordSumL5;
    AudioConnection* patchCordSumR1, *patchCordSumR2, *patchCordSumR4, *patchCordSumR5;
    AudioConnection* patchCordL;
    AudioConnection* patchCordR;

    std::map<int, int> keyToString; // key -> string index
    std::map<int, float> keyToBaseFreq; // key -> base frequency (without bend)
    float pitchBendFactor = 1.0f; // Current pitch bend multiplier
    float masterVolume = 1.0f;
    float stringVolume = 1.0f;
    float droneVolume = 1.0f;
    float stringPadVolume = 1.0f;
    
    SynthMode currentMode = SPLIT; 
    uint8_t splitPoint = 48; // C3 

public:
    HybridSynthesizer() :
        dronePatchCordL(nullptr),
        dronePatchCordR(nullptr),
        stringPadPatchCordL(nullptr),
        stringPadPatchCordR(nullptr),
        patchCordSumL1(nullptr), patchCordSumL2(nullptr), patchCordSumL4(nullptr), patchCordSumL5(nullptr),
        patchCordSumR1(nullptr), patchCordSumR2(nullptr), patchCordSumR4(nullptr), patchCordSumR5(nullptr),
        patchCordL(nullptr),
        patchCordR(nullptr)
    {
        // Connect strings 0-3 to mixerL1 and mixerR1
        for (int i = 0; i < 4; ++i) {
            stringPatchCords[i]     = new AudioConnection(strings[i], 0, mixerL1, i);
            stringPatchCords[i + 8] = new AudioConnection(strings[i], 0, mixerR1, i);
        }
        
        // Connect strings 4-7 to mixerL2 and mixerR2
        for (int i = 4; i < 8; ++i) {
            stringPatchCords[i]     = new AudioConnection(strings[i], 0, mixerL2, i - 4);
            stringPatchCords[i + 8] = new AudioConnection(strings[i], 0, mixerR2, i - 4);
        }
        
        // Connect drone to mixerL4 and mixerR4
        dronePatchCordL = new AudioConnection(*drone.getLeftOutput(), 0, mixerL4, 0);
        dronePatchCordR = new AudioConnection(*drone.getRightOutput(), 0, mixerR4, 0);
        
        // Connect string pad to mixerL5 and mixerR5 (mono to both channels)
        stringPadPatchCordL = new AudioConnection(*stringPad.getOutput(), 0, mixerL5, 0);
        stringPadPatchCordR = new AudioConnection(*stringPad.getOutput(), 0, mixerR5, 0);
        
        // Sum all mixers
        patchCordSumL1 = new AudioConnection(mixerL1, 0, sumL, 0);
        patchCordSumL2 = new AudioConnection(mixerL2, 0, sumL, 1);
        patchCordSumL4 = new AudioConnection(mixerL4, 0, sumL, 2);
        patchCordSumL5 = new AudioConnection(mixerL5, 0, sumL, 3);
        
        patchCordSumR1 = new AudioConnection(mixerR1, 0, sumR, 0);
        patchCordSumR2 = new AudioConnection(mixerR2, 0, sumR, 1);
        patchCordSumR4 = new AudioConnection(mixerR4, 0, sumR, 2);
        patchCordSumR5 = new AudioConnection(mixerR5, 0, sumR, 3);
        
        // Set initial mixer gains
        updateMixerGains();
        
        // Final output to I2S
        patchCordL = new AudioConnection(sumL, 0, i2s1, 0);
        patchCordR = new AudioConnection(sumR, 0, i2s1, 1);
    }

    ~HybridSynthesizer() {
        for (int i = 0; i < 16; ++i) delete stringPatchCords[i];
        delete dronePatchCordL;
        delete dronePatchCordR;
        delete stringPadPatchCordL;
        delete stringPadPatchCordR;
        delete patchCordSumL1;
        delete patchCordSumL2;
        delete patchCordSumL4;
        delete patchCordSumL5;
        delete patchCordSumR1;
        delete patchCordSumR2;
        delete patchCordSumR4;
        delete patchCordSumR5;
        delete patchCordL;
        delete patchCordR;
    }
    
    // Synthesis mode control
    void setSynthMode(SynthMode mode) { 
        currentMode = mode; 
        updateMixerGains();
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
        droneVolume = volume;
        updateMixerGains();
    }
    
    void setStringPadVolume(float volume) {
        stringPadVolume = volume;
        updateMixerGains();
    }
    
    // Synthesizer access
    DroneSynthesizer& getDrone() { return drone; }
    StringPadSynthesizer& getStringPad() { return stringPad; }

    // Processing - call this regularly from main loop
    void update() {
        drone.processEnvelopes();
        stringPad.processEnvelope();
    }

    void noteOn(int key, float freq, float velocity) {        
        switch (currentMode) {
            case PLUCKED_STRINGS:
                playStringNote(key, freq, velocity);
                break;
            case DRONE:
                // Convert key to MIDI note and use polyphonic interface
                drone.noteOn(key, velocity);
                break;
            case STRING_PADS:
                // Pass the current chord mode directly
                stringPad.noteOn(key, velocity, stringPad.getChordMode());
                break;
            case SPLIT:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint) {
                    drone.noteOn(key, velocity);
                } else {
                    // Use chord mode for specific keys in split mode
                    StringPadSynthesizer::ChordMode chordMode = StringPadSynthesizer::CHORD_MODE_OFF;
                    if (key >= 53 && key <= 57) {
                        chordMode = StringPadSynthesizer::CHORD_MODE_MAJOR;
                    }
                    else if (key == 60)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_OCTAVE;
                    }
                    key += 12;
                    stringPad.noteOn(key, velocity, chordMode);
                }
                break;
        }
    }

    void noteOff(int key) {
        switch (currentMode) {
            case PLUCKED_STRINGS:
                // Stop string if it's playing this key
                {
                    auto it = keyToString.find(key);
                    if (it != keyToString.end()) {
                        int stringIndex = it->second;
                        strings[stringIndex].noteOff();
                        keyToString.erase(it);
                        keyToBaseFreq.erase(key);
                    }
                }
                break;
            case DRONE:
                drone.noteOff(key);
                break;
            case STRING_PADS:
                // Pass the current chord mode directly
                stringPad.noteOff(key, stringPad.getChordMode());
                break;
            case SPLIT:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint) {
                    drone.noteOff(key);
                } else {
                    // Use chord mode for specific keys in split mode
                    StringPadSynthesizer::ChordMode chordMode = StringPadSynthesizer::CHORD_MODE_OFF;
                    if (key >= 53 && key <= 57) {
                        chordMode = StringPadSynthesizer::CHORD_MODE_MAJOR;
                    }
                    else if (key == 60) {
                        chordMode = StringPadSynthesizer::CHORD_MODE_OCTAVE;
                    }
                    key += 12;
                    stringPad.noteOff(key, chordMode);
                }
                break;
        }
    }

    void setPitchBend(float bendAmount) {
        // bendAmount is in semitones
        pitchBendFactor = pow(2.0f, bendAmount / 12.0f);
        
        // Apply bend to all active strings
        for (const auto& pair : keyToString) {
            int key = pair.first;
            int stringIndex = pair.second;
            float baseFreq = keyToBaseFreq[key];
            strings[stringIndex].updateFrequency(baseFreq * pitchBendFactor);
        }
    }

    // String-specific controls (for backwards compatibility)
    void setAttenuation(uint16_t newAttenuation) {
        for (int i = 0; i < 8; ++i) {
            strings[i].setAttenuation(newAttenuation);
        }
    }

    void setFilterStrength(uint16_t newStrength) {
        for (int i = 0; i < 8; ++i) {
            strings[i].setFilterStrength(newStrength);
        }
    }
    
    // Direct access to components
    KarplusStrongStringSynth& getString(int index) { 
        if (index >= 0 && index < 8) return strings[index];
        return strings[0]; // Return first string as fallback
    }

private:
    void playStringNote(int key, float freq, float velocity) {
        // Find a free string (not in keyToString)
        for (int i = 0; i < 8; ++i) {
            bool inUse = false;
            for (const auto& pair : keyToString) {
                if (pair.second == i) {
                    inUse = true;
                    break;
                }
            }
            if (!inUse) {
                if(strings[i].noteOn(freq, velocity) == 0) {
                    keyToString[key] = i;
                    keyToBaseFreq[key] = freq;
                }
                return;
            }
        }
        // No available string, could implement voice stealing here
    }
    
    void updateMixerGains() {
        float stringGain = 0.0f;
        float droneGain = 0.0f;
        float stringPadGain = 0.0f;
        
        switch (currentMode) {
            case PLUCKED_STRINGS:
                stringGain = masterVolume * stringVolume;
                droneGain = 0.0f;
                stringPadGain = 0.0f;
                break;
            case DRONE:
                stringGain = 0.0f;
                droneGain = masterVolume * droneVolume;
                stringPadGain = 0.0f;
                break;
            case STRING_PADS:
                stringGain = 0.0f;
                droneGain = 0.0f;
                stringPadGain = masterVolume * stringPadVolume;
                break;
            case SPLIT:
                stringGain = 0.0f; // Plucked strings not used in split mode
                droneGain = masterVolume * droneVolume;
                stringPadGain = masterVolume * stringPadVolume;
                break;
        }
        
        // Apply string gains
        for (int i = 0; i < 4; ++i) {
            mixerL1.gain(i, stringGain);
            mixerR1.gain(i, stringGain);
            mixerL2.gain(i, stringGain);
            mixerR2.gain(i, stringGain);
        }
        
        // Apply drone gains
        mixerL4.gain(0, droneGain);
        mixerR4.gain(0, droneGain);
        
        // Apply string pad gains
        mixerL5.gain(0, stringPadGain);
        mixerR5.gain(0, stringPadGain);
        
        // Sum mixer gains
        sumL.gain(0, 1.0f); // mixerL1
        sumL.gain(1, 1.0f); // mixerL2
        sumL.gain(2, 1.0f); // mixerL4
        sumL.gain(3, 1.0f); // mixerL5
        
        sumR.gain(0, 1.0f); // mixerR1
        sumR.gain(1, 1.0f); // mixerR2
        sumR.gain(2, 1.0f); // mixerR4
        sumR.gain(3, 1.0f); // mixerR5
    }
};