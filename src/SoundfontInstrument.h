#pragma once

#include <Audio.h>
#include <AudioStream.h>
#include <SD.h>
#include <sf22aswt.h>

/**
 * SoundfontInstrument
 * 
 * Encapsulates a single soundfont instrument with its own:
 * - 4 wavetable voices
 * - SF2 reader
 * - Instrument data
 * - Voice mixer
 * 
 * Provides an AudioStream output that can be connected to other mixers.
 */
class SoundfontInstrument {
public:
    SoundfontInstrument();
    ~SoundfontInstrument();

    // Instrument loading
    bool loadInstrument(const char* filename, int instrumentIndex);
    bool isLoaded() const;
    const char* getName() const;
    void unload();
    
    // Note control - 4-voice polyphony
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    int getActiveVoiceCount() const;
    
    // Volume control
    void setVolume(float volume);  // 0.0 - 1.0
    float getVolume() const;
    
    // Audio output
    AudioStream* getOutput();

private:
    static const int VOICES_PER_INSTRUMENT = 4;
    
    // Audio synthesis
    AudioSynthWavetable voices[VOICES_PER_INSTRUMENT];
    
    // Audio mixing - combines 4 voices
    AudioMixer4 mixer;
    
    // Audio connections
    AudioConnection* voiceConnections[VOICES_PER_INSTRUMENT];
    
    // SoundFont reader
    SF22ASWTreader sf2Reader;
    
    // Instrument data
    AudioSynthWavetable::instrument_data* instrumentData;
    bool loaded;
    char name[64];
    char filename[128];
    int instrumentIndex;
    
    // Voice allocation
    struct VoiceState {
        bool active;
        int midiNote;
        unsigned long noteOnTime;
    };
    VoiceState voiceStates[VOICES_PER_INSTRUMENT];
    
    // Volume
    float volume;
    
    // Helper methods
    int findAvailableVoice();
    int findVoicePlayingNote(int midiNote);
    int findOldestVoice();
    void updateMixerGains();
};
