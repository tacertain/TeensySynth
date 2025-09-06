#include "StringPadSynthesizer.h"
#include <Arduino.h>

// StringVoice implementation
StringPadSynthesizer::StringVoice::StringVoice() 
    : active(false)
    , midiNote(-1)
    , baseFrequency(0.0f)
    , velocity(0.0f)
    , noteOnTime(0)
    , noteOffTime(0)
    , releasing(false)
    , voiceId(-1)
    , currentGain(0.0f)
    , patchCord1(nullptr)
    , patchCord2(nullptr)
    , patchCord3(nullptr)
    , patchCord4(nullptr)
    , patchCord5(nullptr)
{
}

StringPadSynthesizer::StringVoice::~StringVoice() {
    cleanup();
}

void StringPadSynthesizer::StringVoice::initialize(int id) {
    voiceId = id;
    // Set up oscillator waveforms - all sawtooth for classic string sound
    osc1.begin(1.0, 440.0, WAVEFORM_SAWTOOTH);
    osc2.begin(1.0, 440.0, WAVEFORM_SAWTOOTH);
    osc3.begin(1.0, 440.0, WAVEFORM_SAWTOOTH);
    
    // Set ensemble mixer gains for balanced ensemble sound
    ensembleMixer.gain(0, 0.35f);  // osc1 (center)
    ensembleMixer.gain(1, 0.33f);  // osc2 (sharp)
    ensembleMixer.gain(2, 0.32f);  // osc3 (flat)
    ensembleMixer.gain(3, 0.0f);   // unused
    
    // Initialize filter for warm string pad sound
    filter.frequency(800.0f);      // Warm initial cutoff
    filter.resonance(0.3f);        // Gentle resonance
    filter.octaveControl(7.0f);    // Full range control
    
    // Create audio connections
    patchCord1 = new AudioConnection(osc1, 0, ensembleMixer, 0);
    patchCord2 = new AudioConnection(osc2, 0, ensembleMixer, 1);
    patchCord3 = new AudioConnection(osc3, 0, ensembleMixer, 2);
    patchCord4 = new AudioConnection(ensembleMixer, 0, filter, 0);
    patchCord5 = new AudioConnection(filter, 0, envAmp, 0);
    
    // Initialize envelope amplifier
    envAmp.gain(0.0f);
    
    currentGain = 0.0f;
}

void StringPadSynthesizer::StringVoice::cleanup() {
    delete patchCord1;
    delete patchCord2;
    delete patchCord3;
    delete patchCord4;
    delete patchCord5;
    patchCord1 = patchCord2 = patchCord3 = patchCord4 = patchCord5 = nullptr;
}

void StringPadSynthesizer::StringVoice::startNote(int note, float freq, float vel) {
    active = true;
    midiNote = note;
    baseFrequency = freq;
    velocity = vel;
    noteOnTime = millis();
    noteOffTime = 0;
    releasing = false;
    currentGain = 0.0f;  // Start from silence for attack
    
    // Set base frequencies - will be updated with detuning by parent class
    osc1.frequency(baseFrequency);
    osc2.frequency(baseFrequency);
    osc3.frequency(baseFrequency);
    
    // Start with zero gain - envelope will ramp up
    envAmp.gain(0.0f);
}

void StringPadSynthesizer::StringVoice::stopNote() {
    if (active && !releasing) {
        releasing = true;
        noteOffTime = millis();
    }
}

void StringPadSynthesizer::StringVoice::updateEnvelope() {
    if (!active) return;
    
    unsigned long currentTime = millis();
    
    if (!releasing) {
        // Attack phase - ramp up from 0 to velocity level
        unsigned long attackTime = currentTime - noteOnTime;
        
        // Simple linear attack for now - could be improved with exponential curves
        if (attackTime < 200) { // 200ms default attack
            float attackProgress = (float)attackTime / 200.0f;
            currentGain = velocity * 0.6f * attackProgress; // 0.6 max gain for headroom
        } else {
            // Sustain phase
            currentGain = velocity * 0.6f;
        }
        
        envAmp.gain(currentGain);
        
    } else {
        // Release phase - ramp down to 0
        if (noteOffTime == 0) {
            noteOffTime = currentTime;
        }
        
        unsigned long releaseTime = currentTime - noteOffTime;
        
        // Simple linear release - 1000ms default
        if (releaseTime < 1000) {
            float releaseProgress = (float)releaseTime / 1000.0f;
            currentGain = velocity * 0.6f * (1.0f - releaseProgress);
            envAmp.gain(currentGain);
        } else {
            // Note finished
            currentGain = 0.0f;
            envAmp.gain(0.0f);
            active = false;
        }
    }
}

