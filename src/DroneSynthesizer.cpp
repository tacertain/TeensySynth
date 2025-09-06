#include "DroneSynthesizer.h"
#include <Arduino.h>

// DroneVoice implementation
DroneSynthesizer::DroneVoice::DroneVoice() 
    : active(false)
    , midiNote(-1)
    , frequency(0.0f)
    , velocity(0.0f)
    , noteOnTime(0)
    , noteOffTime(0)
    , releasing(false)
    , patchCord1(nullptr)
    , patchCord2(nullptr)
    , patchCord3(nullptr)
    , patchCord4(nullptr)
    , patchCord5(nullptr)
{
}

DroneSynthesizer::DroneVoice::~DroneVoice() {
    cleanup();
}

void DroneSynthesizer::DroneVoice::initialize() {
    // Set up oscillator waveforms
    osc1.begin(1.0, 440.0, 1);   // sawtooth
    osc2.begin(1.0, 440.0, 2);   // pulse wave
    subOsc.begin(1.0, 220.0, 3); // square wave
    
    // Set oscillator mixer gains
    oscMixer.gain(0, 0.4f);  // osc1 (sawtooth)
    oscMixer.gain(1, 0.3f);  // osc2 (pulse)
    oscMixer.gain(2, 0.2f);  // subOsc (square, lower level)
    oscMixer.gain(3, 0.0f);  // unused
    
    // Initialize filter (Phase 3 Step 1)
    filter.frequency(1000.0f);    // Default cutoff frequency
    filter.resonance(0.7f);       // Default resonance
    filter.octaveControl(7.0f);   // Full range control
    
    // Create audio connections (Phase 3 Step 1 - updated routing)
    patchCord1 = new AudioConnection(osc1, 0, oscMixer, 0);
    patchCord2 = new AudioConnection(osc2, 0, oscMixer, 1);
    patchCord3 = new AudioConnection(subOsc, 0, oscMixer, 2);
    patchCord4 = new AudioConnection(oscMixer, 0, filter, 0);      // mixer -> filter
    patchCord5 = new AudioConnection(filter, 0, envAmp, 0);        // filter -> envelope
    
    // Initialize envelope amplifier
    envAmp.gain(0.0f);
}

void DroneSynthesizer::DroneVoice::cleanup() {
    delete patchCord1;
    delete patchCord2;
    delete patchCord3;
    delete patchCord4;
    delete patchCord5;
    patchCord1 = patchCord2 = patchCord3 = patchCord4 = patchCord5 = nullptr;
}

void DroneSynthesizer::DroneVoice::startNote(int note, float freq, float vel) {
    active = true;
    midiNote = note;
    frequency = freq;
    velocity = vel;
    noteOnTime = millis();
    noteOffTime = 0;
    releasing = false;
    
    // Update oscillator frequencies
    osc1.frequency(frequency);
    osc2.frequency(frequency * 1.005f); // Slight detune for warmth
    subOsc.frequency(frequency * 0.5f);
    
    // Start envelope
    envAmp.gain(velocity * 0.8f); // Quick attack for now
}

void DroneSynthesizer::DroneVoice::stopNote() {
    if (active && !releasing) {
        releasing = true;
        noteOffTime = millis();
    }
}

void DroneSynthesizer::DroneVoice::updateEnvelope() {
    if (!active) return;
    
    // Release time constant (in milliseconds)
    static const unsigned long ENVELOPE_RELEASE_TIME_MS = 50;
    
    if (releasing) {
        // Simple release - fade out over time
        if (noteOffTime == 0) {
            // Safety check - if noteOffTime wasn't set properly, set it now
            noteOffTime = millis();
        }
        
        unsigned long releaseTime = millis() - noteOffTime;
        if (releaseTime > ENVELOPE_RELEASE_TIME_MS) { // 50ms release
            envAmp.gain(0.0f);
            active = false;
            releasing = false;
            midiNote = -1;
        } else {
            float releaseGain = (1.0f - (float)releaseTime / (float)ENVELOPE_RELEASE_TIME_MS) * velocity * 0.8f;
            envAmp.gain(releaseGain);
        }
    }
}

