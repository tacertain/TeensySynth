#pragma once

#include <Audio.h>
#include <AudioStream.h>
#include <SD.h>
#include "SoundfontInstrument.h"

/**
 * SoundfontSynthesizer
 * 
 * Manages multiple SoundfontInstrument instances (up to MAX_INSTRUMENTS).
 * Each instrument has 4 voices and its own mixer.
 * All instruments are combined in the final mixer.
 * 
 * Features:
 * - Multiple instrument support (up to MAX_INSTRUMENTS)
 * - Each instrument has 4-voice polyphony
 * - Dynamic instrument loading from SD card
 * - Stereo output
 * - Volume control
 */
class SoundfontSynthesizer {
public:
    SoundfontSynthesizer();
    ~SoundfontSynthesizer();

    // Initialization
    bool begin();  // Initialize SD card and prepare for loading
    bool loadInstrument(int instrumentSlot, const char* filename, int instrumentIndex);
    bool isInstrumentLoaded(int instrumentSlot) const;
    const char* getInstrumentName(int instrumentSlot) const;
    void unloadInstrument(int instrumentSlot);  // Unload instrument and free memory
    
    // Note control - polyphonic with instrument selection
    void noteOn(int instrumentSlot, int midiNote, float velocity);
    void noteOff(int instrumentSlot, int midiNote);
    void allNotesOff(int instrumentSlot = -1);  // -1 = all instruments
    int getActiveVoiceCount() const;
    
    // Volume control
    void setVolume(float volume);  // 0.0 - 1.0
    float getVolume() const;
    
    // ADSR envelope control (applies to all loaded instruments)
    void setAttack(float milliseconds);
    void setDecay(float milliseconds);
    void setSustain(float level);  // 0.0-1.0
    void setRelease(float milliseconds);
    
    // Filter control (applies to all loaded instruments)
    void setFilterFrequency(float frequency);  // Hz
    void setFilterResonance(float q);          // Q factor
    
    // Audio outputs (mono output, duplicated to L/R by HybridSynthesizer)
    AudioStream* getLeftOutput();
    AudioStream* getRightOutput();

private:
    static const int MAX_INSTRUMENTS = 4;  // Maximum number of instruments
    
    // Instrument instances
    SoundfontInstrument instruments[MAX_INSTRUMENTS];
    
    // Final mixer - combines all instruments
    AudioMixer4 finalMixer;
    
    // Audio connections - instrument outputs to final mixer
    AudioConnection* instrumentConnections[MAX_INSTRUMENTS];
    
    // State
    bool initialized;
    float volume;
    
    // Helper methods
    void updateMixerGains();
};
