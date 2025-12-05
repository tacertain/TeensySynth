#include "SoundfontSynthesizer.h"
#include <Arduino.h>

SoundfontSynthesizer::SoundfontSynthesizer()
    : mixerConnection(nullptr)
    , mixer2Connection(nullptr)
    , initialized(false)
    , volume(1.0f)
{
    // Initialize instrument slots
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].data = nullptr;
        instruments[i].loaded = false;
        instruments[i].name[0] = '\0';
        instruments[i].filename[0] = '\0';
        instruments[i].instrumentIndex = -1;
    }
    
    // Initialize voice states
    for (int i = 0; i < MAX_VOICES; ++i) {
        voiceStates[i].active = false;
        voiceStates[i].instrumentSlot = -1;
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
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        if (instruments[i].data != nullptr) {
            delete instruments[i].data;
            instruments[i].data = nullptr;
        }
    }
}

bool SoundfontSynthesizer::begin() {
    if (initialized) {
        return true;
    }
    
    Serial.println("SoundfontSynthesizer: Initializing...");
    
    // Check if SD card is initialized
    if (!SD.begin(BUILTIN_SDCARD)) {
        Serial.println("SoundfontSynthesizer: SD card initialization failed!");
        return false;
    }
    
    // Test SD card read access
    File testFile = SD.open("/");
    if (!testFile) {
        Serial.println("SoundfontSynthesizer: Cannot open SD root directory!");
        return false;
    }
    testFile.close();
    
    initialized = true;
    Serial.println("SoundfontSynthesizer: Initialized");
    return true;
}

bool SoundfontSynthesizer::loadInstrument(int instrumentSlot, const char* filename, int instrumentIndex) {
    if (!initialized) {
        Serial.println("SoundfontSynthesizer: Not initialized! Call begin() first.");
        return false;
    }
    
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        Serial.println("SoundfontSynthesizer: Invalid instrument slot");
        return false;
    }
    
    Serial.print("SoundfontSynthesizer: Loading ");
    Serial.print(filename);
    Serial.print("[");
    Serial.print(instrumentIndex);
    Serial.print("] into slot ");
    Serial.println(instrumentSlot);
    
    // Ensure filename starts with "/" for SD library compatibility
    char fullPath[256];
    if (filename[0] != '/') {
        snprintf(fullPath, sizeof(fullPath), "/%s", filename);
    } else {
        strncpy(fullPath, filename, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';  // Ensure null termination
    }
    
    // Check if file exists
    if (!SD.exists(fullPath)) {
        Serial.print("SoundfontSynthesizer: File not found: ");
        Serial.println(fullPath);
        return false;
    }
    
    // Store old instrument data to delete after switching
    AudioSynthWavetable::instrument_data* oldInstrument = instruments[instrumentSlot].data;
    
    // Check if we can clone from an existing reader with the same file
    int sourceSlot = -1;
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        if (i != instrumentSlot && instruments[i].loaded && 
            strcmp(instruments[i].filename, fullPath) == 0) {
            sourceSlot = i;
            Serial.print("SoundfontSynthesizer: Found existing file in slot ");
            Serial.print(i);
            Serial.println(", cloning reader...");
            break;
        }
    }
    
    bool success = false;
    if (sourceSlot != -1) {
        // Clone from existing reader
        if (sf22aswt_readers[sourceSlot].CloneInto(sf22aswt_readers[instrumentSlot])) {
            success = sf22aswt_readers[instrumentSlot].Load_instrument(instrumentIndex, instruments[instrumentSlot].data);
        } else {
            Serial.println("SoundfontSynthesizer: Failed to clone reader");
        }
    } else {
        // Read file fresh
        Serial.println("SoundfontSynthesizer: Reading file fresh...");
        if (sf22aswt_readers[instrumentSlot].ReadFile(fullPath)) {
            success = sf22aswt_readers[instrumentSlot].Load_instrument(instrumentIndex, instruments[instrumentSlot].data);
        } else {
            Serial.println("SoundfontSynthesizer: Failed to read SF2 file");
        }
    }
    
    if (!success) {
        Serial.println("SoundfontSynthesizer: Failed to load instrument");
        return false;
    }
    
    // Clean up old instrument data
    if (oldInstrument != nullptr) {
        delete oldInstrument;
    }
    
    // Store instrument metadata and mark as loaded
    snprintf(instruments[instrumentSlot].name, sizeof(instruments[instrumentSlot].name), "%s[%d]", filename, instrumentIndex);
    strncpy(instruments[instrumentSlot].filename, fullPath, sizeof(instruments[instrumentSlot].filename) - 1);
    instruments[instrumentSlot].filename[sizeof(instruments[instrumentSlot].filename) - 1] = '\0';
    instruments[instrumentSlot].instrumentIndex = instrumentIndex;
    instruments[instrumentSlot].loaded = true;
    
    Serial.println("SoundfontSynthesizer: Instrument loaded successfully");
    return true;
}

