#pragma once

#include <Audio.h>
#include <AudioStream.h>

/**
 * SoundfontPadSynthesizer
 *
 * Lean SF2 renderer for pre-shaped synth-source samples (e.g. the Whitesnake VS pad).
 * Each voice contains a main wavetable (plays the pressed note) and a sub
 * wavetable (plays one octave down), summed by a per-voice mixer:
 *
 *     mainWT --gain(0)=1.0----+
 *                              +--> voiceMixer --> lpfVoice --> envelope --> stage-1 mixer
 *     subWT  --gain(1)=mix----+        ^
 *                                       | (octave-control mod input)
 *                              free-running per-voice sine LFO
 *
 * The sub gain is set via setOctaveMix() (CC 25 in WHITESNAKE) and takes
 * effect live on currently-sounding voices.
 *
 * 8 voices for note overlap during long releases.
 *
 * Path B post-FX (mono, sits after finalMixer):
 *
 *     finalMixer -+----------> wetDryMixer.0 (dry, fixed 1.0)
 *                 |
 *                 +--> chorusA -+
 *                 |             +--> chorusBus -+--> wetDryMixer.1 (chorus wet)
 *                 +--> chorusB -+               |
 *                 |                             +--+
 *                 +------------> preReverbMixer.0  |
 *                                preReverbMixer.1 <+
 *                                       |
 *                                       v
 *                                 reverbPost (Freeverb) --> wetDryMixer.2 (reverb wet)
 *
 * Does NOT load SF2 files itself — borrows an instrument_data pointer from
 * SoundfontSynthesizer so that the heavy reader machinery (PSRAM allocation,
 * sample buffer ownership, the global samples_usedRam budget) lives in one place.
 * Caller is responsible for keeping the pointed-to instrument_data alive.
 */
class SoundfontPadSynthesizer {
public:
    static const int NUM_VOICES = 8;

    // 2048 samples (~46 ms @ 44.1 kHz). Comfortable headroom above the 30 ms
    // max offset+depth this class spec's for the chorus flanges.
    static const int CHORUS_BUF_LEN = 2048;

    SoundfontPadSynthesizer();
    ~SoundfontPadSynthesizer();

