#include "SoundfontPadSynthesizer.h"
#include <Arduino.h>

// Path B chorus defaults: two flanges in parallel, both 0.5 amplitude into chorusBus.
// Rates 0.43 / 0.67 Hz (ratio ~1.56) keep them from drifting back in phase too quickly.
// Offsets and depths are in samples at the audio sample rate (44.1 kHz):
//   A: offset 661 (~15 ms), depth 132 (~3 ms)
//   B: offset 882 (~20 ms), depth 176 (~4 ms)
// CC 46 scales depth around these values; offsets/rates stay fixed.

// Chamberlin SVF stability ceiling. The Teensy filter's frequency() clamps at
// AUDIO_SAMPLE_RATE_EXACT/2.5 (~17.6 kHz), but the actual Chamberlin algorithm
// goes unstable above ~Fs/3.5 (≈12 kHz at Q=0.9) and worse at higher Q. With
// the per-voice LFO modulating up to ±0.6 octaves, an 8 kHz base ceiling
// keeps the worst-case modulated cutoff (8 kHz × 2^0.6 ≈ 12 kHz) just below
// the runaway threshold. Above this ceiling the filter outputs static noise.
static constexpr float LP_CUTOFF_MAX_HZ = 8000.0f;

static constexpr int   CHORUS_A_OFFSET = 661;
static constexpr int   CHORUS_A_DEPTH_DEFAULT = 132;
static constexpr float CHORUS_A_RATE = 0.43f;
static constexpr int   CHORUS_B_OFFSET = 882;
static constexpr int   CHORUS_B_DEPTH_DEFAULT = 176;
static constexpr float CHORUS_B_RATE = 0.67f;
// Max depth (CC 46 fraction = 1.0) is 2x the default. Default corresponds to fraction 0.5.
static constexpr int   CHORUS_A_DEPTH_MAX = CHORUS_A_DEPTH_DEFAULT * 2;
static constexpr int   CHORUS_B_DEPTH_MAX = CHORUS_B_DEPTH_DEFAULT * 2;