// Phase 3 Step 2: Update filter with LFO modulation
void DroneSynthesizer::DroneVoice::updateFilter(float cutoff, float resonance, float lfoValue) {
    if (!active) return;
    
    // Apply LFO modulation to filter cutoff
    // cutoff: 0.0-1.0 base cutoff frequency
    // lfoValue: -1.0 to +1.0 LFO output
    // Map to reasonable filter frequency range (100Hz - 8000Hz)
    float baseCutoff = 100.0f + (cutoff * 7900.0f);  // 100Hz to 8000Hz
    float lfoModulation = lfoValue * cutoff * 2000.0f;  // LFO can add ±2000Hz when cutoff is at max
    float finalCutoff = constrain(baseCutoff + lfoModulation, 100.0f, 8000.0f);
    
    filter.frequency(finalCutoff);
    filter.resonance(0.7f + resonance * 4.3f);  // Map 0.0-1.0 to 0.7-5.0 (reasonable Q range)
}

// Main DroneSynthesizer implementation  
DroneSynthesizer::DroneSynthesizer() 
    : filterCutoff(0.3f)
    , filterResonance(0.3f)
    , lfoRate(0.33f)
    , lfoDepth(0.75f)
    , oscillatorDetune(-0.11f)
    , pulseWidth(0.5f)
    , attackTime(200.0f)
    , sustainLevel(0.8f)
    , releaseTime(800.0f)
    , masterVolume(.25f)
{
    // Initialize all voices
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].initialize();
    }
    
    // Set up global LFO (Phase 3 Step 2)
    globalLFO.begin(1.0, lfoRate, 0); // sine wave for smooth modulation
    updateGlobalLFO();
    
    // Set up voice mixers - voices 0-3 to mixer1, voices 4-5 to mixer2
    voiceMixerL1.gain(0, 0.25f); // Voice 0
    voiceMixerL1.gain(1, 0.25f); // Voice 1  
    voiceMixerL1.gain(2, 0.25f); // Voice 2
    voiceMixerL1.gain(3, 0.25f); // Voice 3
    
    voiceMixerL2.gain(0, 0.5f);  // Voice 4
    voiceMixerL2.gain(1, 0.5f);  // Voice 5
    voiceMixerL2.gain(2, 0.0f);  // Unused
    voiceMixerL2.gain(3, 0.0f);  // Unused
    
    voiceMixerR1.gain(0, 0.25f); // Voice 0
    voiceMixerR1.gain(1, 0.25f); // Voice 1
    voiceMixerR1.gain(2, 0.25f); // Voice 2
    voiceMixerR1.gain(3, 0.25f); // Voice 3
    
    voiceMixerR2.gain(0, 0.5f);  // Voice 4
    voiceMixerR2.gain(1, 0.5f);  // Voice 5
    voiceMixerR2.gain(2, 0.0f);  // Unused
    voiceMixerR2.gain(3, 0.0f);  // Unused
    
    // Master mixer combines the two voice mixers
    masterMixerL.gain(0, 1.0f);  // voiceMixerL1
    masterMixerL.gain(1, 1.0f);  // voiceMixerL2
    masterMixerL.gain(2, 0.0f);  // Unused
    masterMixerL.gain(3, 0.0f);  // Unused
    
    masterMixerR.gain(0, 1.0f);  // voiceMixerR1
    masterMixerR.gain(1, 1.0f);  // voiceMixerR2
    masterMixerR.gain(2, 0.0f);  // Unused
    masterMixerR.gain(3, 0.0f);  // Unused
    
    // Connect voices to mixers
    for (int i = 0; i < MAX_VOICES; i++) {
        if (i < 4) {
            // Voices 0-3 go to mixer 1
            voiceConnectionsL[i] = new AudioConnection(voices[i].envAmp, 0, voiceMixerL1, i);
            voiceConnectionsR[i] = new AudioConnection(voices[i].envAmp, 0, voiceMixerR1, i);
        } else {
            // Voices 4-5 go to mixer 2
            voiceConnectionsL[i] = new AudioConnection(voices[i].envAmp, 0, voiceMixerL2, i - 4);
            voiceConnectionsR[i] = new AudioConnection(voices[i].envAmp, 0, voiceMixerR2, i - 4);
        }
    }
    
    // Connect voice mixers to master mixers
    mixerConnectionL1 = new AudioConnection(voiceMixerL1, 0, masterMixerL, 0);
    mixerConnectionL2 = new AudioConnection(voiceMixerL2, 0, masterMixerL, 1);
    mixerConnectionR1 = new AudioConnection(voiceMixerR1, 0, masterMixerR, 0);
    mixerConnectionR2 = new AudioConnection(voiceMixerR2, 0, masterMixerR, 1);
    
    // Connect master mixers to output amplifiers
    outputConnectionL = new AudioConnection(masterMixerL, 0, leftAmp, 0);
    outputConnectionR = new AudioConnection(masterMixerR, 0, rightAmp, 0);
    
    // Initialize output amplifiers
    leftAmp.gain(masterVolume);
    rightAmp.gain(masterVolume);
    
    Serial.println("DroneSynthesizer Phase 2 (Polyphonic) initialized");
}

