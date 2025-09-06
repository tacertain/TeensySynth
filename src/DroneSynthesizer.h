#pragma once

#include <Audio.h>
#include <AudioStream.h>

/**
 * DroneSynthesizer - Phase 2 Implementation
 * 
 * Polyphonic analog-style drone synthesizer designed to recreate
 * the classic 1980s Korg MS-10 sound as heard in "I Ran" by Flock of Seagulls.
 * 
 * This Phase 2 implementation adds polyphonic capability with voice allocation,
 * supporting up to 6 simultaneous voices for chord pads and layered drones.
 */
class DroneSynthesizer {
public:
    // Voice management constants
    static const int MAX_VOICES = 6;
    
    DroneSynthesizer();
    ~DroneSynthesizer();

    // Polyphonic note control
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    
    // Real-time parameter control (affects all voices)
    void setFilterCutoff(float cutoff);     // 0.0 - 1.0
    void setFilterResonance(float resonance); // 0.0 - 1.0
    void setLFORate(float rate);            // 0.1 - 10.0 Hz
    void setLFODepth(float depth);          // 0.0 - 1.0
    void setOscillatorDetune(float detune); // -1.0 to +1.0 semitones
    void setPulseWidth(float width);        // 0.1 - 0.9
    
    // Envelope parameters (affects all voices)
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
    // Voice structure for polyphonic synthesis
    struct DroneVoice {
        // Core oscillators per voice
        AudioSynthWaveform osc1;           // Main oscillator (sawtooth)
        AudioSynthWaveform osc2;           // Second oscillator for detuning (pulse)
        AudioSynthWaveform subOsc;         // Sub-oscillator (square, 1 octave down)
        
        // Voice mixing
        AudioMixer4 oscMixer;
        
        // Filter per voice (Phase 3 Step 1)
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
        void updateFilter(float cutoff, float resonance, float lfoValue);  // Phase 3 Step 2
    };
    
    // Voice array
    DroneVoice voices[MAX_VOICES];
    
    // Global LFO (shared across all voices) - Phase 3 Step 2
    AudioSynthWaveform globalLFO;
    
    // Voice mixing and output
    AudioMixer4 voiceMixerL1;              // Voices 0-3 left
    AudioMixer4 voiceMixerL2;              // Voices 4-5 left (and unused channels)
    AudioMixer4 voiceMixerR1;              // Voices 0-3 right
    AudioMixer4 voiceMixerR2;              // Voices 4-5 right (and unused channels)
    AudioMixer4 masterMixerL;              // Final left mix
    AudioMixer4 masterMixerR;              // Final right mix
    AudioAmplifier leftAmp;                // Final left output
    AudioAmplifier rightAmp;               // Final right output
    
    // Voice mixer connections
    AudioConnection* voiceConnectionsL[MAX_VOICES];
    AudioConnection* voiceConnectionsR[MAX_VOICES];
    AudioConnection* mixerConnectionL1;
    AudioConnection* mixerConnectionL2; 
    AudioConnection* mixerConnectionR1;
    AudioConnection* mixerConnectionR2;
    AudioConnection* outputConnectionL;
    AudioConnection* outputConnectionR;
    
    // Global parameters (shared by all voices)
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
    
    // Voice management methods
    int findFreeVoice();
    int findVoiceByNote(int midiNote);
    int findOldestVoice();
    void updateVoiceParameters(DroneVoice& voice);
    float midiNoteToFrequency(int midiNote);
    
    // Internal methods
    void updateAllVoiceParameters();
    void updateGlobalLFO();
    void updateAllVoiceFilters();  // Phase 3 Step 2 - Apply LFO modulation to all voices
};