SoundfontPadSynthesizer::SoundfontPadSynthesizer()
    : instrumentData(nullptr)
    , volume(2.0f)  // single WHITESNAKE compensation: VS-sample headroom + multi-voice sum
    , octaveMix(95.0f / 127.0f)   // matches WHITESNAKE default CC 25 = 95
    , attackMs(5.0f)
    , decayMs(0.0f)
    , sustainLevel(1.0f)
    , releaseMs(800.0f)
    , velocityFloor(50.0f / 127.0f)         // matches WHITESNAKE default CC 26 = 50
    , hpMultiplier(3.1f)                    // matches WHITESNAKE default CC 27 = 89
    , hpMix(60.0f * 2.0f / 127.0f)          // matches WHITESNAKE default CC 28 = 60
    , lpMultiplier(7.60f)                    // matches WHITESNAKE default CC 41 = 86 (~7.6×)
    , lpKeyTrack(0.0f)                        // 0% tracking below middle C: flat cutoff floor at MC's value (locked in on hardware)
    , lpResonance(1.12f)                     // matches WHITESNAKE default CC 42 = 16 (~1.12)
    , lpLfoRateHz(0.20f)
    , lpLfoDepth(0.5f)
    , chorusMix(55.0f / 127.0f)              // matches WHITESNAKE default CC 45 = 55 (~0.433)
    , chorusDepthFraction(72.0f / 127.0f)    // matches WHITESNAKE default CC 46 = 72 (~0.567)
    , reverbMix(75.0f / 127.0f)              // matches WHITESNAKE default CC 47 = 75 (~0.591)
    , reverbRoomSize(75.0f / 127.0f)         // matches WHITESNAKE default CC 48 = 75 (~0.591)
    , expression(1.0f)
{
    for (int i = 0; i < NUM_VOICES; ++i) {
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
        voiceStates[i].noteOnTime = 0;
        voiceBaseAmp[i] = 0.0f;
        mainToVoiceMixer[i] = nullptr;
        mainToHpFilter[i] = nullptr;
        hpFilterToVoiceMixer[i] = nullptr;
        subToVoiceMixer[i] = nullptr;
        voiceMixerToLpf[i] = nullptr;
        lfoToLpf[i] = nullptr;
        lpfToEnvelope[i] = nullptr;
        envelopeToMixer[i] = nullptr;
    }
    mixerAToFinal = nullptr;
    mixerBToFinal = nullptr;
    finalToDry = nullptr;
    finalToChorusA = nullptr;
    finalToChorusB = nullptr;
    chorusAToBus = nullptr;
    chorusBToBus = nullptr;
    finalToChorusBusDryCancel = nullptr;
    chorusBusToWetDry = nullptr;
    finalToPreReverb = nullptr;
    chorusBusToPreReverb = nullptr;
    preReverbToReverb = nullptr;
    reverbToWetDry = nullptr;

    for (int i = 0; i < NUM_VOICES; ++i) {
        mainToVoiceMixer[i]     = new AudioConnection(mainVoices[i], 0, voiceMixers[i], 0);
        mainToHpFilter[i]       = new AudioConnection(mainVoices[i], 0, hpFilters[i],   0);
        // State-variable filter output port 2 = highpass
        hpFilterToVoiceMixer[i] = new AudioConnection(hpFilters[i],  2, voiceMixers[i], 2);
        subToVoiceMixer[i]      = new AudioConnection(subVoices[i],  0, voiceMixers[i], 1);

        // Path B: voiceMixer -> lpfVoice (signal on input 0, LFO on input 1) -> envelope
        voiceMixerToLpf[i] = new AudioConnection(voiceMixers[i], 0, lpfVoice[i], 0);
        lfoToLpf[i]        = new AudioConnection(lfoVoice[i],    0, lpfVoice[i], 1);
        // State-variable filter output port 0 = lowpass
        lpfToEnvelope[i]   = new AudioConnection(lpfVoice[i],    0, envelopes[i], 0);

        AudioMixer4& targetMixer = (i < 4) ? mixerA : mixerB;
        int targetSlot = i % 4;
        envelopeToMixer[i] = new AudioConnection(envelopes[i], 0, targetMixer, targetSlot);

        envelopes[i].attack(attackMs);
        envelopes[i].decay(decayMs);
        envelopes[i].sustain(sustainLevel);
        envelopes[i].release(releaseMs);

        // ±lpLfoDepth * octaveControl swing around the base cutoff. octaveControl
        // is a build-time constant (0.6 octaves at LFO=±1). Default depth 0.5
        // gives ±0.15 octaves of brightness motion under the pad.
        lpfVoice[i].octaveControl(0.6f);
        lpfVoice[i].resonance(lpResonance);

        // Free-running sine LFO with per-voice random phase (decorrelated for
        // the life of the synth — never resets on noteOn).
        lfoVoice[i].begin(WAVEFORM_SINE);
        lfoVoice[i].amplitude(lpLfoDepth);
        lfoVoice[i].frequency(lpLfoRateHz);
        lfoVoice[i].phase((float)random(360));
    }

    mixerAToFinal = new AudioConnection(mixerA, 0, finalMixer, 0);
    mixerBToFinal = new AudioConnection(mixerB, 0, finalMixer, 1);

    // Path B post-FX wiring (all mono)
    finalToDry                = new AudioConnection(finalMixer, 0, wetDryMixer, 0);
    finalToChorusA            = new AudioConnection(finalMixer, 0, chorusA,     0);
    finalToChorusB            = new AudioConnection(finalMixer, 0, chorusB,     0);
    chorusAToBus              = new AudioConnection(chorusA,    0, chorusBus,   0);
    chorusBToBus              = new AudioConnection(chorusB,    0, chorusBus,   1);
    // AudioEffectFlange outputs (dry+delayed)/2. Subtract dry on ch2 so the
    // chorus tap is pure delayed/modulated signal — otherwise the dry bleed
    // dominates the wet mix and masks the modulation.
    finalToChorusBusDryCancel = new AudioConnection(finalMixer, 0, chorusBus,   2);
    chorusBusToWetDry         = new AudioConnection(chorusBus,  0, wetDryMixer, 1);
    finalToPreReverb     = new AudioConnection(finalMixer,    0, preReverbMixer, 0);
    chorusBusToPreReverb = new AudioConnection(chorusBus,     0, preReverbMixer, 1);
    preReverbToReverb    = new AudioConnection(preReverbMixer, 0, reverbPost,    0);
    reverbToWetDry       = new AudioConnection(reverbPost,    0, wetDryMixer,    2);

    chorusA.begin(chorusBufA, CHORUS_BUF_LEN, CHORUS_A_OFFSET, CHORUS_A_DEPTH_DEFAULT, CHORUS_A_RATE);
    chorusB.begin(chorusBufB, CHORUS_BUF_LEN, CHORUS_B_OFFSET, CHORUS_B_DEPTH_DEFAULT, CHORUS_B_RATE);
    // ch0/1: sum the two flanges at unity (each is already (dry+delayed)/2).
    // ch2: subtract one copy of dry to cancel the two halves of dry bleed
    // contributed by the flanges. Result: chorusBus = (delayedA + delayedB) / 2,
    // pure modulated signal. Final wet level is set by wetDryMixer.ch1 (chorusMix).
    chorusBus.gain(0,  1.0f);
    chorusBus.gain(1,  1.0f);
    chorusBus.gain(2, -1.0f);
    chorusBus.gain(3,  0.0f);
    // Reverb input: dry + chorus summed at 0.5 each so the reverb sees roughly
    // unity-amplitude input even when chorus is at full and stays present when
    // chorusMix is dialed to 0 (reverb is fed from chorusBus, not wetDryMixer).
    preReverbMixer.gain(0, 0.5f);
    preReverbMixer.gain(1, 0.5f);
    preReverbMixer.gain(2, 0.0f);
    preReverbMixer.gain(3, 0.0f);

    reverbPost.roomsize(reverbRoomSize);
    reverbPost.damping(0.5f);

    updateMixerGains();
    updateVoiceMixerGains();
    updateWetDryGains();
}

