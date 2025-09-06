#pragma once

#include <Audio.h>
#include <AudioStream.h>

/**
 * DroneSynthesizer - Phase 1 Implementation
 * 
 * Single-voice analog-style drone synthesizer designed to recreate
 * the classic 1980s Korg MS-10 sound as heard in "I Ran" by Flock of Seagulls.
 * 
 * This implementation focuses on the core oscillator + filter chain
 * with basic envelope and modulation capabilities.
 */
class DroneSynthesizer {
public:
    DroneSynthesizer();
    ~DroneSynthesizer();

    // Note control
    void noteOn(float frequency, float velocity);
    void noteOff();
    
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

private:
    // Core oscillators
    AudioSynthWaveform osc1;           // Main oscillator (sawtooth)
    AudioSynthWaveform osc2;           // Second oscillator for detuning (pulse)
    AudioSynthWaveform subOsc;         // Sub-oscillator (square, 1 octave down)
    
    // LFO for modulation
    AudioSynthWaveform lfo;
    
    // Mixing oscillators
    AudioMixer4 oscMixer;
    
    // Filter (using basic components available)
    // Note: Will use available filter components
    
    // Envelope generator (using basic components)
    AudioAmplifier envAmp;                 // For envelope simulation
    
    // Output amplifiers for stereo
    AudioAmplifier leftAmp;
    AudioAmplifier rightAmp;
    
    // Audio connections
    AudioConnection* patchCord1;           // osc1 to mixer
    AudioConnection* patchCord2;           // osc2 to mixer
    AudioConnection* patchCord3;           // subOsc to mixer
    AudioConnection* patchCord4;           // mixer to envelope
    AudioConnection* patchCord5;           // envelope to left amp
    AudioConnection* patchCord6;           // envelope to right amp
    
    // Current state
    bool playing;
    float currentFrequency;
    float currentVelocity;
    
    // Parameter storage
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
    void updateOscillatorFrequencies();
    void updateFilter();
    void startEnvelope();
    void stopEnvelope();
};