DroneSynthesizer::~DroneSynthesizer() {
    // Clean up voice connections
    for (int i = 0; i < MAX_VOICES; i++) {
        delete voiceConnectionsL[i];
        delete voiceConnectionsR[i];
    }
    
    // Clean up mixer connections
    delete mixerConnectionL1;
    delete mixerConnectionL2;
    delete mixerConnectionR1;
    delete mixerConnectionR2;
    delete outputConnectionL;
    delete outputConnectionR;
}

void DroneSynthesizer::noteOn(int midiNote, float velocity) {
    if (velocity <= 0.0f || velocity > 1.0f) {
        return;
    }
    
    // First, check if this note is already playing - if so, retrigger it
    int existingVoice = findVoiceByNote(midiNote);
    if (existingVoice >= 0) {
        // Retrigger the existing voice
        float frequency = midiNoteToFrequency(midiNote);
        voices[existingVoice].startNote(midiNote, frequency, velocity);
        updateVoiceParameters(voices[existingVoice]);
        
        Serial.printf("Drone noteOn (retrigger): voice=%d, note=%d, freq=%.2f, vel=%.2f\n", 
                      existingVoice, midiNote, frequency, velocity);
        return;
    }
    
    // Find a free voice
    int voiceIndex = findFreeVoice();
    if (voiceIndex == -1) {
        // No free voices, steal the oldest one
        voiceIndex = findOldestVoice();
    }
    
    if (voiceIndex >= 0) {
        float frequency = midiNoteToFrequency(midiNote);
        voices[voiceIndex].startNote(midiNote, frequency, velocity);
        updateVoiceParameters(voices[voiceIndex]);
        
        Serial.printf("Drone noteOn: voice=%d, note=%d, freq=%.2f, vel=%.2f\n", 
                      voiceIndex, midiNote, frequency, velocity);
    }
}

void DroneSynthesizer::noteOff(int midiNote) {
    // Find the voice playing this note
    int voiceIndex = findVoiceByNote(midiNote);
    if (voiceIndex >= 0) {
        voices[voiceIndex].stopNote();
        Serial.printf("Drone noteOff: voice=%d, note=%d (was releasing=%d)\n", 
                      voiceIndex, midiNote, voices[voiceIndex].releasing ? 1 : 0);
    } else {
        Serial.printf("Drone noteOff: note=%d not found in any voice\n", midiNote);
    }
}

void DroneSynthesizer::allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active) {
            voices[i].stopNote();
        }
    }
    Serial.println("Drone allNotesOff");
}

void DroneSynthesizer::setFilterCutoff(float cutoff) {
    filterCutoff = constrain(cutoff, 0.0f, 1.0f);
    Serial.printf("Filter cutoff set to %.2f (affects all voices)\n", filterCutoff);
}

void DroneSynthesizer::setFilterResonance(float resonance) {
    filterResonance = constrain(resonance, 0.0f, 1.0f);
    Serial.printf("Filter resonance set to %.2f (affects all voices)\n", filterResonance);
}

void DroneSynthesizer::setLFORate(float rate) {
    lfoRate = constrain(rate, 0.1f, 10.0f);
    updateGlobalLFO();  // Update the actual LFO parameters
    Serial.printf("Global LFO rate set to %.2f Hz\n", lfoRate);
}

void DroneSynthesizer::setLFODepth(float depth) {
    lfoDepth = constrain(depth, 0.0f, 1.0f);
    updateGlobalLFO();  // Update the actual LFO parameters
    Serial.printf("Global LFO depth set to %.2f\n", lfoDepth);
}

void DroneSynthesizer::setOscillatorDetune(float detune) {
    oscillatorDetune = constrain(detune, -1.0f, 1.0f);
    updateAllVoiceParameters();
    Serial.printf("Oscillator detune set to %.3f semitones (affects all voices)\n", oscillatorDetune);
}