SoundfontPadSynthesizer::~SoundfontPadSynthesizer() {
    for (int i = 0; i < NUM_VOICES; ++i) {
        delete mainToVoiceMixer[i];
        delete mainToHpFilter[i];
        delete hpFilterToVoiceMixer[i];
        delete subToVoiceMixer[i];
        delete voiceMixerToLpf[i];
        delete lfoToLpf[i];
        delete lpfToEnvelope[i];
        delete envelopeToMixer[i];
    }
    delete mixerAToFinal;
    delete mixerBToFinal;
    delete finalToDry;
    delete finalToChorusA;
    delete finalToChorusB;
    delete chorusAToBus;
    delete chorusBToBus;
    delete finalToChorusBusDryCancel;
    delete chorusBusToWetDry;
    delete finalToPreReverb;
    delete chorusBusToPreReverb;
    delete preReverbToReverb;
    delete reverbToWetDry;
    // instrumentData is borrowed — do NOT delete
}

void SoundfontPadSynthesizer::setInstrumentData(AudioSynthWavetable::instrument_data* data) {
    allNotesOff();
    instrumentData = data;
    if (data != nullptr) {
        Serial.println("SoundfontPadSynthesizer: instrument data set");
    } else {
        Serial.println("SoundfontPadSynthesizer: instrument data cleared");
    }
}

void SoundfontPadSynthesizer::noteOn(int midiNote, float velocity) {
    if (instrumentData == nullptr) {
        return;
    }

    int voiceIndex = findAvailableVoice();
    if (voiceIndex == -1) {
        voiceIndex = findOldestVoice();
    }

    // Always stop both wavetables before re-arming. A stale sub from a
    // previous noteOn that never got an explicit stop would otherwise keep
    // sounding (muted only by the envelope) and bleed through if the
    // envelope re-opens with this voice.
    stopVoiceWavetables(voiceIndex);

    mainVoices[voiceIndex].setInstrument(*instrumentData);
    subVoices[voiceIndex].setInstrument(*instrumentData);

    // Per-voice HP cutoff = noteHz * hpMultiplier, pinned at noteOn. Does not
    // track pitch bend (not used in WHITESNAKE) -- if we ever add bend, retune
    // sounding voices' filters from the global bend value.
    float noteHz = 440.0f * powf(2.0f, ((float)midiNote - 69.0f) / 12.0f);
    hpFilters[voiceIndex].frequency(noteHz * hpMultiplier);
    // Per-voice LP cutoff base, pinned at noteOn. LFO modulates ±octaveControl
    // around this base. Re-pushed to sounding voices when setPadLpMultiplier
    // or setPadLpKeyTrack is called. See computeLpCutoffHz for the curve.
    lpfVoice[voiceIndex].frequency(computeLpCutoffHz(midiNote));

    float amp = velocityToAmp(velocity);
    voiceBaseAmp[voiceIndex] = amp;
    Serial.printf("Pad noteOn: midiNote=%d, inVel=%.3f, amp=%.4f\n",
                  midiNote, velocity, amp);

    // Always play at full wavetable amplitude; dynamics live in the voice
    // mixer (full float precision, and adjustable while the note sounds).
    mainVoices[voiceIndex].playNote(midiNote, 127);
    if (midiNote >= 12) {
        subVoices[voiceIndex].playNote(midiNote - 12, 127);
    }
    envelopes[voiceIndex].noteOn();

    voiceStates[voiceIndex].active = true;
    voiceStates[voiceIndex].midiNote = midiNote;
    voiceStates[voiceIndex].noteOnTime = millis();

    updateVoiceMixerGains();
}

