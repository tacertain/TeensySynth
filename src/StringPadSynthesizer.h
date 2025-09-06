#pragma once

#include <Audio.h>
#include <AudioStream.h>

/**
 * StringPadSynthesizer - Phase 2 Implementation
 * 
 * Classic 80s string synthesizer designed to recreate the lush, ensemble
 * string pad sounds characteristic of instruments like the Korg Delta,
 * Roland Jupiter-6, and similar analog string synthesizers.
 * 
 * This Phase 2 implementation provides 6-voice polyphony with intelligent
 * voice allocation, allowing full chord playing capabilities.
 */
class StringPadSynthesizer {
public:
    StringPadSynthesizer();
    ~StringPadSynthesizer();

    // Polyphonic voice control (Phase 2)
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    int getActiveVoiceCount() const;
    
    // Real-time parameter control
    void setFilterCutoff(float cutoff);     // 0.0 - 1.0 (maps to ~200-2000Hz)
    void setFilterResonance(float resonance); // 0.0 - 1.0
    void setChorusDepth(float depth);       // 0.0 - 1.0
    void setDetuneAmount(float detune);     // 0.0 - 1.0 (maps to ±0-15 cents)
    
    // Envelope parameters
    void setAttackTime(float attackMs);     // 50 - 2000 ms
    void setReleaseTime(float releaseMs);   // 100 - 5000 ms
    
    // Volume control
    void setVolume(float volume);           // 0.0 - 1.0
    
    // Audio output
    AudioStream* getOutput();
    
    // Processing - call regularly to update envelopes
    void processEnvelope();

private:
    static const int MAX_VOICES = 6;    // 6-voice polyphony
    
    // Multi-voice with 3-layer ensemble per voice
    struct StringVoice {
        // Three ensemble layers with different detuning
        AudioSynthWaveform osc1;           // Center pitch
        AudioSynthWaveform osc2;           // Slightly sharp (+7 cents)
        AudioSynthWaveform osc3;           // Slightly flat (-5 cents)
        
        // Voice mixing and processing
        AudioMixer4 ensembleMixer;         // Mix the 3 oscillators
        AudioFilterStateVariable filter;    // Lowpass filter
        AudioAmplifier envAmp;             // Envelope amplitude control
        
        // Voice state
        bool active;
        int midiNote;
        float baseFrequency;
        float velocity;
        unsigned long noteOnTime;
        unsigned long noteOffTime;
        bool releasing;
        int voiceId;                       // Voice identification for allocation
        
        // Envelope state
        float currentGain;
        
        // Audio connections
        AudioConnection *patchCord1; // osc1 to mixer
        AudioConnection *patchCord2; // osc2 to mixer  
        AudioConnection *patchCord3; // osc3 to mixer
        AudioConnection *patchCord4; // mixer to filter
        AudioConnection *patchCord5; // filter to envelope
        
        StringVoice();
        ~StringVoice();
        void initialize(int id);
        void cleanup();
        void startNote(int note, float freq, float vel);
        void stopNote();
        void updateEnvelope();
        void updateOscillatorFrequencies(float baseFreq, float detuneAmount);
        void updateFilter(float cutoff, float resonance);
    };
    
    // Voice array and mixing for polyphony
    StringVoice voices[MAX_VOICES];
    AudioMixer4 voiceMixerL1;          // Mix voices 0-3 (left channel)
    AudioMixer4 voiceMixerL2;          // Mix voices 4-5 + final mix (left)
    AudioMixer4 voiceMixerR1;          // Mix voices 0-3 (right channel)  
    AudioMixer4 voiceMixerR2;          // Mix voices 4-5 + final mix (right)
    
    // Voice mixing connections
    AudioConnection* voiceConnections[MAX_VOICES * 2]; // L and R for each voice
    
    // Global parameters
    float filterCutoff;        // 0.0 - 1.0
    float filterResonance;     // 0.0 - 1.0
    float chorusDepth;         // 0.0 - 1.0 (for future use)
    float detuneAmount;        // 0.0 - 1.0 (maps to cents)
    float attackTime;          // milliseconds
    float releaseTime;         // milliseconds
    float masterVolume;        // 0.0 - 1.0
    
    // Voice allocation system
    int findAvailableVoice();
    int findVoicePlayingNote(int midiNote);
    int findOldestVoice();
    void updateAllVoiceParameters();
    
    // Utility methods
    float midiNoteToFrequency(int midiNote);
    float mapCutoffToFrequency(float cutoff01);
    float mapDetuneToSemitones(float detune01);
    void updateVoiceParameters();
};
