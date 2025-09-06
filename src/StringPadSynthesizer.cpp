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

void StringPadSynthesizer::StringVoice::initialize() {
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
    voice.initialize();
}

StringPadSynthesizer::~StringPadSynthesizer() {
    // Cleanup handled by voice destructor
}

void StringPadSynthesizer::noteOn(int midiNote, float velocity) {
    if (velocity <= 0.0f || velocity > 1.0f) {
        noteOff();
        return;
    }
    
    float frequency = midiNoteToFrequency(midiNote);
    voice.startNote(midiNote, frequency, velocity);
    updateVoiceParameters();
}

void StringPadSynthesizer::noteOff() {
    voice.stopNote();
}

bool StringPadSynthesizer::isActive() const {
    return voice.active;
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
    return &voice.envAmp;
}

void StringPadSynthesizer::processEnvelope() {
    voice.updateEnvelope();
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
    // Update filter settings
    voice.updateFilter(filterCutoff, filterResonance);
    
    // Update oscillator frequencies with detuning
    if (voice.active) {
        voice.updateOscillatorFrequencies(voice.baseFrequency, detuneAmount);
    }
}