void SoundfontPadSynthesizer::noteOff(int midiNote) {
    int voiceIndex = findVoicePlayingNote(midiNote);
    if (voiceIndex != -1) {
        envelopes[voiceIndex].noteOff();
        voiceStates[voiceIndex].active = false;
        voiceStates[voiceIndex].midiNote = -1;
    }
}

void SoundfontPadSynthesizer::allNotesOff() {
    for (int i = 0; i < NUM_VOICES; ++i) {
        stopVoiceWavetables(i);
        envelopes[i].noteOff();
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
    }
}

void SoundfontPadSynthesizer::stopVoiceWavetables(int voiceIndex) {
    mainVoices[voiceIndex].stop();
    subVoices[voiceIndex].stop();
}

void SoundfontPadSynthesizer::setOctaveMix(float mix) {
    octaveMix = constrain(mix, 0.0f, 1.0f);
    updateVoiceMixerGains();
}

void SoundfontPadSynthesizer::updateVoiceMixerGains() {
    for (int i = 0; i < NUM_VOICES; ++i) {
        float mainGain = voiceBaseAmp[i] * expression;
        voiceMixers[i].gain(0, mainGain);             // main (dry)
        voiceMixers[i].gain(1, mainGain * octaveMix); // sub
        voiceMixers[i].gain(2, mainGain * hpMix);     // main (HP)
        voiceMixers[i].gain(3, 0.0f);
    }
}

void SoundfontPadSynthesizer::setExpression(float e) {
    expression = constrain(e, 0.0f, 1.0f);
    updateVoiceMixerGains();
}

// Square-law velocity-to-amplitude curve, aligned with midiVelocityToFloat (v/128):
//   MIDI byte <= 20  -> amp = velocityFloor^2
//   MIDI byte >= 100 -> amp = 1.0
//   between          -> amp = (velocityFloor + (1-velocityFloor)*t)^2
//                       where t = (v - 20/128) / (80/128)
// velocityFloor controls the dB range: 0.25 -> 24 dB (default), 0.5 -> 12 dB, 1.0 -> flat.
float SoundfontPadSynthesizer::velocityToAmp(float velocity) const {
    constexpr float V_LOW  = 20.0f / 128.0f;
    constexpr float V_HIGH = 100.0f / 128.0f;
    float v = constrain(velocity, V_LOW, V_HIGH);
    float t = (v - V_LOW) / (V_HIGH - V_LOW);
    float linear = velocityFloor + (1.0f - velocityFloor) * t;
    return linear * linear;
}

void SoundfontPadSynthesizer::setHeldLevel(float velocity) {
    float amp = velocityToAmp(velocity);
    for (int i = 0; i < NUM_VOICES; ++i) {
        if (voiceStates[i].active) {
            voiceBaseAmp[i] = amp;
        }
    }
    updateVoiceMixerGains();
}

int SoundfontPadSynthesizer::getActiveVoiceCount() const {
    int count = 0;
    for (int i = 0; i < NUM_VOICES; ++i) {
        if (voiceStates[i].active) {
            count++;
        }
    }
    return count;
}

void SoundfontPadSynthesizer::setVolume(float vol) {
    volume = constrain(vol, 0.0f, 1.0f);
    updateMixerGains();
}

AudioStream* SoundfontPadSynthesizer::getOutput() {
    return &wetDryMixer;
}

void SoundfontPadSynthesizer::setAttack(float milliseconds) {
    attackMs = constrain(milliseconds, 0.0f, 11880.0f);
    for (int i = 0; i < NUM_VOICES; ++i) {
        envelopes[i].attack(attackMs);
    }
}

void SoundfontPadSynthesizer::setDecay(float milliseconds) {
    decayMs = constrain(milliseconds, 0.0f, 11880.0f);
    for (int i = 0; i < NUM_VOICES; ++i) {
        envelopes[i].decay(decayMs);
    }
}

void SoundfontPadSynthesizer::setSustain(float level) {
    sustainLevel = constrain(level, 0.0f, 1.0f);
    for (int i = 0; i < NUM_VOICES; ++i) {
        envelopes[i].sustain(sustainLevel);
    }
}