void DroneSynthesizer::setPulseWidth(float width) {
    pulseWidth = constrain(width, 0.1f, 0.9f);
    // Will be applied to osc2 in all voices when pulse width modulation is added in Phase 3
    Serial.printf("Pulse width set to %.2f (affects all voices, not applied yet)\n", pulseWidth);
}

void DroneSynthesizer::setAttackTime(float attackMs) {
    attackTime = constrain(attackMs, 10.0f, 2000.0f);
    Serial.printf("Attack time set to %.0f ms (affects all voices)\n", attackTime);
}

void DroneSynthesizer::setSustainLevel(float sustain) {
    sustainLevel = constrain(sustain, 0.0f, 1.0f);
    Serial.printf("Sustain level set to %.2f (affects all voices)\n", sustainLevel);
}

void DroneSynthesizer::setReleaseTime(float releaseMs) {
    releaseTime = constrain(releaseMs, 10.0f, 5000.0f);
    Serial.printf("Release time set to %.0f ms (affects all voices)\n", releaseMs);
}

void DroneSynthesizer::setVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
    leftAmp.gain(masterVolume);
    rightAmp.gain(masterVolume);
    Serial.printf("Drone master volume set to %.2f\n", masterVolume);
}

AudioStream* DroneSynthesizer::getLeftOutput() {
    return &leftAmp;
}

AudioStream* DroneSynthesizer::getRightOutput() {
    return &rightAmp;
}

// Voice management methods
int DroneSynthesizer::findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            return i;
        }
    }
    return -1; // No free voices
}

int DroneSynthesizer::findVoiceByNote(int midiNote) {
    for (int i = 0; i < MAX_VOICES; i++) {
        // A voice is "playing" a note if it's active (including releasing phase)
        if (voices[i].active && voices[i].midiNote == midiNote) {
            return i;
        }
    }
    return -1; // Note not found
}

int DroneSynthesizer::findOldestVoice() {
    unsigned long oldestTime = millis();
    int oldestVoice = 0;
    
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].noteOnTime < oldestTime) {
            oldestTime = voices[i].noteOnTime;
            oldestVoice = i;
        }
    }
    return oldestVoice;
}

void DroneSynthesizer::updateVoiceParameters(DroneVoice& voice) {
    if (!voice.active) return;
    
    // Apply current global parameters to this voice
    float detuneFactor = pow(2.0f, oscillatorDetune / 12.0f);
    voice.osc1.frequency(voice.frequency);
    voice.osc2.frequency(voice.frequency * detuneFactor);
    voice.subOsc.frequency(voice.frequency * 0.5f);
}

float DroneSynthesizer::midiNoteToFrequency(int midiNote) {
    // Standard MIDI note to frequency conversion: f = 440 * 2^((n-69)/12)
    return 440.0f * pow(2.0f, (float)(midiNote - 69) / 12.0f);
}

void DroneSynthesizer::updateAllVoiceParameters() {
    for (int i = 0; i < MAX_VOICES; i++) {
        updateVoiceParameters(voices[i]);
    }
}

void DroneSynthesizer::updateGlobalLFO() {
    globalLFO.frequency(lfoRate);
    globalLFO.amplitude(lfoDepth);
}

// Phase 3 Step 2: Apply LFO modulation to all voice filters
void DroneSynthesizer::updateAllVoiceFilters() {
    // Read current LFO value (this returns a sample from the audio stream)
    // Note: In a real implementation, this would be properly synchronized with audio blocks
    // For now, we'll simulate LFO with a simple sine wave calculation
    static unsigned long lastLFOUpdate = 0;
    static float lfoPhase = 0.0f;
    
    unsigned long currentTime = millis();
    if (currentTime - lastLFOUpdate >= 10) { // Update every 10ms for smooth modulation
        lastLFOUpdate = currentTime;
        
        // Calculate LFO value: -1.0 to +1.0
        lfoPhase += (lfoRate * 0.01f * 2.0f * PI); // 0.01 = 10ms update interval
        if (lfoPhase > 2.0f * PI) lfoPhase -= 2.0f * PI;
        
        float lfoValue = sin(lfoPhase) * lfoDepth;
        
        // Apply LFO modulation to all active voices
        for (int i = 0; i < MAX_VOICES; i++) {
            voices[i].updateFilter(filterCutoff, filterResonance, lfoValue);
        }
    }
}

void DroneSynthesizer::processEnvelopes() {
    // Update all voice envelopes
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].updateEnvelope();
    }
    
    // Update filters with LFO modulation (Phase 3 Step 2)
    updateAllVoiceFilters();
}
