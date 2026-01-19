#include "SoundfontSynthesizer.h"
#include <Arduino.h>

SoundfontSynthesizer::SoundfontSynthesizer()
    : initialized(false)
    , volume(1.0f)
{
    // Initialize instrument connections
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instrumentConnections[i] = nullptr;
    }
    
    // Connect instruments to final mixer
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instrumentConnections[i] = new AudioConnection(*instruments[i].getOutput(), 0, finalMixer, i);
    }
    
    // Set initial mixer gains
    updateMixerGains();
}

SoundfontSynthesizer::~SoundfontSynthesizer() {
    // Clean up audio connections
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        delete instrumentConnections[i];
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
    
    Serial.print("SoundfontSynthesizer: Loading into slot ");
    Serial.println(instrumentSlot);
    
    bool success = instruments[instrumentSlot].loadInstrument(filename, instrumentIndex);
    
    if (success) {
        // Update mixer gains based on new instrument count
        updateMixerGains();
    }
    
    return success;
}

bool SoundfontSynthesizer::isInstrumentLoaded(int instrumentSlot) const {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return false;
    }
    return instruments[instrumentSlot].isLoaded();
}

const char* SoundfontSynthesizer::getInstrumentName(int instrumentSlot) const {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return "";
    }
    return instruments[instrumentSlot].getName();
}

void SoundfontSynthesizer::unloadInstrument(int instrumentSlot) {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return;
    }
    
    Serial.print("SoundfontSynthesizer: Unloading instrument from slot ");
    Serial.println(instrumentSlot);
    
    instruments[instrumentSlot].unload();
    
    // Update mixer gains based on new instrument count
    updateMixerGains();
}

void SoundfontSynthesizer::noteOn(int instrumentSlot, int midiNote, float velocity) {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return;
    }
    
    instruments[instrumentSlot].noteOn(midiNote, velocity);
}

void SoundfontSynthesizer::noteOff(int instrumentSlot, int midiNote) {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return;
    }
    
    instruments[instrumentSlot].noteOff(midiNote);
}

void SoundfontSynthesizer::allNotesOff(int instrumentSlot) {
    if (instrumentSlot == -1) {
        // All instruments
        for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
            instruments[i].allNotesOff();
        }
    } else if (instrumentSlot >= 0 && instrumentSlot < MAX_INSTRUMENTS) {
        instruments[instrumentSlot].allNotesOff();
    }
}

int SoundfontSynthesizer::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        count += instruments[i].getActiveVoiceCount();
    }
    return count;
}

void SoundfontSynthesizer::setVolume(float vol) {
    volume = constrain(vol, 0.0f, 1.0f);
    
    // Apply volume to each instrument
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setVolume(volume);
    }
}

float SoundfontSynthesizer::getVolume() const {
    return volume;
}

AudioStream* SoundfontSynthesizer::getLeftOutput() {
    return &finalMixer;
}

AudioStream* SoundfontSynthesizer::getRightOutput() {
    return &finalMixer;
}

void SoundfontSynthesizer::updateMixerGains() {
    // Count how many instruments are loaded
    int loadedCount = 0;
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        if (instruments[i].isLoaded()) {
            loadedCount++;
        }
    }
    
    // Calculate final mixer gain based on loaded instruments
    // This prevents overflow while maximizing volume
    float finalMixerGain = (loadedCount > 0) ? (1.0f / loadedCount) : 1.0f;
    
    // Final mixer combines all instrument outputs
    // Gain is adjusted based on number of loaded instruments
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        finalMixer.gain(i, finalMixerGain);
    }
}