void StringPadSynthesizer::StringVoice::updateOscillatorFrequencies(float baseFreq, float detuneAmount) {
    if (!active) return;
    
    // Convert detune amount (0.0-1.0) to cents (0-15 cents)
    float maxDetuneCents = 15.0f;
    float detuneCents = detuneAmount * maxDetuneCents;
    
    // Convert cents to frequency multipliers
    float sharpMultiplier = pow(2.0f, (detuneCents * 0.7f) / 1200.0f);  // +7 cents at max
    float flatMultiplier = pow(2.0f, (-detuneCents * 0.5f) / 1200.0f);  // -5 cents at max
    
    osc1.frequency(baseFreq);                    // Center frequency
    osc2.frequency(baseFreq * sharpMultiplier);  // Slightly sharp
    osc3.frequency(baseFreq * flatMultiplier);   // Slightly flat
}

void StringPadSynthesizer::StringVoice::updateFilter(float cutoff, float resonance) {
    // Map cutoff (0.0-1.0) to frequency range (200-2000 Hz)
    float filterFreq = 200.0f + (cutoff * 1800.0f);
    
    filter.frequency(filterFreq);
    filter.resonance(resonance * 4.0f); // Scale for reasonable resonance range
}

// StringPadSynthesizer implementation
StringPadSynthesizer::StringPadSynthesizer()
    : filterCutoff(0.4f)        // Start with warm sound
    , filterResonance(0.3f)     // Gentle resonance
    , chorusDepth(0.5f)         // Medium chorus depth (for future use)
    , detuneAmount(0.6f)        // Medium detuning for ensemble effect
    , attackTime(200.0f)        // 200ms attack
    , releaseTime(1000.0f)      // 1000ms release
    , masterVolume(0.8f)        // 80% volume
{
    // Initialize all voices
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].initialize(i);
        voiceConnections[i * 2] = nullptr;     // Left connection
        voiceConnections[i * 2 + 1] = nullptr; // Right connection
    }
    
    // Set up voice mixing for polyphony
    // Mixer 1: Voices 0-3 (4 inputs each)
    voiceMixerL1.gain(0, 0.25f);  // Voice 0
    voiceMixerL1.gain(1, 0.25f);  // Voice 1
    voiceMixerL1.gain(2, 0.25f);  // Voice 2
    voiceMixerL1.gain(3, 0.25f);  // Voice 3
    
    voiceMixerR1.gain(0, 0.25f);  // Voice 0
    voiceMixerR1.gain(1, 0.25f);  // Voice 1
    voiceMixerR1.gain(2, 0.25f);  // Voice 2
    voiceMixerR1.gain(3, 0.25f);  // Voice 3
    
    // Mixer 2: Voices 4-5 + mix from mixer1 (3 inputs used)
    voiceMixerL2.gain(0, 1.0f);   // Mix from voiceMixerL1
    voiceMixerL2.gain(1, 0.25f);  // Voice 4
    voiceMixerL2.gain(2, 0.25f);  // Voice 5
    voiceMixerL2.gain(3, 0.0f);   // Unused
    
    voiceMixerR2.gain(0, 1.0f);   // Mix from voiceMixerR1
    voiceMixerR2.gain(1, 0.25f);  // Voice 4
    voiceMixerR2.gain(2, 0.25f);  // Voice 5
    voiceMixerR2.gain(3, 0.0f);   // Unused
    
    // Connect voices to mixers
    voiceConnections[0] = new AudioConnection(voices[0].envAmp, 0, voiceMixerL1, 0);  // Voice 0 -> L1
    voiceConnections[1] = new AudioConnection(voices[0].envAmp, 0, voiceMixerR1, 0);  // Voice 0 -> R1
    voiceConnections[2] = new AudioConnection(voices[1].envAmp, 0, voiceMixerL1, 1);  // Voice 1 -> L1
    voiceConnections[3] = new AudioConnection(voices[1].envAmp, 0, voiceMixerR1, 1);  // Voice 1 -> R1
    voiceConnections[4] = new AudioConnection(voices[2].envAmp, 0, voiceMixerL1, 2);  // Voice 2 -> L1
    voiceConnections[5] = new AudioConnection(voices[2].envAmp, 0, voiceMixerR1, 2);  // Voice 2 -> R1
    voiceConnections[6] = new AudioConnection(voices[3].envAmp, 0, voiceMixerL1, 3);  // Voice 3 -> L1
    voiceConnections[7] = new AudioConnection(voices[3].envAmp, 0, voiceMixerR1, 3);  // Voice 3 -> R1
    
    voiceConnections[8] = new AudioConnection(voices[4].envAmp, 0, voiceMixerL2, 1);  // Voice 4 -> L2
    voiceConnections[9] = new AudioConnection(voices[4].envAmp, 0, voiceMixerR2, 1);  // Voice 4 -> R2
    voiceConnections[10] = new AudioConnection(voices[5].envAmp, 0, voiceMixerL2, 2); // Voice 5 -> L2
    voiceConnections[11] = new AudioConnection(voices[5].envAmp, 0, voiceMixerR2, 2); // Voice 5 -> R2
    
    // Connect mixer1 to mixer2
    new AudioConnection(voiceMixerL1, 0, voiceMixerL2, 0);
    new AudioConnection(voiceMixerR1, 0, voiceMixerR2, 0);
}