void SoundfontPadSynthesizer::setRelease(float milliseconds) {
    releaseMs = constrain(milliseconds, 0.0f, 11880.0f);
    for (int i = 0; i < NUM_VOICES; ++i) {
        envelopes[i].release(releaseMs);
    }
}

void SoundfontPadSynthesizer::setADSR(float attack, float decay, float sustain, float release) {
    setAttack(attack);
    setDecay(decay);
    setSustain(sustain);
    setRelease(release);
}

void SoundfontPadSynthesizer::setVelocityFloor(float floor) {
    velocityFloor = constrain(floor, 0.0f, 1.0f);
}

void SoundfontPadSynthesizer::setHighpassMultiplier(float m) {
    hpMultiplier = constrain(m, 1.0f, 4.0f);
    // Re-push cutoff for every sounding voice using its captured note.
    for (int i = 0; i < NUM_VOICES; ++i) {
        int n = voiceStates[i].midiNote;
        if (n < 0) continue;
        float noteHz = 440.0f * powf(2.0f, ((float)n - 69.0f) / 12.0f);
        hpFilters[i].frequency(noteHz * hpMultiplier);
    }
}

void SoundfontPadSynthesizer::setHighpassMix(float m) {
    hpMix = constrain(m, 0.0f, 2.0f);
    updateVoiceMixerGains();
}

// Per-voice LP base cutoff for a MIDI note. At/above middle C the cutoff is
// fully proportional to pitch (noteHz * lpMultiplier) -- the original curve.
// Below middle C the slope is reduced by lpKeyTrack so the bass keeps
// brightness: each octave down multiplies the cutoff by 2^(-lpKeyTrack)
// instead of halving it. Pivots exactly at middle C (both branches equal
// refHz*lpMultiplier there), so the above-middle-C sound is untouched.
// Clamped to LP_CUTOFF_MAX_HZ to keep the Chamberlin SVF stable.
float SoundfontPadSynthesizer::computeLpCutoffHz(int midiNote) const {
    constexpr float refHz = 261.63f;  // middle C
    float noteHz = 440.0f * powf(2.0f, ((float)midiNote - 69.0f) / 12.0f);
    float lpHz;
    if (noteHz >= refHz) {
        lpHz = noteHz * lpMultiplier;
    } else {
        lpHz = refHz * lpMultiplier * powf(noteHz / refHz, lpKeyTrack);
    }
    if (lpHz > LP_CUTOFF_MAX_HZ) lpHz = LP_CUTOFF_MAX_HZ;
    return lpHz;
}

void SoundfontPadSynthesizer::setPadLpMultiplier(float m) {
    lpMultiplier = constrain(m, 1.0f, 20.0f);
    // Re-push the LP base cutoff for every sounding voice (held or releasing).
    // findVoicePlayingNote() only finds held; we want releasing voices to track
    // too, so iterate all and skip cleared midiNote slots.
    for (int i = 0; i < NUM_VOICES; ++i) {
        int n = voiceStates[i].midiNote;
        if (n < 0) continue;
        lpfVoice[i].frequency(computeLpCutoffHz(n));
    }
}

void SoundfontPadSynthesizer::setPadLpKeyTrack(float k) {
    lpKeyTrack = constrain(k, 0.0f, 1.0f);
    // Re-push to every sounding voice (held or releasing), same as the
    // multiplier setter. Only notes below middle C actually change.
    for (int i = 0; i < NUM_VOICES; ++i) {
        int n = voiceStates[i].midiNote;
        if (n < 0) continue;
        lpfVoice[i].frequency(computeLpCutoffHz(n));
    }
}

void SoundfontPadSynthesizer::setPadLpResonance(float q) {
    lpResonance = constrain(q, 0.7f, 4.0f);
    for (int i = 0; i < NUM_VOICES; ++i) {
        lpfVoice[i].resonance(lpResonance);
    }
}

void SoundfontPadSynthesizer::setPadLpLfoRate(float hz) {
    lpLfoRateHz = constrain(hz, 0.05f, 1.5f);
    for (int i = 0; i < NUM_VOICES; ++i) {
        lfoVoice[i].frequency(lpLfoRateHz);
    }
}

void SoundfontPadSynthesizer::setPadLpLfoDepth(float depth) {
    lpLfoDepth = constrain(depth, 0.0f, 1.0f);
    for (int i = 0; i < NUM_VOICES; ++i) {
        lfoVoice[i].amplitude(lpLfoDepth);
    }
}

