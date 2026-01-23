#pragma once

#include <Audio.h>
#include <AudioStream.h>
#include <SD.h>
#include <sf22aswt.h>
#include "DelayedFader.h"

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
    
    // ADSR envelope control
    void setAttack(float milliseconds);   // Attack time in ms
    void setDecay(float milliseconds);    // Decay time in ms
    void setSustain(float level);         // Sustain level 0.0-1.0
    void setRelease(float milliseconds);  // Release time in ms
    void setADSR(float attack, float decay, float sustain, float release);
    
    // Filter control
    void setFilterMultiplier(float multiplier);  // Multiplier of note frequency (0.5 - 20.0)
    void setFilterResonance(float q);            // Q factor (0.7 - 5.0)
    
    // Crossfade control
    void setCrossfadeDuration(float milliseconds);  // Duration of filter crossfade
    
    // Audio output
    AudioStream* getOutput();

private:
    static const int VOICES_PER_INSTRUMENT = 4;
    
    // Audio synthesis
    AudioSynthWavetable voices[VOICES_PER_INSTRUMENT];
    
    // ADSR envelopes - one per voice
    AudioEffectEnvelope envelopes[VOICES_PER_INSTRUMENT];
    
    // Low-pass filters - one per voice
    AudioFilterStateVariable filters[VOICES_PER_INSTRUMENT];
    
    // Delayed faders for crossfade - one pair per voice
    DelayedFader filteredFaders[VOICES_PER_INSTRUMENT];    // Fade in filtered signal
    DelayedFader unfilteredFaders[VOICES_PER_INSTRUMENT];  // Fade out unfiltered signal
    
    // Crossfade mixers - one per voice (combines filtered + unfiltered)
    AudioMixer4 crossfadeMixers[VOICES_PER_INSTRUMENT];
    
    // Final voice mixer - combines all 4 voices
    AudioMixer4 mixer;
    
    // Audio connections: 
    // voice → envelope → filter → filteredFader → crossfadeMixer[0]
    //                  → unfilteredFader → crossfadeMixer[1]
    //                     crossfadeMixer → mixer
    AudioConnection* voiceConnections[VOICES_PER_INSTRUMENT];
    AudioConnection* envelopeToFilterConnections[VOICES_PER_INSTRUMENT];
    AudioConnection* envelopeToUnfilteredConnections[VOICES_PER_INSTRUMENT];
    AudioConnection* filterToFaderConnections[VOICES_PER_INSTRUMENT];
    AudioConnection* filteredToMixerConnections[VOICES_PER_INSTRUMENT];
    AudioConnection* unfilteredToMixerConnections[VOICES_PER_INSTRUMENT];
    AudioConnection* crossfadeToFinalConnections[VOICES_PER_INSTRUMENT];
    
    // SoundFont reader
    SF22ASWTreader sf2Reader;
    
    // Instrument data
    AudioSynthWavetable::instrument_data* instrumentData;
    int* centsOffsets;  // Store original CENTS_OFFSET for each sample (for debugging)
    int sampleCount;
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
    
    // ADSR parameters (in milliseconds and 0.0-1.0 for sustain)
    float attackMs;
    float decayMs;
    float sustainLevel;
    float releaseMs;
    
    // Filter parameters
    float filterMultiplier;  // Multiplier of note frequency
    float filterResonance;   // Q factor
    
    // Crossfade parameters
    float crossfadeDurationMs;  // Duration of filter crossfade in ms
    
    // Helper methods
    void updateFaderDelays();  // Update fader delays based on ADSR attack+decay
    int findAvailableVoice();
    int findVoicePlayingNote(int midiNote);
    int findOldestVoice();
    void updateMixerGains();
};
