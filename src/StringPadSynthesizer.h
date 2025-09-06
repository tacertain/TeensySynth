#pragma once

#include <Audio.h>
#include <AudioStream.h>

/**
 * StringPadSynthesizer - Phase 1 Implementation
 * 
 * Classic 80s string synthesizer designed to recreate the lush, ensemble
 * string pad sounds characteristic of instruments like the Korg Delta,
 * Roland Jupiter-6, and similar analog string synthesizers.
 * 
 * This Phase 1 implementation provides a single voice with 3-layer ensemble
 * for testing basic functionality and sound character.
 */
class StringPadSynthesizer {
public:
    StringPadSynthesizer();
    ~StringPadSynthesizer();

    // Single voice control (Phase 1)
    void noteOn(int midiNote, float velocity);
    void noteOff();
    bool isActive() const;
    
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
    // Single voice with 3-layer ensemble
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
        void initialize();
        void cleanup();
        void startNote(int note, float freq, float vel);
        void stopNote();
        void updateEnvelope();
        void updateOscillatorFrequencies(float baseFreq, float detuneAmount);
        void updateFilter(float cutoff, float resonance);
    };
    
    // Single voice for Phase 1
    StringVoice voice;
    
    // Global parameters
    float filterCutoff;        // 0.0 - 1.0
    float filterResonance;     // 0.0 - 1.0
    float chorusDepth;         // 0.0 - 1.0 (for future use)
    float detuneAmount;        // 0.0 - 1.0 (maps to cents)
    float attackTime;          // milliseconds
    float releaseTime;         // milliseconds
    float masterVolume;        // 0.0 - 1.0
    
    // Utility methods
    float midiNoteToFrequency(int midiNote);
    float mapCutoffToFrequency(float cutoff01);
    float mapDetuneToSemitones(float detune01);
    void updateVoiceParameters();
};
