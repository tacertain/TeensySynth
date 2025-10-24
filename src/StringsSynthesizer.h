#pragma once

#include <Audio.h>
#include <AudioStream.h>
#include <map>
#include "karplus_strong_string_synth.h"

/**
 * StringsSynthesizer - Karplus-Strong String Synthesis Wrapper
 * 
 * Encapsulates 8 Karplus-Strong plucked string voices with stereo output.
 * This class follows the pattern of StringPadSynthesizer, managing internal
 * voice allocation and mixing to provide a simple L/R output interface.
 * 
 * Features:
 * - 8-voice polyphony with automatic voice allocation
 * - Stereo output (mono signal duplicated to L/R)
 * - Pitch bend support for all active voices
 * - Per-voice attenuation and filter controls
 */
class StringsSynthesizer {
public:
    StringsSynthesizer();
    ~StringsSynthesizer();

    // Note control
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    
    // Pitch control
    void setPitchBend(float bendAmount); // bendAmount in semitones
    
    // String-specific controls
    void setAttenuation(uint8_t attenuation);
    void setFilterStrength(uint16_t strength);
    
    // Volume control
    void setVolume(float volume); // 0.0 - 1.0
    
    // Direct voice access (for advanced use cases like display)
    KarplusStrongStringSynth& getVoice(int index);
    int getActiveVoiceCount() const;
    
    // Audio outputs
    AudioStream* getLeftOutput();
    AudioStream* getRightOutput();

private:
    static const int MAX_VOICES = 8;
    
    // String voices
    KarplusStrongStringSynth voices[MAX_VOICES];
    
    // Audio mixing
    AudioMixer4 mixerL1;  // Mix voices 0-3 (left)
    AudioMixer4 mixerL2;  // Mix voices 4-7 (left)
    AudioMixer4 mixerR1;  // Mix voices 0-3 (right)
    AudioMixer4 mixerR2;  // Mix voices 4-7 (right)
    
    // Audio connections
    AudioConnection* voiceConnectionsL[MAX_VOICES]; // Voice to mixer L
    AudioConnection* voiceConnectionsR[MAX_VOICES]; // Voice to mixer R
    
    // Voice allocation tracking
    std::map<int, int> noteToVoice;      // MIDI note -> voice index
    std::map<int, float> noteToBaseFreq; // MIDI note -> base frequency (without bend)
    
    // Parameters
    float pitchBendFactor;
    float volume;
    uint8_t attenuation;
    uint16_t filterStrength;
    
    // Helper methods
    int findAvailableVoice();
    float midiNoteToFrequency(int midiNote);
    void updateMixerGains();
};
