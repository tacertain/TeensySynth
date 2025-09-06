#pragma once
#include <output_i2s.h>
#include <Audio.h>
#include <map>
#include "karplus_strong_string_synth.h"
#include "DroneSynthesizer.h"

class HybridSynthesizer {
public:
    enum SynthMode {
        STRINGS_ONLY,
        LAYERED,      // Strings + Drone
        SPLIT,        // Keyboard split (low=strings, high=drone)
        DRONE         // Analog-style drone synthesizer
    };

private:
    // Karplus-Strong string synthesis
    KarplusStrongStringSynth strings[8];
    
    // Drone synthesizer
    DroneSynthesizer drone;
    
    // Audio mixing and output
    AudioMixer4 mixerL1, mixerL2;  // String mixers
    AudioMixer4 mixerR1, mixerR2;  // String mixers
    AudioMixer4 mixerL4, mixerR4;  // Drone mixers
    AudioMixer4 sumL, sumR;
    AudioOutputI2S i2s1;
    
    // Audio connections
    AudioConnection* stringPatchCords[16]; // 8 for L, 8 for R (strings)
    AudioConnection* dronePatchCordL;      // Drone to mixer
    AudioConnection* dronePatchCordR;      // Drone to mixer
    AudioConnection* patchCordSumL1, *patchCordSumL2, *patchCordSumL4;
    AudioConnection* patchCordSumR1, *patchCordSumR2, *patchCordSumR4;
    AudioConnection* patchCordL;
    AudioConnection* patchCordR;

    std::map<int, int> keyToString; // key -> string index
    std::map<int, float> keyToBaseFreq; // key -> base frequency (without bend)
    float pitchBendFactor = 1.0f; // Current pitch bend multiplier
    float masterVolume = 1.0f;
    float stringVolume = 1.0f;
    float droneVolume = 1.0f;
    
    SynthMode currentMode = STRINGS_ONLY;
    uint8_t splitPoint = 60; // Middle C

public:
    HybridSynthesizer() :
        dronePatchCordL(nullptr),
        dronePatchCordR(nullptr),
        patchCordSumL1(nullptr), patchCordSumL2(nullptr), patchCordSumL4(nullptr),
        patchCordSumR1(nullptr), patchCordSumR2(nullptr), patchCordSumR4(nullptr),
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
        
        // Sum all mixers
        patchCordSumL1 = new AudioConnection(mixerL1, 0, sumL, 0);
        patchCordSumL2 = new AudioConnection(mixerL2, 0, sumL, 1);
        patchCordSumL4 = new AudioConnection(mixerL4, 0, sumL, 2);
        
        patchCordSumR1 = new AudioConnection(mixerR1, 0, sumR, 0);
        patchCordSumR2 = new AudioConnection(mixerR2, 0, sumR, 1);
        patchCordSumR4 = new AudioConnection(mixerR4, 0, sumR, 2);
        
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
        delete patchCordSumL1;
        delete patchCordSumL2;
        delete patchCordSumL4;
        delete patchCordSumR1;
        delete patchCordSumR2;
        delete patchCordSumR4;
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
    
    // Drone-specific controls
    DroneSynthesizer& getDrone() { return drone; }

    void noteOn(int key, float freq, float velocity) {
        bool playStrings = false;
        bool playDrone = false;
        
        switch (currentMode) {
            case STRINGS_ONLY:
                playStrings = true;
                break;
            case LAYERED:
                playStrings = true;
                playDrone = true;
                break;
            case SPLIT:
                if (key < splitPoint) {
                    playStrings = true;
                } else {
                    playDrone = true;
                }
                break;
            case DRONE:
                playDrone = true;
                break;
        }
        
        if (playStrings) {
            playStringNote(key, freq, velocity);
        }
        
        if (playDrone) {
            drone.noteOn(freq, velocity);
        }
    }

    void noteOff(int key) {
        // Always try to stop strings - they'll ignore if not playing
        auto it = keyToString.find(key);
        if (it != keyToString.end()) {
            int stringIndex = it->second;
            strings[stringIndex].noteOff();
            keyToString.erase(it);
            keyToBaseFreq.erase(key);
        }
        
        // For drone mode, we use monophonic behavior (single voice)
        if (currentMode == DRONE || currentMode == LAYERED || currentMode == SPLIT) {
            drone.noteOff();
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
        
        switch (currentMode) {
            case STRINGS_ONLY:
                stringGain = masterVolume * stringVolume;
                droneGain = 0.0f;
                break;
            case LAYERED:
                stringGain = masterVolume * stringVolume * 0.7f; // Reduce to avoid clipping
                droneGain = masterVolume * droneVolume * 0.7f;
                break;
            case SPLIT:
                stringGain = masterVolume * stringVolume;
                droneGain = masterVolume * droneVolume;
                break;
            case DRONE:
                stringGain = 0.0f;
                droneGain = masterVolume * droneVolume;
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
        
        // Sum mixer gains
        sumL.gain(0, 1.0f); // mixerL1
        sumL.gain(1, 1.0f); // mixerL2
        sumL.gain(2, 1.0f); // mixerL4
        
        sumR.gain(0, 1.0f); // mixerR1
        sumR.gain(1, 1.0f); // mixerR2
        sumR.gain(2, 1.0f); // mixerR4
    }
};
