#pragma once

#include <Audio.h>
#include <AudioStream.h>
#include <SD.h>
#include <sf22aswt.h>

/**
 * SoundfontSynthesizer
 * 
 * Polyphonic synthesizer using SoundFont 2 (.sf2) files.
 * Utilizes the sf22aswt library to load and play SF2 instruments
 * with the Teensy AudioSynthWavetable.
 * 
 * Features:
 * - Multiple instrument support (up to MAX_INSTRUMENTS)
 * - Polyphonic playback with multiple voices
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
    
    // Audio outputs (mono output, duplicated to L/R by HybridSynthesizer)
    AudioStream* getLeftOutput();
    AudioStream* getRightOutput();
    
    // Direct voice access (for advanced use)
    AudioSynthWavetable& getVoice(int index);

private:
    static const int MAX_VOICES = 8;  // 8-voice polyphony
    static const int MAX_INSTRUMENTS = 4;  // Maximum number of instruments loaded simultaneously
    
    // Audio synthesis
    AudioSynthWavetable voices[MAX_VOICES];
    
    // Audio mixing
    AudioMixer4 mixer1;  // Mix voices 0-3
    AudioMixer4 mixer2;  // Mix voices 4-7
    AudioMixer4 finalMixer; // Combine mixer1 and mixer2
    
    // Audio connections
    AudioConnection* voiceConnections[MAX_VOICES];
    AudioConnection* mixerConnection;    // mixer1 to finalMixer
    AudioConnection* mixer2Connection;   // mixer2 to finalMixer
    
    // SoundFont readers - separate instance for each instrument slot
    SF22ASWTreader sf22aswt_readers[MAX_INSTRUMENTS];
    
    // Multiple instrument support
    struct InstrumentSlot {
        AudioSynthWavetable::instrument_data* data;
        bool loaded;
        char name[64];
        char filename[128];  // Track loaded filename for cloning
        int instrumentIndex; // Track instrument index within file
    };
    InstrumentSlot instruments[MAX_INSTRUMENTS];
    
    // State
    bool initialized;
    float volume;
    
    // Voice allocation with instrument tracking
    struct VoiceState {
        bool active;
        int instrumentSlot;
        int midiNote;
        unsigned long noteOnTime;
    };
    VoiceState voiceStates[MAX_VOICES];
    
    // Helper methods
    int findAvailableVoice();
    int findVoicePlayingNote(int instrumentSlot, int midiNote);
    int findOldestVoice();
    void updateMixerGains();
    void clearInstrumentSlot(int instrumentSlot);  // Clear instrument data safely
};
