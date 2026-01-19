#include "SoundfontInstrument.h"
#include <Arduino.h>

SoundfontInstrument::SoundfontInstrument()
    : instrumentData(nullptr)
    , loaded(false)
    , instrumentIndex(-1)
    , volume(1.0f)
{
    name[0] = '\0';
    filename[0] = '\0';
    
    // Initialize voice states
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
        voiceStates[i].noteOnTime = 0;
        voiceConnections[i] = nullptr;
    }
    
    // Connect voices to mixer
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        voiceConnections[i] = new AudioConnection(voices[i], 0, mixer, i);
    }
    
    // Set initial mixer gains
    updateMixerGains();
}

SoundfontInstrument::~SoundfontInstrument() {
    // Clean up audio connections
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        delete voiceConnections[i];
    }
    
    // Clean up instrument data
    if (instrumentData != nullptr) {
        delete instrumentData;
        instrumentData = nullptr;
    }
}

bool SoundfontInstrument::loadInstrument(const char* filename, int instrumentIndex) {
    Serial.print("SoundfontInstrument: Loading ");
    Serial.print(filename);
    Serial.print("[");
    Serial.print(instrumentIndex);
    Serial.println("]");
    
    // Ensure filename starts with "/" for SD library compatibility
    char fullPath[256];
    if (filename[0] != '/') {
        snprintf(fullPath, sizeof(fullPath), "/%s", filename);
    } else {
        strncpy(fullPath, filename, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
    }
    
    // Check if file exists
    if (!SD.exists(fullPath)) {
        Serial.print("SoundfontInstrument: File not found: ");
        Serial.println(fullPath);
        return false;
    }
    
    // Store old instrument data to delete after switching
    AudioSynthWavetable::instrument_data* oldInstrument = instrumentData;
    
    // Read and load instrument
    bool success = false;
    if (sf2Reader.ReadFile(fullPath)) {
        success = sf2Reader.Load_instrument(instrumentIndex, instrumentData);
    } else {
        Serial.println("SoundfontInstrument: Failed to read SF2 file");
    }
    
    if (!success) {
        Serial.println("SoundfontInstrument: Failed to load instrument");
        instrumentData = oldInstrument;  // Restore old data
        return false;
    }
    
    // Clean up old instrument data
    if (oldInstrument != nullptr) {
        delete oldInstrument;
    }
    
    // Store instrument metadata
    snprintf(name, sizeof(name), "%s[%d]", filename, instrumentIndex);
    strncpy(this->filename, fullPath, sizeof(this->filename) - 1);
    this->filename[sizeof(this->filename) - 1] = '\0';
    this->instrumentIndex = instrumentIndex;
    loaded = true;
    
    Serial.println("SoundfontInstrument: Instrument loaded successfully");
    return true;
}

bool SoundfontInstrument::isLoaded() const {
    return loaded;
}

const char* SoundfontInstrument::getName() const {
    return name;
}

void SoundfontInstrument::unload() {
    Serial.println("SoundfontInstrument: Unloading instrument");
    
    // Stop all voices
    allNotesOff();
    
    // Clean up instrument data
    if (instrumentData != nullptr) {
        delete instrumentData;
        instrumentData = nullptr;
    }
    
    // Clear metadata
    loaded = false;
    name[0] = '\0';
    filename[0] = '\0';
    instrumentIndex = -1;
}

void SoundfontInstrument::noteOn(int midiNote, float velocity) {
    if (!loaded) {
        return;
    }
    
    // Find an available voice
    int voiceIndex = findAvailableVoice();
    if (voiceIndex == -1) {
        // No free voice, steal the oldest one
        voiceIndex = findOldestVoice();
        voices[voiceIndex].stop();
    }
    
    // Set the instrument on this voice
    voices[voiceIndex].setInstrument(*instrumentData);
    
    // Convert velocity to 0-127 range for AudioSynthWavetable
    int vel = constrain(velocity * 127.0f, 0, 127);
    
    // Start the note
    voices[voiceIndex].playNote(midiNote, vel);
    
    // Update voice state
    voiceStates[voiceIndex].active = true;
    voiceStates[voiceIndex].midiNote = midiNote;
    voiceStates[voiceIndex].noteOnTime = millis();
}

void SoundfontInstrument::noteOff(int midiNote) {
    // Find the voice playing this note
    int voiceIndex = findVoicePlayingNote(midiNote);
    if (voiceIndex != -1) {
        voices[voiceIndex].stop();
        voiceStates[voiceIndex].active = false;
        voiceStates[voiceIndex].midiNote = -1;
    }
}

void SoundfontInstrument::allNotesOff() {
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        voices[i].stop();
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
    }
}

int SoundfontInstrument::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        if (voiceStates[i].active) {
            count++;
        }
    }
    return count;
}

void SoundfontInstrument::setVolume(float vol) {
    volume = constrain(vol, 0.0f, 1.0f);
    updateMixerGains();
}

float SoundfontInstrument::getVolume() const {
    return volume;
}

AudioStream* SoundfontInstrument::getOutput() {
    return &mixer;
}

int SoundfontInstrument::findAvailableVoice() {
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        if (!voiceStates[i].active) {
            return i;
        }
    }
    return -1;
}

int SoundfontInstrument::findVoicePlayingNote(int midiNote) {
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        if (voiceStates[i].active && voiceStates[i].midiNote == midiNote) {
            return i;
        }
    }
    return -1;
}

int SoundfontInstrument::findOldestVoice() {
    int oldestIndex = 0;
    unsigned long oldestTime = voiceStates[0].noteOnTime;
    
    for (int i = 1; i < VOICES_PER_INSTRUMENT; ++i) {
        if (voiceStates[i].noteOnTime < oldestTime) {
            oldestTime = voiceStates[i].noteOnTime;
            oldestIndex = i;
        }
    }
    
    return oldestIndex;
}

void SoundfontInstrument::updateMixerGains() {
    // Apply volume to all mixer channels (4 voices max)
    // Use 0.25 gain per voice to prevent overflow when all 4 are active
    for (int i = 0; i < VOICES_PER_INSTRUMENT; ++i) {
        mixer.gain(i, volume * 0.25f);
    }
}
