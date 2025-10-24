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
    osc1.begin(1.0, 440.0, WAVEFORM_SAWTOOTH);   // sawtooth
    osc2.begin(1.0, 440.0, WAVEFORM_SQUARE);   // pulse wave
    subOsc.begin(1.0, 220.0, WAVEFORM_TRIANGLE); // square wave
    
    // Set oscillator mixer gains
    oscMixer.gain(0, 0.4f);  // osc1 (sawtooth)
    oscMixer.gain(1, 0.3f);  // osc2 (pulse)
    oscMixer.gain(2, 0.2f);  // subOsc (square, lower level)
    oscMixer.gain(3, 0.0f);  // unused
    
    // Initialize filter
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
    , masterVolume(.2f)
{
    // Initialize the single voice
    voice.initialize();
    
    // Set up global LFO
    globalLFO.begin(1.0, lfoRate, WAVEFORM_SINE); // sine wave for smooth modulation
    updateGlobalLFO();
    
    // Connect voice directly to output amplifiers
    outputConnectionL = new AudioConnection(voice.envAmp, 0, leftAmp, 0);
    outputConnectionR = new AudioConnection(voice.envAmp, 0, rightAmp, 0);
    
    // Initialize output amplifiers
    leftAmp.gain(masterVolume);
    rightAmp.gain(masterVolume);
    
    Serial.println("DroneSynthesizer (Single Voice) initialized");
}

DroneSynthesizer::~DroneSynthesizer() {
    // Clean up connections
    delete outputConnectionL;
    delete outputConnectionR;
}

void DroneSynthesizer::noteOn(int midiNote, float velocity) {
    if (velocity <= 0.0f || velocity > 1.0f) {
        return;
    }
    
    // For single voice: always use the one voice
    float frequency = midiNoteToFrequency(midiNote);
    voice.startNote(midiNote, frequency, velocity);
    updateVoiceParameters();
    
    Serial.printf("Drone noteOn: note=%d, freq=%.2f, vel=%.2f\n", 
                  midiNote, frequency, velocity);
}

void DroneSynthesizer::noteOff(int midiNote) {
    // Only stop if this is the note currently playing
    if (voice.active && voice.midiNote == midiNote) {
        voice.stopNote();
        Serial.printf("Drone noteOff: note=%d\n", midiNote);
    } else {
        Serial.printf("Drone noteOff: note=%d not currently playing\n", midiNote);
    }
}

void DroneSynthesizer::allNotesOff() {
    if (voice.active) {
        voice.stopNote();
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
    lfoRate = constrain(rate, 0.0f, 10.0f);  // Allow 0.0f to disable LFO
    globalLFO.frequency(lfoRate);
    if (lfoRate == 0.0f) {
        Serial.println("Global LFO disabled");
    } else {
        Serial.printf("Global LFO rate set to %.2f Hz\n", lfoRate);
    }
}

void DroneSynthesizer::setLFODepth(float depth) {
    lfoDepth = constrain(depth, 0.0f, 1.0f);
    updateGlobalLFO();  // Update the actual LFO parameters
    Serial.printf("Global LFO depth set to %.2f\n", lfoDepth);
}

void DroneSynthesizer::setOscillatorDetune(float detune) {
    oscillatorDetune = constrain(detune, -1.0f, 1.0f);
    updateVoiceParameters();
    Serial.printf("Oscillator detune set to %.3f semitones\n", oscillatorDetune);
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

AudioStream* DroneSynthesizer::getOscMixer() {
    return &voice.oscMixer;
}

// Voice management methods (removed - single voice only)

void DroneSynthesizer::updateVoiceParameters() {
    if (!voice.active) return;
    
    // Apply current global parameters to the voice
    float detuneFactor = pow(2.0f, oscillatorDetune / 12.0f);
    voice.osc1.frequency(voice.frequency);
    voice.osc2.frequency(voice.frequency * detuneFactor);
    voice.subOsc.frequency(voice.frequency * 0.5f);
}

float DroneSynthesizer::midiNoteToFrequency(int midiNote) {
    // Standard MIDI note to frequency conversion: f = 440 * 2^((n-69)/12)
    return 440.0f * pow(2.0f, (float)(midiNote - 69) / 12.0f);
}

void DroneSynthesizer::updateGlobalLFO() {
    globalLFO.frequency(lfoRate);
    globalLFO.amplitude(lfoDepth);
}

// Apply LFO modulation to the single voice filter
void DroneSynthesizer::updateVoiceFilter() {
    // Read current LFO value
    static unsigned long lastLFOUpdate = 0;
    static float lfoPhase = 0.0f;
    
    unsigned long currentTime = millis();
    if (currentTime - lastLFOUpdate >= 10) { // Update every 10ms for smooth modulation
        lastLFOUpdate = currentTime;
        
        // Calculate LFO value: -1.0 to +1.0
        lfoPhase += (lfoRate * 0.01f * 2.0f * PI); // 0.01 = 10ms update interval
        if (lfoPhase > 2.0f * PI) lfoPhase -= 2.0f * PI;
        
        float lfoValue = sin(lfoPhase) * lfoDepth;
        
        // Apply LFO modulation to the voice
        voice.updateFilter(filterCutoff, filterResonance, lfoValue);
    }
}

void DroneSynthesizer::processEnvelopes() {
    // Update the voice envelope
    voice.updateEnvelope();
    
    // Update filter with LFO modulation
    updateVoiceFilter();
}