StringPadSynthesizer::~StringPadSynthesizer() {
    // Clean up voice connections
    for (int i = 0; i < MAX_VOICES * 2; i++) {
        delete voiceConnections[i];
    }
}

void StringPadSynthesizer::noteOn(int midiNote, float velocity) {
    if (velocity <= 0.0f || velocity > 1.0f) {
        return;
    }
    
    // Check if this note is already playing
    int existingVoice = findVoicePlayingNote(midiNote);
    if (existingVoice >= 0) {
        // Retrigger existing note
        voices[existingVoice].stopNote();
    }
    
    // Find an available voice
    int voiceIndex = findAvailableVoice();
    if (voiceIndex < 0) {
        // No available voices, steal the oldest one
        voiceIndex = findOldestVoice();
    }
    
    if (voiceIndex >= 0) {
        float frequency = midiNoteToFrequency(midiNote);
        voices[voiceIndex].startNote(midiNote, frequency, velocity);
        voices[voiceIndex].updateOscillatorFrequencies(frequency, detuneAmount);
        voices[voiceIndex].updateFilter(filterCutoff, filterResonance);
    }
}

void StringPadSynthesizer::noteOff(int midiNote) {
    // Find the voice playing this note
    int voiceIndex = findVoicePlayingNote(midiNote);
    if (voiceIndex >= 0) {
        voices[voiceIndex].stopNote();
    }
}

void StringPadSynthesizer::allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active) {
            voices[i].stopNote();
        }
    }
}

int StringPadSynthesizer::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active) {
            count++;
        }
    }
    return count;
}

void StringPadSynthesizer::setFilterCutoff(float cutoff) {
    filterCutoff = constrain(cutoff, 0.0f, 1.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setFilterResonance(float resonance) {
    filterResonance = constrain(resonance, 0.0f, 1.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setChorusDepth(float depth) {
    chorusDepth = constrain(depth, 0.0f, 1.0f);
    // Not implemented in Phase 1
}

void StringPadSynthesizer::setDetuneAmount(float detune) {
    detuneAmount = constrain(detune, 0.0f, 1.0f);
    updateVoiceParameters();
}

void StringPadSynthesizer::setAttackTime(float attackMs) {
    attackTime = constrain(attackMs, 50.0f, 2000.0f);
    // Note: Currently using hardcoded attack time in envelope
    // Will be implemented properly in later phases
}

void StringPadSynthesizer::setReleaseTime(float releaseMs) {
    releaseTime = constrain(releaseMs, 100.0f, 5000.0f);
    // Note: Currently using hardcoded release time in envelope
    // Will be implemented properly in later phases
}

void StringPadSynthesizer::setVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
    // Volume is handled through envelope gain in Phase 1
}

AudioStream* StringPadSynthesizer::getOutput() {
    return &voiceMixerL2;  // Final mixed output (we use left mixer for mono output)
}

void StringPadSynthesizer::processEnvelope() {
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].updateEnvelope();
    }
}

float StringPadSynthesizer::midiNoteToFrequency(int midiNote) {
    // Standard MIDI note to frequency conversion
    // A4 (MIDI note 69) = 440 Hz
    return 440.0f * pow(2.0f, (midiNote - 69) / 12.0f);
}

float StringPadSynthesizer::mapCutoffToFrequency(float cutoff01) {
    // Map 0.0-1.0 to 200-2000 Hz with slight exponential curve for more musical response
    float linear = 200.0f + (cutoff01 * 1800.0f);
    return linear;
}

float StringPadSynthesizer::mapDetuneToSemitones(float detune01) {
    // Map 0.0-1.0 to 0-15 cents
    return detune01 * 0.15f; // 15 cents = 0.15 semitones
}

void StringPadSynthesizer::updateVoiceParameters() {
    updateAllVoiceParameters();
}

void StringPadSynthesizer::updateAllVoiceParameters() {
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].updateFilter(filterCutoff, filterResonance);
        if (voices[i].active) {
            voices[i].updateOscillatorFrequencies(voices[i].baseFrequency, detuneAmount);
        }
    }
}

// Voice allocation methods
int StringPadSynthesizer::findAvailableVoice() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            return i;
        }
    }
    return -1; // No available voices
}

int StringPadSynthesizer::findVoicePlayingNote(int midiNote) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].midiNote == midiNote) {
            return i;
        }
    }
    return -1; // Note not found
}

int StringPadSynthesizer::findOldestVoice() {
    int oldestVoice = 0;
    unsigned long oldestTime = voices[0].noteOnTime;
    
    for (int i = 1; i < MAX_VOICES; i++) {
        if (voices[i].noteOnTime < oldestTime) {
            oldestTime = voices[i].noteOnTime;
            oldestVoice = i;
        }
    }
    
    return oldestVoice;
}
