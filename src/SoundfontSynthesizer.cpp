#include "SoundfontSynthesizer.h"
#include <Arduino.h>

SoundfontSynthesizer::SoundfontSynthesizer()
    : outputPeakConnection(nullptr)
    , initialized(false)
    , volume(1.0f)
{
    // Initialize instrument connections
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instrumentConnections[i] = nullptr;
        peakMonitorConnections[i] = nullptr;
    }
    
    // Connect instruments to final mixer
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instrumentConnections[i] = new AudioConnection(*instruments[i].getOutput(), 0, finalMixer, i);
        // Connect instrument output to peak monitor (parallel path for monitoring)
        peakMonitorConnections[i] = new AudioConnection(*instruments[i].getOutput(), 0, instrumentPeakMonitors[i], 0);
    }
    
    // Connect finalMixer output to output peak monitor
    outputPeakConnection = new AudioConnection(finalMixer, 0, outputPeakMonitor, 0);
    
    // Set initial mixer gains
    updateMixerGains();
}

SoundfontSynthesizer::~SoundfontSynthesizer() {
    // Clean up audio connections
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        delete instrumentConnections[i];
        delete peakMonitorConnections[i];
    }
    delete outputPeakConnection;
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

AudioSynthWavetable::instrument_data* SoundfontSynthesizer::getInstrumentData(int instrumentSlot) const {
    if (instrumentSlot < 0 || instrumentSlot >= MAX_INSTRUMENTS) {
        return nullptr;
    }
    return instruments[instrumentSlot].getInstrumentData();
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

void SoundfontSynthesizer::setAttack(float milliseconds) {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setAttack(milliseconds);
    }
}

void SoundfontSynthesizer::setDecay(float milliseconds) {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setDecay(milliseconds);
    }
}

void SoundfontSynthesizer::setSustain(float level) {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setSustain(level);
    }
}

void SoundfontSynthesizer::setRelease(float milliseconds) {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setRelease(milliseconds);
    }
}

void SoundfontSynthesizer::setFilterMultiplier(float multiplier) {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setFilterMultiplier(multiplier);
    }
}

void SoundfontSynthesizer::setFilterResonance(float q) {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setFilterResonance(q);
    }
}

void SoundfontSynthesizer::setCrossfadeDuration(float milliseconds) {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instruments[i].setCrossfadeDuration(milliseconds);
    }
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

void SoundfontSynthesizer::printPeakLevels() {
    Serial.println("=== SoundfontSynthesizer Peak Levels ===");
    
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        Serial.print("  Instrument ");
        Serial.print(i);
        Serial.print(" (");
        Serial.print(instruments[i].isLoaded() ? instruments[i].getName() : "empty");
        Serial.print("): min=");
        Serial.print(instrumentPeakMonitors[i].getMin());
        Serial.print(" max=");
        Serial.println(instrumentPeakMonitors[i].getMax());
    }
    
    Serial.print("  FinalMixer output: min=");
    Serial.print(outputPeakMonitor.getMin());
    Serial.print(" max=");
    Serial.println(outputPeakMonitor.getMax());
    
    Serial.println("========================================");
}

void SoundfontSynthesizer::resetPeakMonitors() {
    for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
        instrumentPeakMonitors[i].reset();
    }
    outputPeakMonitor.reset();
}
