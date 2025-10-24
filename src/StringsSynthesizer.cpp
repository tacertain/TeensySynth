#include "StringsSynthesizer.h"
#include <Arduino.h>

StringsSynthesizer::StringsSynthesizer()
    : mixerConnectionL1(nullptr)
    , mixerConnectionL2(nullptr)
    , mixerConnectionR1(nullptr)
    , mixerConnectionR2(nullptr)
    , pitchBendFactor(1.0f)
    , volume(1.0f)
    , attenuation(66)
    , filterStrength(103)
{
    // Initialize all voice connections
    // Connect voices 0-3 to mixerL1 and mixerR1
    for (int i = 0; i < 4; ++i) {
        voiceConnectionsL[i] = new AudioConnection(voices[i], 0, mixerL1, i);
        voiceConnectionsR[i] = new AudioConnection(voices[i], 0, mixerR1, i);
    }
    
    // Connect voices 4-7 to mixerL2 and mixerR2 (channels 0-3)
    for (int i = 4; i < MAX_VOICES; ++i) {
        voiceConnectionsL[i] = new AudioConnection(voices[i], 0, mixerL2, i - 4);
        voiceConnectionsR[i] = new AudioConnection(voices[i], 0, mixerR2, i - 4);
    }
    
    // CRITICAL: Connect mixerL1/R1 outputs to mixerL2/R2 so all 8 voices are heard!
    // This combines voices 0-3 (from mixerL1/R1) with voices 4-7 (in mixerL2/R2)
    // Using the 4th channel (index 3) of the second mixers
    mixerConnectionL1 = new AudioConnection(mixerL1, 0, sumL, 0);
    mixerConnectionL2 = new AudioConnection(mixerL2, 0, sumL, 1);
    mixerConnectionR1 = new AudioConnection(mixerR1, 0, sumR, 0);
    mixerConnectionR2 = new AudioConnection(mixerR2, 0, sumR, 1);

    // Set initial mixer gains
    updateMixerGains();
}

StringsSynthesizer::~StringsSynthesizer() {
    // Clean up all audio connections
    for (int i = 0; i < MAX_VOICES; ++i) {
        delete voiceConnectionsL[i];
        delete voiceConnectionsR[i];
    }
    delete mixerConnectionL1;
    delete mixerConnectionL2;
    delete mixerConnectionR1;
    delete mixerConnectionR2;
}

void StringsSynthesizer::noteOn(int midiNote, float velocity) {
    // Find an available voice
    int voiceIndex = findAvailableVoice();
    if (voiceIndex == -1) {
        // No available voice - could implement voice stealing here
        Serial.println("StringsSynthesizer: No available voice!");
        return;
    }
    
    // Calculate frequency
    float freq = midiNoteToFrequency(midiNote);
    
    Serial.print("StringsSynthesizer::noteOn - MIDI note: ");
    Serial.print(midiNote);
    Serial.print(", freq: ");
    Serial.print(freq);
    Serial.print(", velocity: ");
    Serial.print(velocity);
    Serial.print(", voice: ");
    Serial.println(voiceIndex);
    
    // Start the note on the voice
    int result = voices[voiceIndex].noteOn(freq * pitchBendFactor, velocity);
    if (result == 0) {
        noteToVoice[midiNote] = voiceIndex;
        noteToBaseFreq[midiNote] = freq;
        Serial.println("  -> Voice started successfully");
    } else {
        Serial.print("  -> Voice failed to start, error: ");
        Serial.println(result);
    }
}

void StringsSynthesizer::noteOff(int midiNote) {
    // Find the voice playing this note
    auto it = noteToVoice.find(midiNote);
    if (it != noteToVoice.end()) {
        int voiceIndex = it->second;
        voices[voiceIndex].noteOff();
        
        // Clean up tracking
        noteToVoice.erase(it);
        noteToBaseFreq.erase(midiNote);
    }
}

void StringsSynthesizer::allNotesOff() {
    // Stop all voices
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].noteOff();
    }
    
    // Clear tracking
    noteToVoice.clear();
    noteToBaseFreq.clear();
}

void StringsSynthesizer::setPitchBend(float bendAmount) {
    // bendAmount is in semitones
    pitchBendFactor = pow(2.0f, bendAmount / 12.0f);
    
    // Apply bend to all active voices
    for (const auto& pair : noteToVoice) {
        int midiNote = pair.first;
        int voiceIndex = pair.second;
        float baseFreq = noteToBaseFreq[midiNote];
        voices[voiceIndex].updateFrequency(baseFreq * pitchBendFactor);
    }
}

void StringsSynthesizer::setAttenuation(uint8_t newAttenuation) {
    attenuation = newAttenuation;
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].setAttenuation(attenuation);
    }
}

void StringsSynthesizer::setFilterStrength(uint16_t strength) {
    filterStrength = strength;
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].setFilterStrength(filterStrength);
    }
}

void StringsSynthesizer::setVolume(float newVolume) {
    volume = constrain(newVolume, 0.0f, 1.0f);
    updateMixerGains();
}

KarplusStrongStringSynth& StringsSynthesizer::getVoice(int index) {
    if (index >= 0 && index < MAX_VOICES) {
        return voices[index];
    }
    // Return first voice as fallback
    return voices[0];
}

int StringsSynthesizer::getActiveVoiceCount() const {
    return noteToVoice.size();
}

AudioStream* StringsSynthesizer::getLeftOutput() {
    return &sumL;
}

AudioStream* StringsSynthesizer::getRightOutput() {
    return &sumR;
}

int StringsSynthesizer::findAvailableVoice() {
    // Find a voice that's not in use
    for (int i = 0; i < MAX_VOICES; ++i) {
        bool inUse = false;
        for (const auto& pair : noteToVoice) {
            if (pair.second == i) {
                inUse = true;
                break;
            }
        }
        if (!inUse) {
            return i;
        }
    }
    return -1; // No available voice
}

float StringsSynthesizer::midiNoteToFrequency(int midiNote) {
    // MIDI note to frequency: f = 440 * 2^((n-69)/12)
    return 440.0f * pow(2.0f, (midiNote - 69) / 12.0f);
}

void StringsSynthesizer::updateMixerGains() {
    // Apply volume to all mixer channels
    for (int i = 0; i < 4; ++i) {
        mixerL1.gain(i, volume);
        mixerR1.gain(i, volume);
        mixerL2.gain(i, volume);
        mixerR2.gain(i, volume);
    }
}
