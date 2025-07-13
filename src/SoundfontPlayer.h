#pragma once
#include <Audio.h>
#include <map>

class SoundfontPlayer : public AudioStream {
public:
    SoundfontPlayer() : AudioStream(0, NULL) {
        for (int i = 0; i < MAX_VOICES; i++) {
            voices[i].active = false;
            voices[i].frequency = 0;
            voices[i].amplitude = 0;
            voices[i].phase = 0;
            voices[i].phaseIncrement = 0;
        }
    }

    // Simple interface for now - we'll add SF2 loading later
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOff(uint8_t channel, uint8_t note);
    void setPitchBend(uint8_t channel, float bendSemitones);
    void setVolume(float volume) { masterVolume = volume; }
    void allNotesOff();
    bool loadSoundfont(const char* filename) { return true; } // Stub for now
    
    virtual void update(void) override;

private:
    static const int MAX_VOICES = 8;
    
    struct Voice {
        bool active;
        uint8_t note;
        uint8_t velocity;
        uint8_t channel;
        float frequency;
        float amplitude;
        float targetAmplitude;
        uint32_t phase;          // 32-bit phase accumulator
        uint32_t phaseIncrement; // 32-bit phase increment
        uint32_t envPhase;       // Envelope phase
        bool noteOffReceived;
    };
    
    Voice voices[MAX_VOICES];
    float masterVolume = 1.0f;
    float channelPitchBend[16] = {0}; // Per-channel pitch bend
    
    int findFreeVoice();
    uint32_t frequencyToPhaseIncrement(float frequency);
};