    void setInstrumentData(AudioSynthWavetable::instrument_data* data);
    bool isLoaded() const { return instrumentData != nullptr; }

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);
    void allNotesOff();
    int getActiveVoiceCount() const;

    void setVolume(float volume);

    // 0.0 = no sub-octave, 1.0 = sub at same gain as main. Updates live.
    void setOctaveMix(float mix);
    float getOctaveMix() const { return octaveMix; }

    void setAttack(float milliseconds);
    void setDecay(float milliseconds);
    void setSustain(float level);
    void setRelease(float milliseconds);
    void setADSR(float attack, float decay, float sustain, float release);

    // Linear floor of the velocity curve before squaring. 0.0 = full
    // dynamic range (silent at minimum velocity); 1.0 = flat (no velocity
    // dynamics). Default 0.25 -> amp floor 0.0625 -> 24 dB range.
    void setVelocityFloor(float floor);
    float getVelocityFloor() const { return velocityFloor; }

    // Highpass branch on the main signal. Cutoff = noteHz * hpMultiplier,
    // set at each voice's noteOn and re-pushed to sounding voices when the
    // multiplier changes. Range [1.0, 4.0]. Default 3.1 (matches CC 27 = 89).
    void setHighpassMultiplier(float m);
    float getHighpassMultiplier() const { return hpMultiplier; }

    // Mix level of the highpassed branch into the voice mixer. Range [0.0, 2.0].
    // Default 1.0 (HP branch summed at unity with the dry main).
    void setHighpassMix(float m);
    float getHighpassMix() const { return hpMix; }

    // Live expression multiplier (0..1, default 1.0) applied on top of the
    // per-chord velocity amplitude to every sounding voice. Intended for the
    // expression pedal / aftertouch swell. Updates currently-playing voices.
    void setExpression(float e);
    float getExpression() const { return expression; }

    // Set the amplitude of every currently-held (key-down) voice from a MIDI
    // velocity (0..1 float). Released-but-sounding voices keep their level.
    // Called per chord with the smoother's blended velocity so held chords
    // track the evolving level.
    void setHeldLevel(float velocity);

    // ----- Path B: per-voice LP filter + free-running LFO -----
    // Base LP cutoff = noteHz * lpMultiplier, set at each voice's noteOn and
    // re-pushed to sounding voices when the multiplier changes. The per-voice
    // LFO modulates the cutoff via the filter's frequency-mod input, scaled
    // by octaveControl (built-in constant 0.6 octaves).
    void setPadLpMultiplier(float m);
    float getPadLpMultiplier() const { return lpMultiplier; }

    // Filter key-tracking exponent applied ONLY below middle C. 1.0 = full
    // proportional tracking (cutoff = noteHz * lpMultiplier, the original
    // behavior, which darkens the low end). Lower values lift the bass: the
    // cutoff falls more slowly per octave below middle C. 0.0 = flat (every
    // note below middle C gets middle C's cutoff). At/above middle C the
    // cutoff is unchanged regardless of this value.
    void setPadLpKeyTrack(float k);
    float getPadLpKeyTrack() const { return lpKeyTrack; }

    // LP filter Q, applied to every voice's lpfVoice. Range [0.7, 4.0].
    void setPadLpResonance(float q);
    float getPadLpResonance() const { return lpResonance; }

    // Per-voice LFO rate (Hz). Same rate on all voices; per-voice phase stays
    // randomized at construction so chords decorrelate.
    void setPadLpLfoRate(float hz);
    float getPadLpLfoRate() const { return lpLfoRateHz; }

    // Per-voice LFO amplitude. 0 = filter is static; 1 = full ±octaveControl swing.
    void setPadLpLfoDepth(float depth);
    float getPadLpLfoDepth() const { return lpLfoDepth; }

    // ----- Path B: post-mix chorus + reverb chain -----
    // Wet level of the chorus bus into wetDryMixer. Range [0.0, 1.0].
    void setChorusMix(float m);
    float getChorusMix() const { return chorusMix; }

    // Single fraction in [0.0, 1.0] that scales both flanges' delay_depth
    // around their defaults (fraction 0.5 ≈ default 132/176 samples; 1.0 ≈ 264/352).
    // Re-inits both flanges via voices(); resets their LFO phase and the
    // circular buffer index, so expect a click on each call. Acceptable for
    // sound-design tweaking, not for live sweeps.
    void setChorusDepth(float fraction);
    float getChorusDepth() const { return chorusDepthFraction; }

    // Wet level of the Freeverb tap into wetDryMixer. Range [0.0, 1.0].
    void setReverbMix(float m);
    float getReverbMix() const { return reverbMix; }

    // Freeverb roomsize. Range [0.0, 1.0].
    void setReverbRoomSize(float s);
    float getReverbRoomSize() const { return reverbRoomSize; }

    AudioStream* getOutput();

