#include "DroneSynthesizer.h"
#include <Arduino.h>

DroneSynthesizer::DroneSynthesizer() 
    : playing(false)
    , currentFrequency(440.0f)
    , currentVelocity(0.0f)
    , filterCutoff(0.5f)
    , filterResonance(0.3f)
    , lfoRate(2.0f)
    , lfoDepth(0.2f)
    , oscillatorDetune(0.05f)
    , pulseWidth(0.5f)
    , attackTime(200.0f)
    , sustainLevel(0.8f)
    , releaseTime(800.0f)
    , masterVolume(1.0f)
{
    // Set up oscillator waveforms - using simple numeric constants for now
    osc1.begin(1.0, 440.0, 1);  // 1 = sawtooth
    osc2.begin(1.0, 440.0, 2);  // 2 = pulse wave
    subOsc.begin(1.0, 220.0, 3); // 3 = square wave
    
    // Set up LFO
    lfo.begin(1.0, lfoRate, 0);  // 0 = sine/triangle wave
    
    // Set oscillator mixer gains
    oscMixer.gain(0, 0.4f);  // osc1 (sawtooth)
    oscMixer.gain(1, 0.3f);  // osc2 (pulse)
    oscMixer.gain(2, 0.2f);  // subOsc (square, lower level)
    oscMixer.gain(3, 0.0f);  // unused
    
    // Set up audio connections
    patchCord1 = new AudioConnection(osc1, 0, oscMixer, 0);
    patchCord2 = new AudioConnection(osc2, 0, oscMixer, 1);
    patchCord3 = new AudioConnection(subOsc, 0, oscMixer, 2);
    patchCord4 = new AudioConnection(oscMixer, 0, envAmp, 0);
    patchCord5 = new AudioConnection(envAmp, 0, leftAmp, 0);
    patchCord6 = new AudioConnection(envAmp, 0, rightAmp, 0);
    
    // Initialize amplifiers
    envAmp.gain(0.0f);      // Start with envelope off
    leftAmp.gain(masterVolume);
    rightAmp.gain(masterVolume);
    
    // Set initial pulse width (if available)
    // osc2.pulseWidth(pulseWidth);  // Commented out for Phase 1
    
    Serial.println("DroneSynthesizer initialized");
}

DroneSynthesizer::~DroneSynthesizer() {
    delete patchCord1;
    delete patchCord2;
    delete patchCord3;
    delete patchCord4;
    delete patchCord5;
    delete patchCord6;
}

void DroneSynthesizer::noteOn(float frequency, float velocity) {
    if (velocity <= 0.0f || velocity > 1.0f) {
        return;
    }
    
    currentFrequency = frequency;
    currentVelocity = velocity;
    playing = true;
    
    updateOscillatorFrequencies();
    startEnvelope();
    
    Serial.printf("Drone noteOn: freq=%.2f, vel=%.2f\n", frequency, velocity);
}

void DroneSynthesizer::noteOff() {
    if (playing) {
        playing = false;
        stopEnvelope();
        Serial.println("Drone noteOff");
    }
}

void DroneSynthesizer::setFilterCutoff(float cutoff) {
    filterCutoff = constrain(cutoff, 0.0f, 1.0f);
    updateFilter();
}

void DroneSynthesizer::setFilterResonance(float resonance) {
    filterResonance = constrain(resonance, 0.0f, 1.0f);
    updateFilter();
}

void DroneSynthesizer::setLFORate(float rate) {
    lfoRate = constrain(rate, 0.1f, 10.0f);
    lfo.frequency(lfoRate);
    Serial.printf("LFO rate set to %.2f Hz\n", lfoRate);
}

void DroneSynthesizer::setLFODepth(float depth) {
    lfoDepth = constrain(depth, 0.0f, 1.0f);
    lfo.amplitude(lfoDepth);
    Serial.printf("LFO depth set to %.2f\n", lfoDepth);
}

void DroneSynthesizer::setOscillatorDetune(float detune) {
    oscillatorDetune = constrain(detune, -1.0f, 1.0f);
    if (playing) {
        updateOscillatorFrequencies();
    }
    Serial.printf("Oscillator detune set to %.3f semitones\n", oscillatorDetune);
}

void DroneSynthesizer::setPulseWidth(float width) {
    pulseWidth = constrain(width, 0.1f, 0.9f);
    // osc2.pulseWidth(pulseWidth);  // Commented out for Phase 1
    Serial.printf("Pulse width set to %.2f (not applied yet)\n", pulseWidth);
}

void DroneSynthesizer::setAttackTime(float attackMs) {
    attackTime = constrain(attackMs, 10.0f, 2000.0f);
    Serial.printf("Attack time set to %.0f ms\n", attackTime);
}

void DroneSynthesizer::setSustainLevel(float sustain) {
    sustainLevel = constrain(sustain, 0.0f, 1.0f);
    Serial.printf("Sustain level set to %.2f\n", sustainLevel);
}

void DroneSynthesizer::setReleaseTime(float releaseMs) {
    releaseTime = constrain(releaseMs, 10.0f, 5000.0f);
    Serial.printf("Release time set to %.0f ms\n", releaseMs);
}

void DroneSynthesizer::setVolume(float volume) {
    masterVolume = constrain(volume, 0.0f, 1.0f);
    leftAmp.gain(masterVolume);
    rightAmp.gain(masterVolume);
    Serial.printf("Drone volume set to %.2f\n", masterVolume);
}

AudioStream* DroneSynthesizer::getLeftOutput() {
    return &leftAmp;
}

AudioStream* DroneSynthesizer::getRightOutput() {
    return &rightAmp;
}

void DroneSynthesizer::updateOscillatorFrequencies() {
    if (currentFrequency <= 0.0f) return;
    
    // Main oscillator at fundamental frequency
    osc1.frequency(currentFrequency);
    
    // Second oscillator with slight detuning (creates analog warmth)
    float detuneFactor = pow(2.0f, oscillatorDetune / 12.0f);
    osc2.frequency(currentFrequency * detuneFactor);
    
    // Sub-oscillator one octave down
    subOsc.frequency(currentFrequency * 0.5f);
    
    Serial.printf("Frequencies: osc1=%.2f, osc2=%.2f, sub=%.2f\n", 
                  currentFrequency, currentFrequency * detuneFactor, currentFrequency * 0.5f);
}

void DroneSynthesizer::updateFilter() {
    // Note: In this phase 1 implementation, we don't have a real filter yet
    // This is a placeholder for when we add proper filtering in later phases
    Serial.printf("Filter update: cutoff=%.2f, resonance=%.2f\n", filterCutoff, filterResonance);
}

void DroneSynthesizer::startEnvelope() {
    // Simple envelope simulation using amplitude ramp
    // In Phase 1, we'll use a basic approach
    float targetGain = currentVelocity * sustainLevel;
    envAmp.gain(targetGain);
    
    Serial.printf("Envelope started: target gain=%.2f\n", targetGain);
}

void DroneSynthesizer::stopEnvelope() {
    // Simple release - immediately stop for now
    // In later phases, we'll implement proper release timing
    envAmp.gain(0.0f);
    
    Serial.println("Envelope stopped");
}
