#pragma once

#include <Audio.h>
#include <AudioStream.h>

/**
 * SoundfontPadSynthesizer
 *
 * Lean SF2 renderer for pre-shaped synth-source samples (e.g. the Whitesnake VS pad).
 * Audio chain per voice:  AudioSynthWavetable -> AudioEffectEnvelope -> mixer
 *
 * 8 voices for note overlap during long releases. No filter, no crossfade.
 *
 * Does NOT load SF2 files itself — borrows an instrument_data pointer from
 * SoundfontSynthesizer so that the heavy reader machinery (PSRAM allocation,
 * sample buffer ownership, the global samples_usedRam budget) lives in one place.
 * Caller is responsible for keeping the pointed-to instrument_data alive.
 */
class SoundfontPadSynthesizer {
public:
    static const int NUM_VOICES = 8;

    SoundfontPadSynthesizer();
    ~SoundfontPadSynthesizer();

    void setInstrumentData(AudioSynthWavetable::instrument_data* data);
    bool isLoaded() const { return instrumentData != nullptr; }

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    int getActiveVoiceCount() const;

    void setVolume(float volume);

    void setAttack(float milliseconds);
    void setDecay(float milliseconds);
    void setSustain(float level);
    void setRelease(float milliseconds);
    void setADSR(float attack, float decay, float sustain, float release);

    AudioStream* getOutput();

private:
    AudioSynthWavetable voices[NUM_VOICES];
    AudioEffectEnvelope envelopes[NUM_VOICES];

    // 8 voices -> 2 stage-1 mixers (4 inputs each) -> 1 final mixer (2 of 4 used)
    AudioMixer4 mixerA;       // voices 0-3
    AudioMixer4 mixerB;       // voices 4-7
    AudioMixer4 finalMixer;   // mixerA + mixerB

    AudioConnection* voiceToEnvelope[NUM_VOICES];
    AudioConnection* envelopeToMixer[NUM_VOICES];
    AudioConnection* mixerAToFinal;
    AudioConnection* mixerBToFinal;

    AudioSynthWavetable::instrument_data* instrumentData;  // borrowed, NOT owned

    struct VoiceState {
        bool active;
        int midiNote;
        unsigned long noteOnTime;
    };
    VoiceState voiceStates[NUM_VOICES];

    float volume;
    float attackMs;
    float decayMs;
    float sustainLevel;
    float releaseMs;

    int findAvailableVoice();
    int findVoicePlayingNote(int midiNote);
    int findOldestVoice();
    void updateMixerGains();
};
