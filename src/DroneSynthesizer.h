#pragma once

#include <Audio.h>
#include <AudioStream.h>

/**
 * DroneSynthesizer - Single Voice Implementation
 * 
 * Monophonic analog-style drone synthesizer designed to recreate
 * the classic 1980s Korg MS-10 sound as heard in "I Ran" by Flock of Seagulls.
 * 
 * This implementation uses a single voice for classic monophonic drone sounds.
 */
class DroneSynthesizer {
public:
    DroneSynthesizer();
    ~DroneSynthesizer();

    // Monophonic note control
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    
    // Real-time parameter control
    void setFilterCutoff(float cutoff);     // 0.0 - 1.0
    void setFilterResonance(float resonance); // 0.0 - 1.0
    void setLFORate(float rate);            // 0.1 - 10.0 Hz
    void setLFODepth(float depth);          // 0.0 - 1.0
    void setOscillatorDetune(float detune); // -1.0 to +1.0 semitones
    void setPulseWidth(float width);        // 0.1 - 0.9
    
    // Envelope parameters
    void setAttackTime(float attackMs);     // 10 - 2000 ms
    void setSustainLevel(float sustain);    // 0.0 - 1.0
    void setReleaseTime(float releaseMs);   // 10 - 5000 ms
    
    // Volume control
    void setVolume(float volume);           // 0.0 - 1.0
    
    // Audio outputs (stereo)
    AudioStream* getLeftOutput();
    AudioStream* getRightOutput();
    
    // Processing - call regularly to update envelopes
    void processEnvelopes();

private:
    // Single voice structure
    struct DroneVoice {
        // Core oscillators
        AudioSynthWaveform osc1;           // Main oscillator (sawtooth)
        AudioSynthWaveform osc2;           // Second oscillator for detuning (pulse)
        AudioSynthWaveform subOsc;         // Sub-oscillator (square, 1 octave down)
        
        // Voice mixing
        AudioMixer4 oscMixer;
        
        // Filter
        AudioFilterStateVariable filter;    // Lowpass filter with cutoff and resonance
        
        // Envelope simulation
        AudioAmplifier envAmp;
        
        // Voice state
        bool active;
        int midiNote;
        float frequency;
        float velocity;
        unsigned long noteOnTime;
        unsigned long noteOffTime;
        bool releasing;

        // Audio connections for this voice
        AudioConnection *patchCord1; // osc1 to mixer
        AudioConnection *patchCord2; // osc2 to mixer
        AudioConnection *patchCord3; // subOsc to mixer
        AudioConnection *patchCord4; // mixer to filter
        AudioConnection *patchCord5; // filter to envelope

        DroneVoice();
        ~DroneVoice();
        void initialize();
        void cleanup();
        void startNote(int note, float freq, float vel);
        void stopNote();
        void updateEnvelope();
        void updateFilter(float cutoff, float resonance, float lfoValue);
    };
    
    // Single voice instance
    DroneVoice voice;
    
    // Global LFO
    AudioSynthWaveform globalLFO;
    
    // Audio output (simplified for single voice)
    AudioAmplifier leftAmp;                // Final left output
    AudioAmplifier rightAmp;               // Final right output
    
    // Audio connections
    AudioConnection* outputConnectionL;    // voice to left output
    AudioConnection* outputConnectionR;    // voice to right output
    
    // Global parameters
    float filterCutoff;
    float filterResonance;
    float lfoRate;
    float lfoDepth;
    float oscillatorDetune;
    float pulseWidth;
    float attackTime;
    float sustainLevel;
    float releaseTime;
    float masterVolume;
    
    // Internal methods
    void updateVoiceParameters();
    float midiNoteToFrequency(int midiNote);
    void updateGlobalLFO();
    void updateVoiceFilter();  // Apply LFO modulation to the voice
};