private:
    AudioSynthWavetable mainVoices[NUM_VOICES];
    AudioSynthWavetable subVoices[NUM_VOICES];
    AudioFilterStateVariable hpFilters[NUM_VOICES]; // 12 dB/oct HP per voice on main
    AudioMixer4 voiceMixers[NUM_VOICES];      // ch0=main dry, ch1=sub, ch2=main HP, ch3 unused
    AudioFilterStateVariable lpfVoice[NUM_VOICES];  // post-mix per-voice LP with LFO mod
    AudioSynthWaveform lfoVoice[NUM_VOICES];        // free-running sine, randomized phase per voice
    AudioEffectEnvelope envelopes[NUM_VOICES];

    // 8 voices -> 2 stage-1 mixers (4 inputs each) -> 1 final mixer (2 of 4 used)
    AudioMixer4 mixerA;       // voices 0-3
    AudioMixer4 mixerB;       // voices 4-7
    AudioMixer4 finalMixer;   // mixerA + mixerB

    // Path B post-FX
    AudioEffectFlange chorusA;
    AudioEffectFlange chorusB;
    AudioMixer4 chorusBus;          // sums chorusA + chorusB
    AudioMixer4 preReverbMixer;     // sums dry + chorus into reverb input
    AudioEffectFreeverb reverbPost;
    AudioMixer4 wetDryMixer;        // ch0=dry, ch1=chorus wet, ch2=reverb wet
    short chorusBufA[CHORUS_BUF_LEN];
    short chorusBufB[CHORUS_BUF_LEN];

    AudioConnection* mainToVoiceMixer[NUM_VOICES];
    AudioConnection* mainToHpFilter[NUM_VOICES];
    AudioConnection* hpFilterToVoiceMixer[NUM_VOICES];
    AudioConnection* subToVoiceMixer[NUM_VOICES];
    AudioConnection* voiceMixerToLpf[NUM_VOICES];  // (was voiceMixerToEnvelope)
    AudioConnection* lfoToLpf[NUM_VOICES];
    AudioConnection* lpfToEnvelope[NUM_VOICES];
    AudioConnection* envelopeToMixer[NUM_VOICES];
    AudioConnection* mixerAToFinal;
    AudioConnection* mixerBToFinal;

    // Post-FX cords
    AudioConnection* finalToDry;
    AudioConnection* finalToChorusA;
    AudioConnection* finalToChorusB;
    AudioConnection* chorusAToBus;
    AudioConnection* chorusBToBus;
    AudioConnection* finalToChorusBusDryCancel;  // -dry, cancels AudioEffectFlange's 50% dry bleed
    AudioConnection* chorusBusToWetDry;
    AudioConnection* finalToPreReverb;
    AudioConnection* chorusBusToPreReverb;
    AudioConnection* preReverbToReverb;
    AudioConnection* reverbToWetDry;

    AudioSynthWavetable::instrument_data* instrumentData;  // borrowed, NOT owned

    struct VoiceState {
        bool active;
        int midiNote;
        unsigned long noteOnTime;
    };
    VoiceState voiceStates[NUM_VOICES];

    float volume;
    float octaveMix;
    float attackMs;
    float decayMs;
    float sustainLevel;
    float releaseMs;
    float velocityFloor;
    float hpMultiplier;
    float hpMix;

    // Path B state
    float lpMultiplier;
    float lpKeyTrack;   // key-track exponent below middle C (1.0 = original)
    float lpResonance;
    float lpLfoRateHz;
    float lpLfoDepth;
    float chorusMix;
    float chorusDepthFraction;
    float reverbMix;
    float reverbRoomSize;

    // Per-voice amplitude (square-law curve of the smoothed velocity) captured
    // at each voice's noteOn, and a global live expression multiplier. Per-voice
    // main gain = voiceBaseAmp[i] * expression; sub gain multiplies by octaveMix.
    // Per-voice (not global) so that releasing voices keep their own level and
    // don't jump when a new chord is struck. Expression is the live swell
    // (pedal/aftertouch) that scales every sounding voice.
    float voiceBaseAmp[NUM_VOICES];
    float expression;

    // Square-law velocity-to-amplitude curve (uses velocityFloor / 20..100 window).
    float velocityToAmp(float velocity) const;

    // Per-voice LP base cutoff (Hz) for a MIDI note: proportional to pitch at
    // and above middle C, key-track-lifted below it. Shared by noteOn and the
    // re-push setters so the curve is defined in exactly one place.
    float computeLpCutoffHz(int midiNote) const;

    int findAvailableVoice();
    int findVoicePlayingNote(int midiNote);
    int findOldestVoice();
    void updateMixerGains();
    void updateVoiceMixerGains();
    void updateWetDryGains();
    void applyChorusDepth();
    void stopVoiceWavetables(int voiceIndex);
};
