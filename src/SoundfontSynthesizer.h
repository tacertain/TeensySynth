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
    bool loadInstrument(const char* filename, int instrumentIndex);
    bool isInstrumentLoaded() const;
    const char* getCurrentInstrumentName() const;
    
    // Note control - polyphonic
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
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
    
    // SoundFont reader
    SF22ASWTreader sf22aswt;
    AudioSynthWavetable::instrument_data* instrumentData;
    
    // State
    bool initialized;
    bool instrumentLoaded;
    char currentInstrumentName[64];
    float volume;
    
    // Voice allocation
    struct VoiceState {
        bool active;
        int midiNote;
        unsigned long noteOnTime;
    };
    VoiceState voiceStates[MAX_VOICES];
    
    // Helper methods
    int findAvailableVoice();
    int findVoicePlayingNote(int midiNote);
    int findOldestVoice();
    void updateMixerGains();
};
