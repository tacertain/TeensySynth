#pragma once
#include <synth_karplusstrong.h>
#include <output_i2s.h>
#include <Audio.h>
#include <map>

class Synthesizer {

    AudioSynthKarplusStrong strings[8];
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
            mixerL1.gain(i, 0.25f);
            mixerR1.gain(i, 0.25f);
        }
        // Strings 4-7 to mixerL2 and mixerR2
        for (int i = 4; i < 8; ++i) {
            patchCords[i]     = new AudioConnection(strings[i], 0, mixerL2, i - 4);
            patchCords[i + 8] = new AudioConnection(strings[i], 0, mixerR2, i - 4);
            mixerL2.gain(i - 4, 0.25f);
            mixerR2.gain(i - 4, 0.25f);
        }
        // Sum both left mixers and both right mixers
        patchCordSumL = new AudioConnection(mixerL1, 0, sumL, 0);
        patchCordSumL = new AudioConnection(mixerL2, 0, sumL, 1);
        patchCordSumR = new AudioConnection(mixerR1, 0, sumR, 0);
        patchCordSumR = new AudioConnection(mixerR2, 0, sumR, 1);
        sumL.gain(0, 0.5f); sumL.gain(1, 0.5f);
        sumR.gain(0, 0.5f); sumR.gain(1, 0.5f);

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
                strings[i].noteOn(freq, velocity);
                keyToString[key] = i;
                return;
            }
        }
        // No available string, do nothing or handle voice stealing here
    }

    void noteOff(int key) {
        auto it = keyToString.find(key);
        if (it != keyToString.end()) {
            int stringIndex = it->second;
            strings[stringIndex].noteOff(0.0f);
            keyToString.erase(it);
        }
    }
};