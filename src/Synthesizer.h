#pragma once

// Standard library
#include <map>

// Third-party libraries
#include <Audio.h>
#include <output_i2s.h>

// Project headers
#include "karplus_strong_string_synth.h"

class Synthesizer {

    KarplusStrongStringSynth strings[8];
    AudioMixer4 mixerL1, mixerL2;
    AudioMixer4 mixerR1, mixerR2;
    AudioMixer4 sumL, sumR;
    AudioOutputI2S i2s1;
    AudioConnection* patchCords[16]; // 8 for L, 8 for R
    AudioConnection* patchCordSumL;
    AudioConnection* patchCordSumR;
    AudioConnection* patchCordL;
    AudioConnection* patchCordR;

    std::map<int, int> keyToString; // key -> string index
    std::map<int, float> keyToBaseFreq; // key -> base frequency (without bend)
    float pitchBendFactor = 1.0f; // Current pitch bend multiplier
    float masterVolume = 1.0f;

public:
    Synthesizer() :
        patchCordSumL(nullptr),
        patchCordSumR(nullptr),
        patchCordL(nullptr),
        patchCordR(nullptr)
    {
        // Strings 0-3 to mixerL1 and mixerR1
        for (int i = 0; i < 4; ++i) {
            patchCords[i]     = new AudioConnection(strings[i], 0, mixerL1, i);
            patchCords[i + 8] = new AudioConnection(strings[i], 0, mixerR1, i);

        }
        // Strings 4-7 to mixerL2 and mixerR2
        for (int i = 4; i < 8; ++i) {
            patchCords[i]     = new AudioConnection(strings[i], 0, mixerL2, i - 4);
            patchCords[i + 8] = new AudioConnection(strings[i], 0, mixerR2, i - 4);
        }
        setVolume(1.0f);

        // Sum both left mixers and both right mixers
        patchCordSumL = new AudioConnection(mixerL1, 0, sumL, 0);
        patchCordSumL = new AudioConnection(mixerL2, 0, sumL, 1);
        patchCordSumR = new AudioConnection(mixerR1, 0, sumR, 0);
        patchCordSumR = new AudioConnection(mixerR2, 0, sumR, 1);
        sumL.gain(0, 1.0f); sumL.gain(1, 1.0f); // It's possible for these to clip, but unlikely
        sumR.gain(0, 1.0f); sumR.gain(1, 1.0f);

        // Final output to I2S
        patchCordL = new AudioConnection(sumL, 0, i2s1, 0);
        patchCordR = new AudioConnection(sumR, 0, i2s1, 1);
    }

    ~Synthesizer() {
        for (int i = 0; i < 16; ++i) delete patchCords[i];
        delete patchCordSumL;
        delete patchCordSumR;
        delete patchCordL;
        delete patchCordR;
    }

    void setVolume(float volume) {
        masterVolume = volume;
        for (int i = 0; i < 4; ++i) {
            mixerL1.gain(i, masterVolume);
            mixerR1.gain(i, masterVolume);
            mixerL2.gain(i, masterVolume);
            mixerR2.gain(i, masterVolume);
        }
    }

    void noteOn(int key, float freq, float velocity) {
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
        // No available string, do nothing or handle voice stealing here
    }

    void noteOff(int key) {
        auto it = keyToString.find(key);
        if (it != keyToString.end()) {
            int stringIndex = it->second;
            strings[stringIndex].noteOff();
            keyToString.erase(it);
            keyToBaseFreq.erase(key);
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
};