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
    // Chord mode enum - must be declared before use in method signatures
    enum ChordMode {
        CHORD_MODE_OFF = 0,     // Normal single note mode
        CHORD_MODE_MAJOR,       // Major chord mode
        CHORD_MODE_OCTAVE       // Single note plus octave mode
    };

    StringPadSynthesizer();
    ~StringPadSynthesizer();

    // Polyphonic voice control (Phase 2)
    void noteOn(int midiNote, float velocity, ChordMode chordMode = CHORD_MODE_OFF);
    void noteOff(int midiNote, ChordMode chordMode = CHORD_MODE_OFF);
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
    
    // Preset system (Phase 3)
    enum StringPadPreset {
        PRESET_LUSH_PADS = 0,      // "I Ran" style lush string pads
        PRESET_BRIGHT_STRINGS,     // Brighter, more aggressive strings
        PRESET_SOFT_ENSEMBLE,      // Soft, subtle ensemble strings
        PRESET_ANALOG_WARMTH,      // Warm analog-style pads
        PRESET_SHIMMER,            // Shimmery, ethereal strings
        PRESET_COUNT               // Number of presets
    };
    
    void loadPreset(StringPadPreset preset);
    StringPadPreset getCurrentPreset() const;
    const char* getPresetName(StringPadPreset preset) const;
    
    void setChordMode(ChordMode mode);
    ChordMode getChordMode() const;
    
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
    
    // Chord mode
    ChordMode chordMode;
    
    // Preset system (Phase 3)
    struct PresetData {
        float filterCutoff;
        float filterResonance;
        float detuneAmount;
        float masterVolume;
        float attackTime;
        float releaseTime;
    };
    
    static const PresetData presetData[PRESET_COUNT];
    StringPadPreset currentPreset;
    
    // Voice allocation system
    int findAvailableVoice();
    int findVoicePlayingNote(int midiNote);
    int findOldestVoice();
    
    // Chord generation helpers
    void playMajorChord(int rootNote, float velocity);
    void stopMajorChord(int rootNote);
    void playOctaveNote(int rootNote, float velocity);
    void stopOctaveNote(int rootNote);
    void updateAllVoiceParameters();
    
    // Utility methods
    float midiNoteToFrequency(int midiNote);
    float mapCutoffToFrequency(float cutoff01);
    float mapDetuneToSemitones(float detune01);
    void updateVoiceParameters();
};
