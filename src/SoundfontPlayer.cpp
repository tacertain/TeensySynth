#include "SoundfontPlayer.h"
#include <Arduino.h>

void SoundfontPlayer::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (velocity == 0) {
        noteOff(channel, note);
        return;
    }
    
    // Find a free voice
    int voiceIndex = findFreeVoice();
    if (voiceIndex == -1) {
        Serial.println("No free voices available for soundfont");
        return;
    }
    
    // Calculate frequency for this note
    float frequency = 440.0f * powf(2.0f, (float)(note - 69) / 12.0f);
    
    // Apply pitch bend
    float bendFactor = powf(2.0f, channelPitchBend[channel] / 12.0f);
    frequency *= bendFactor;
    
    // Initialize the voice
    Voice& voice = voices[voiceIndex];
    voice.active = true;
    voice.note = note;
    voice.velocity = velocity;
    voice.channel = channel;
    voice.frequency = frequency;
    voice.amplitude = 0.0f;
    voice.targetAmplitude = (float)velocity / 127.0f * masterVolume;
    voice.phase = 0;
    voice.phaseIncrement = frequencyToPhaseIncrement(frequency);
    voice.envPhase = 0;
    voice.noteOffReceived = false;
    
    Serial.print("SoundfontPlayer: Started voice ");
    Serial.print(voiceIndex);
    Serial.print(" for note ");
    Serial.print(note);
    Serial.print(" freq ");
    Serial.print(frequency);
    Serial.print(" velocity ");
    Serial.println(velocity);
}

void SoundfontPlayer::noteOff(uint8_t channel, uint8_t note) {
    // Find active voice for this note and channel
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].note == note && voices[i].channel == channel) {
            voices[i].noteOffReceived = true;
            voices[i].targetAmplitude = 0.0f; // Start release phase
            Serial.print("SoundfontPlayer: Note off for voice ");
            Serial.print(i);
            Serial.print(" note ");
            Serial.println(note);
            break;
        }
    }
}

void SoundfontPlayer::setPitchBend(uint8_t channel, float bendSemitones) {
    channelPitchBend[channel] = bendSemitones;
    
    // Update all active voices on this channel
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && voices[i].channel == channel) {
            float frequency = 440.0f * powf(2.0f, (float)(voices[i].note - 69) / 12.0f);
            float bendFactor = powf(2.0f, bendSemitones / 12.0f);
            voices[i].frequency = frequency * bendFactor;
            voices[i].phaseIncrement = frequencyToPhaseIncrement(voices[i].frequency);
        }
    }
}

void SoundfontPlayer::allNotesOff() {
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].active = false;
    }
}

void SoundfontPlayer::update(void) {
    audio_block_t *block = allocate();
    if (!block) return;
    
    // Clear the output buffer
    memset(block->data, 0, AUDIO_BLOCK_SAMPLES * sizeof(int16_t));
    
    // Process each active voice
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active) {
            Voice& voice = voices[i];
            
            for (int sampleIndex = 0; sampleIndex < AUDIO_BLOCK_SAMPLES; sampleIndex++) {
                // Simple envelope
                if (voice.noteOffReceived) {
                    // Release: exponential decay
                    voice.amplitude *= 0.999f;
                    if (voice.amplitude < 0.001f) {
                        voice.active = false;
                        break;
                    }
                } else {
                    // Attack: linear ramp up
                    if (voice.amplitude < voice.targetAmplitude) {
                        voice.amplitude += 0.01f;
                        if (voice.amplitude > voice.targetAmplitude) {
                            voice.amplitude = voice.targetAmplitude;
                        }
                    }
                }
                
                // Generate sawtooth wave
                // Convert 32-bit phase to signed 16-bit sawtooth
                int32_t sawtoothValue = (int32_t)(voice.phase >> 16) - 32768;
                
                // Apply amplitude
                int32_t sample = (sawtoothValue * (int32_t)(voice.amplitude * 16384.0f)) >> 14;
                
                // Mix with existing buffer content
                int32_t mixed = block->data[sampleIndex] + sample;
                
                // Clamp to 16-bit range
                if (mixed > 32767) mixed = 32767;
                if (mixed < -32768) mixed = -32768;
                
                block->data[sampleIndex] = (int16_t)mixed;
                
                // Advance phase
                voice.phase += voice.phaseIncrement;
            }
        }
    }
    
    transmit(block);
    release(block);
}

int SoundfontPlayer::findFreeVoice() {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            return i;
        }
    }
    return -1; // No free voice
}

uint32_t SoundfontPlayer::frequencyToPhaseIncrement(float frequency) {
    // Convert frequency to 32-bit phase increment
    // Phase increment = (frequency * 2^32) / sample_rate
    return (uint32_t)((frequency * 4294967296.0f) / AUDIO_SAMPLE_RATE_EXACT);
}