void SoundfontPadSynthesizer::setChorusMix(float m) {
    chorusMix = constrain(m, 0.0f, 1.0f);
    updateWetDryGains();
}

void SoundfontPadSynthesizer::setChorusDepth(float fraction) {
    chorusDepthFraction = constrain(fraction, 0.0f, 1.0f);
    applyChorusDepth();
}

void SoundfontPadSynthesizer::setReverbMix(float m) {
    reverbMix = constrain(m, 0.0f, 1.0f);
    updateWetDryGains();
}

void SoundfontPadSynthesizer::setReverbRoomSize(float s) {
    reverbRoomSize = constrain(s, 0.0f, 1.0f);
    reverbPost.roomsize(reverbRoomSize);
}

void SoundfontPadSynthesizer::updateWetDryGains() {
    wetDryMixer.gain(0, 1.0f);        // dry: fixed at unity
    wetDryMixer.gain(1, chorusMix);   // chorus wet tap
    wetDryMixer.gain(2, reverbMix);   // reverb wet tap
    wetDryMixer.gain(3, 0.0f);
}

void SoundfontPadSynthesizer::applyChorusDepth() {
    // Scale around defaults: fraction 0.5 -> default depth, 1.0 -> 2x default.
    int depthA = (int)(CHORUS_A_DEPTH_MAX * chorusDepthFraction);
    int depthB = (int)(CHORUS_B_DEPTH_MAX * chorusDepthFraction);
    // voices() resets LFO phase and circular buffer index -> click on each call.
    // Acceptable: this CC is intended as a sound-design knob, not a live sweep.
    chorusA.voices(CHORUS_A_OFFSET, depthA, CHORUS_A_RATE);
    chorusB.voices(CHORUS_B_OFFSET, depthB, CHORUS_B_RATE);
}

int SoundfontPadSynthesizer::findAvailableVoice() {
    // A voice is only truly free when the key is up AND the envelope has
    // finished its release tail. Otherwise we'd cut off releasing notes —
    // especially audible on the long pad releases this class is built for.
    for (int i = 0; i < NUM_VOICES; ++i) {
        if (!voiceStates[i].active && !envelopes[i].isActive()) {
            return i;
        }
    }
    return -1;
}

int SoundfontPadSynthesizer::findVoicePlayingNote(int midiNote) {
    for (int i = 0; i < NUM_VOICES; ++i) {
        if (voiceStates[i].active && voiceStates[i].midiNote == midiNote) {
            return i;
        }
    }
    return -1;
}

int SoundfontPadSynthesizer::findOldestVoice() {
    // Prefer to steal a releasing voice (key already up) over a held one.
    int oldestReleasingIndex = -1;
    unsigned long oldestReleasingTime = 0;
    int oldestActiveIndex = -1;
    unsigned long oldestActiveTime = 0;

    for (int i = 0; i < NUM_VOICES; ++i) {
        if (voiceStates[i].active) {
            if (oldestActiveIndex == -1 || voiceStates[i].noteOnTime < oldestActiveTime) {
                oldestActiveTime = voiceStates[i].noteOnTime;
                oldestActiveIndex = i;
            }
        } else if (envelopes[i].isActive()) {
            if (oldestReleasingIndex == -1 || voiceStates[i].noteOnTime < oldestReleasingTime) {
                oldestReleasingTime = voiceStates[i].noteOnTime;
                oldestReleasingIndex = i;
            }
        }
    }

    if (oldestReleasingIndex != -1) return oldestReleasingIndex;
    if (oldestActiveIndex != -1) return oldestActiveIndex;
    return 0;
}

void SoundfontPadSynthesizer::updateMixerGains() {
    // Single compensation point for both VS-sample headroom and multi-voice
    // sum: the per-voice gain on mixerA/B. Downstream (soundfontVolume,
    // masterVolume) is left at user-driven values; nothing else in the chain
    // applies a mode-specific scalar. Tune `volume` to the headroom budget
    // (HP overlay branch up to hpMix=2.0, sub up to octaveMix=1.0, full
    // velocity) -- if the whitesnakePadPeakMonitor clips, drop volume here
    // rather than reintroducing scalars downstream.
    for (int i = 0; i < 4; ++i) {
        mixerA.gain(i, volume);
        mixerB.gain(i, volume);
    }
    finalMixer.gain(0, 1.0f);
    finalMixer.gain(1, 1.0f);
    finalMixer.gain(2, 0.0f);
    finalMixer.gain(3, 0.0f);
}
