#pragma once
#include <output_i2s.h>
#include <Audio.h>
#include <map>
#include "karplus_strong_string_synth.h"
#include "SF2PlayerAdapter.h"

class HybridSynthesizer {
public:
    enum SynthMode {
        STRINGS_ONLY,
        SF2_ONLY,
        LAYERED,      // Both playing together
        SPLIT         // Keyboard split (low=strings, high=SF2)
    };

private:
    // Karplus-Strong string synthesis
    KarplusStrongStringSynth strings[8];
    
    // SF2 player
    SF2PlayerAdapter sf2Player;
    
    // Audio mixing and output
    AudioMixer4 mixerL1, mixerL2, mixerL3;  // Added mixerL3 for SF2
    AudioMixer4 mixerR1, mixerR2, mixerR3;  // Added mixerR3 for SF2
    AudioMixer4 sumL, sumR;
    AudioOutputI2S i2s1;
    
    // Audio connections
    AudioConnection* stringPatchCords[16]; // 8 for L, 8 for R (strings)
    AudioConnection* sf2PatchCordL;  // SF2 to mixer
    AudioConnection* sf2PatchCordR;  // SF2 to mixer
    AudioConnection* patchCordSumL1, *patchCordSumL2, *patchCordSumL3;
    AudioConnection* patchCordSumR1, *patchCordSumR2, *patchCordSumR3;
    AudioConnection* patchCordL;
    AudioConnection* patchCordR;

    std::map<int, int> keyToString; // key -> string index
    std::map<int, float> keyToBaseFreq; // key -> base frequency (without bend)
    float pitchBendFactor = 1.0f; // Current pitch bend multiplier
    float masterVolume = 1.0f;
    float stringVolume = 1.0f;
    float sf2Volume = 1.0f;
    
    SynthMode currentMode = STRINGS_ONLY;
    uint8_t splitPoint = 60; // Middle C

public:
    HybridSynthesizer() :
        sf2PatchCordL(nullptr),
        sf2PatchCordR(nullptr),
        patchCordSumL1(nullptr), patchCordSumL2(nullptr), patchCordSumL3(nullptr),
        patchCordSumR1(nullptr), patchCordSumR2(nullptr), patchCordSumR3(nullptr),
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
        
        // Connect SF2 player to mixerL3 and mixerR3
        sf2PatchCordL = new AudioConnection(sf2Player, 0, mixerL3, 0);
        sf2PatchCordR = new AudioConnection(sf2Player, 1, mixerR3, 0);
        
        // Sum all mixers
        patchCordSumL1 = new AudioConnection(mixerL1, 0, sumL, 0);
        patchCordSumL2 = new AudioConnection(mixerL2, 0, sumL, 1);
        patchCordSumL3 = new AudioConnection(mixerL3, 0, sumL, 2);
        
        patchCordSumR1 = new AudioConnection(mixerR1, 0, sumR, 0);
        patchCordSumR2 = new AudioConnection(mixerR2, 0, sumR, 1);
        patchCordSumR3 = new AudioConnection(mixerR3, 0, sumR, 2);
        
        // Set initial mixer gains
        updateMixerGains();
        
        // Final output to I2S
        patchCordL = new AudioConnection(sumL, 0, i2s1, 0);
        patchCordR = new AudioConnection(sumR, 0, i2s1, 1);
    }

    ~HybridSynthesizer() {
        for (int i = 0; i < 16; ++i) delete stringPatchCords[i];
        delete sf2PatchCordL;
        delete sf2PatchCordR;
        delete patchCordSumL1;
        delete patchCordSumL2;
        delete patchCordSumL3;
        delete patchCordSumR1;
        delete patchCordSumR2;
        delete patchCordSumR3;
        delete patchCordL;
        delete patchCordR;
    }
    
    // SF2 management
    bool loadSF2(const char* filename) {
        return sf2Player.loadSF2(filename);
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
    
    void setSF2Volume(float volume) {
        sf2Volume = volume;
        updateMixerGains();
    }

    void noteOn(int key, float freq, float velocity) {
        bool playStrings = false;
        bool playSF2 = false;
        
        switch (currentMode) {
            case STRINGS_ONLY:
                playStrings = true;
                break;
            case SF2_ONLY:
                playSF2 = true;
                break;
            case LAYERED:
                playStrings = true;
                playSF2 = true;
                break;
            case SPLIT:
                if (key < splitPoint) {
                    playStrings = true;
                } else {
                    playSF2 = true;
                }
                break;
        }
        
        if (playStrings) {
            playStringNote(key, freq, velocity);
        }
        
        if (playSF2) {
            sf2Player.noteOn(0, key, (uint8_t)(velocity * 127.0f));
        }
    }

    void noteOff(int key) {
        // Always try to stop both - they'll ignore if not playing
        auto it = keyToString.find(key);
        if (it != keyToString.end()) {
            int stringIndex = it->second;
            strings[stringIndex].noteOff();
            keyToString.erase(it);
            keyToBaseFreq.erase(key);
        }
        
        sf2Player.noteOff(0, key);
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
        
        // Apply bend to SF2 player
        sf2Player.setPitchBend(0, bendAmount);
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
    SF2PlayerAdapter& getSF2Player() { return sf2Player; }
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
        float sf2Gain = 0.0f;
        
        switch (currentMode) {
            case STRINGS_ONLY:
                stringGain = masterVolume * stringVolume;
                sf2Gain = 0.0f;
                break;
            case SF2_ONLY:
                stringGain = 0.0f;
                sf2Gain = masterVolume * sf2Volume;
                break;
            case LAYERED:
                stringGain = masterVolume * stringVolume * 0.7f; // Reduce to avoid clipping
                sf2Gain = masterVolume * sf2Volume * 0.7f;
                break;
            case SPLIT:
                stringGain = masterVolume * stringVolume;
                sf2Gain = masterVolume * sf2Volume;
                break;
        }
        
        // Apply string gains
        for (int i = 0; i < 4; ++i) {
            mixerL1.gain(i, stringGain);
            mixerR1.gain(i, stringGain);
            mixerL2.gain(i, stringGain);
            mixerR2.gain(i, stringGain);
        }
        
        // Apply SF2 gains
        mixerL3.gain(0, sf2Gain);
        mixerR3.gain(0, sf2Gain);
        
        // Sum mixer gains
        sumL.gain(0, 1.0f); // mixerL1
        sumL.gain(1, 1.0f); // mixerL2
        sumL.gain(2, 1.0f); // mixerL3
        
        sumR.gain(0, 1.0f); // mixerR1
        sumR.gain(1, 1.0f); // mixerR2
        sumR.gain(2, 1.0f); // mixerR3
        
        // Note: SF2PlayerAdapter doesn't have a setVolume method like the old soundfont player
        // Volume control is handled through the mixer gains above
    }
};