bool SoundfontSynthesizer::isInstrumentLoaded(int instrumentSlot) const {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return false;
    }
    return instruments[instrumentSlot].loaded;
}

const char* SoundfontSynthesizer::getInstrumentName(int instrumentSlot) const {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return "";
    }
    return instruments[instrumentSlot].name;
}

void SoundfontSynthesizer::unloadInstrument(int instrumentSlot) {
    Serial.print("SoundfontSynthesizer: Unloading instrument from slot ");
    Serial.println(instrumentSlot);
    clearInstrumentSlot(instrumentSlot);
}

void SoundfontSynthesizer::noteOn(int instrumentSlot, int midiNote, float velocity) {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS || !instruments[instrumentSlot].loaded) {
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
    voices[voiceIndex].setInstrument(*instruments[instrumentSlot].data);
    
    // Convert velocity to 0-127 range for AudioSynthWavetable
    int vel = constrain(velocity * 127.0f, 0, 127);
    
    // Start the note
    voices[voiceIndex].playNote(midiNote, vel);
    
    // Update voice state
    voiceStates[voiceIndex].active = true;
    voiceStates[voiceIndex].instrumentSlot = instrumentSlot;
    voiceStates[voiceIndex].midiNote = midiNote;
    voiceStates[voiceIndex].noteOnTime = millis();
}

void SoundfontSynthesizer::noteOff(int instrumentSlot, int midiNote) {
    // Find the voice playing this note for this instrument
    int voiceIndex = findVoicePlayingNote(instrumentSlot, midiNote);
    if (voiceIndex != -1) {
        voices[voiceIndex].stop();
        voiceStates[voiceIndex].active = false;
        voiceStates[voiceIndex].instrumentSlot = -1;
        voiceStates[voiceIndex].midiNote = -1;
    }
}

void SoundfontSynthesizer::allNotesOff(int instrumentSlot) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (instrumentSlot == -1 || voiceStates[i].instrumentSlot == instrumentSlot) {
            voices[i].stop();
            voiceStates[i].active = false;
            voiceStates[i].instrumentSlot = -1;
            voiceStates[i].midiNote = -1;
        }
    }
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

int SoundfontSynthesizer::findVoicePlayingNote(int instrumentSlot, int midiNote) {
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voiceStates[i].active && 
            voiceStates[i].instrumentSlot == instrumentSlot && 
            voiceStates[i].midiNote == midiNote) {
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

void SoundfontSynthesizer::clearInstrumentSlot(int instrumentSlot) {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return;
    }
    
    // Stop any voices using this instrument
    allNotesOff(instrumentSlot);
    
    // Clean up instrument data
    if (instruments[instrumentSlot].data != nullptr) {
        delete instruments[instrumentSlot].data;
        instruments[instrumentSlot].data = nullptr;
    }
    
    // Clear metadata
    instruments[instrumentSlot].loaded = false;
    instruments[instrumentSlot].name[0] = '\0';
    instruments[instrumentSlot].filename[0] = '\0';
    instruments[instrumentSlot].instrumentIndex = -1;
}
