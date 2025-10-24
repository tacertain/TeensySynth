#include "SoundfontSynthesizer.h"
#include <Arduino.h>

SoundfontSynthesizer::SoundfontSynthesizer()
    : instrumentData(nullptr)
    , initialized(false)
    , instrumentLoaded(false)
    , volume(1.0f)
    , mixerConnection(nullptr)
    , mixer2Connection(nullptr)
{
    currentInstrumentName[0] = '\0';
    
    // Initialize voice states
    for (int i = 0; i < MAX_VOICES; ++i) {
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
        voiceStates[i].noteOnTime = 0;
    }
    
    // Connect voices 0-3 to mixer1
    for (int i = 0; i < 4; ++i) {
        voiceConnections[i] = new AudioConnection(voices[i], 0, mixer1, i);
    }
    
    // Connect voices 4-7 to mixer2
    for (int i = 4; i < MAX_VOICES; ++i) {
        voiceConnections[i] = new AudioConnection(voices[i], 0, mixer2, i - 4);
    }
    
    // Connect mixer1 and mixer2 to finalMixer
    mixerConnection = new AudioConnection(mixer1, 0, finalMixer, 0);
    mixer2Connection = new AudioConnection(mixer2, 0, finalMixer, 1);
    
    // Set initial mixer gains
    updateMixerGains();
}

SoundfontSynthesizer::~SoundfontSynthesizer() {
    // Clean up audio connections
    for (int i = 0; i < MAX_VOICES; ++i) {
        delete voiceConnections[i];
    }
    delete mixerConnection;
    delete mixer2Connection;
    
    // Clean up instrument data
    if (instrumentData != nullptr) {
        delete instrumentData;
        instrumentData = nullptr;
    }
}

bool SoundfontSynthesizer::begin() {
    if (initialized) {
        return true;
    }
    
    // Check if SD card is initialized
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SoundfontSynthesizer: SD card initialization failed!");
        return false;
    }
    
    initialized = true;
    Serial.println("SoundfontSynthesizer: Initialized");
    return true;
}

bool SoundfontSynthesizer::loadInstrument(const char* filename, int instrumentIndex) {
    if (!initialized) {
        Serial.println("SoundfontSynthesizer: Not initialized! Call begin() first.");
        return false;
    }
    
    Serial.print("SoundfontSynthesizer: Loading instrument ");
    Serial.print(instrumentIndex);
    Serial.print(" from ");
    Serial.println(filename);
    
    // Store old instrument data to delete after switching
    AudioSynthWavetable::instrument_data* oldInstrument = instrumentData;
    
    // Load new instrument
    if (!sf22aswt.Load_instrument_from_file(filename, instrumentIndex, &instrumentData)) {
        Serial.println("SoundfontSynthesizer: Failed to load instrument!");
        return false;
    }
    
    // Set the instrument on all voices
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].setInstrument(*instrumentData);
    }
    
    // Clean up old instrument data
    if (oldInstrument != nullptr) {
        delete oldInstrument;
    }
    
    // Store instrument name
    snprintf(currentInstrumentName, sizeof(currentInstrumentName), "%s[%d]", filename, instrumentIndex);
    instrumentLoaded = true;
    
    Serial.println("SoundfontSynthesizer: Instrument loaded successfully");
    return true;
}

bool SoundfontSynthesizer::isInstrumentLoaded() const {
    return instrumentLoaded;
}

const char* SoundfontSynthesizer::getCurrentInstrumentName() const {
    return currentInstrumentName;
}

void SoundfontSynthesizer::noteOn(int midiNote, float velocity) {
    if (!instrumentLoaded) {
        Serial.println("SoundfontSynthesizer: No instrument loaded!");
        return;
    }
    
    // Find an available voice
    int voiceIndex = findAvailableVoice();
    if (voiceIndex == -1) {
        // No free voice, steal the oldest one
        voiceIndex = findOldestVoice();
        voices[voiceIndex].stop();
    }
    
    // Convert velocity to 0-127 range for AudioSynthWavetable
    int vel = constrain(velocity * 127.0f, 0, 127);
    
    // Start the note
    voices[voiceIndex].playNote(midiNote, vel);
    
    // Update voice state
    voiceStates[voiceIndex].active = true;
    voiceStates[voiceIndex].midiNote = midiNote;
    voiceStates[voiceIndex].noteOnTime = millis();
    
    Serial.print("SoundfontSynthesizer: Note ON - MIDI: ");
    Serial.print(midiNote);
    Serial.print(", velocity: ");
    Serial.print(vel);
    Serial.print(", voice: ");
    Serial.println(voiceIndex);
}

void SoundfontSynthesizer::noteOff(int midiNote) {
    // Find the voice playing this note
    int voiceIndex = findVoicePlayingNote(midiNote);
    if (voiceIndex != -1) {
        voices[voiceIndex].stop();
        voiceStates[voiceIndex].active = false;
        voiceStates[voiceIndex].midiNote = -1;
        
        Serial.print("SoundfontSynthesizer: Note OFF - MIDI: ");
        Serial.print(midiNote);
        Serial.print(", voice: ");
        Serial.println(voiceIndex);
    }
}

void SoundfontSynthesizer::allNotesOff() {
    for (int i = 0; i < MAX_VOICES; ++i) {
        voices[i].stop();
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
    }
    Serial.println("SoundfontSynthesizer: All notes off");
}

int SoundfontSynthesizer::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voiceStates[i].active) {
            count++;
        }
    }
    return count;
}

void SoundfontSynthesizer::setVolume(float vol) {
    volume = constrain(vol, 0.0f, 1.0f);
    updateMixerGains();
}

float SoundfontSynthesizer::getVolume() const {
    return volume;
}

AudioStream* SoundfontSynthesizer::getLeftOutput() {
    return &finalMixer;
}

AudioStream* SoundfontSynthesizer::getRightOutput() {
    return &finalMixer;  // Mono output, duplicated to stereo by parent
}

AudioSynthWavetable& SoundfontSynthesizer::getVoice(int index) {
    if (index >= 0 && index < MAX_VOICES) {
        return voices[index];
    }
    return voices[0];  // Fallback
}

int SoundfontSynthesizer::findAvailableVoice() {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voiceStates[i].active) {
            return i;
        }
    }
    return -1;  // No available voice
}

int SoundfontSynthesizer::findVoicePlayingNote(int midiNote) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voiceStates[i].active && voiceStates[i].midiNote == midiNote) {
            return i;
        }
    }
    return -1;  // Note not found
}

int SoundfontSynthesizer::findOldestVoice() {
    int oldestIndex = 0;
    unsigned long oldestTime = voiceStates[0].noteOnTime;
    
    for (int i = 1; i < MAX_VOICES; ++i) {
        if (voiceStates[i].noteOnTime < oldestTime) {
            oldestTime = voiceStates[i].noteOnTime;
            oldestIndex = i;
        }
    }
    
    return oldestIndex;
}

void SoundfontSynthesizer::updateMixerGains() {
    // Apply volume to all mixer channels
    for (int i = 0; i < 4; ++i) {
        mixer1.gain(i, volume);
        mixer2.gain(i, volume);
    }
    // Final mixer combines the two sub-mixers
    finalMixer.gain(0, 1.0f);
    finalMixer.gain(1, 1.0f);
    finalMixer.gain(2, 0.0f);
    finalMixer.gain(3, 0.0f);
}
