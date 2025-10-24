#include "StringsSynthesizer.h"
#include <Arduino.h>

StringsSynthesizer::StringsSynthesizer()
    : pitchBendFactor(1.0f)
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
    
    // Connect voices 4-7 to mixerL2 and mixerR2
    for (int i = 4; i < MAX_VOICES; ++i) {
        voiceConnectionsL[i] = new AudioConnection(voices[i], 0, mixerL2, i - 4);
        voiceConnectionsR[i] = new AudioConnection(voices[i], 0, mixerR2, i - 4);
    }
    
    // Set initial mixer gains
    updateMixerGains();
}

StringsSynthesizer::~StringsSynthesizer() {
    // Clean up all audio connections
    for (int i = 0; i < MAX_VOICES; ++i) {
        delete voiceConnectionsL[i];
        delete voiceConnectionsR[i];
    }
}

void StringsSynthesizer::noteOn(int midiNote, float velocity) {
    // Find an available voice
    int voiceIndex = findAvailableVoice();
    if (voiceIndex == -1) {
        // No available voice - could implement voice stealing here
        return;
    }
    
    // Calculate frequency
    float freq = midiNoteToFrequency(midiNote);
    
    // Start the note on the voice
    if (voices[voiceIndex].noteOn(freq * pitchBendFactor, velocity) == 0) {
        noteToVoice[midiNote] = voiceIndex;
        noteToBaseFreq[midiNote] = freq;
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
    return &mixerL2;
}

AudioStream* StringsSynthesizer::getRightOutput() {
    return &mixerR2;
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
