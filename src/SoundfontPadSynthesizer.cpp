#include "SoundfontPadSynthesizer.h"
#include <Arduino.h>

SoundfontPadSynthesizer::SoundfontPadSynthesizer()
    : instrumentData(nullptr)
    , volume(1.0f)
    , octaveMix(1.0f / 3.0f)
    , attackMs(5.0f)
    , decayMs(0.0f)
    , sustainLevel(1.0f)
    , releaseMs(800.0f)
    , velocityFloor(0.25f)
    , expression(1.0f)
{
    for (int i = 0; i < NUM_VOICES; ++i) {
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
        voiceStates[i].noteOnTime = 0;
        voiceBaseAmp[i] = 0.0f;
        mainToVoiceMixer[i] = nullptr;
        subToVoiceMixer[i] = nullptr;
        voiceMixerToEnvelope[i] = nullptr;
        envelopeToMixer[i] = nullptr;
    }
    mixerAToFinal = nullptr;
    mixerBToFinal = nullptr;

    for (int i = 0; i < NUM_VOICES; ++i) {
        mainToVoiceMixer[i]     = new AudioConnection(mainVoices[i], 0, voiceMixers[i], 0);
        subToVoiceMixer[i]      = new AudioConnection(subVoices[i],  0, voiceMixers[i], 1);
        voiceMixerToEnvelope[i] = new AudioConnection(voiceMixers[i], 0, envelopes[i], 0);

        AudioMixer4& targetMixer = (i < 4) ? mixerA : mixerB;
        int targetSlot = i % 4;
        envelopeToMixer[i] = new AudioConnection(envelopes[i], 0, targetMixer, targetSlot);

        envelopes[i].attack(attackMs);
        envelopes[i].decay(decayMs);
        envelopes[i].sustain(sustainLevel);
        envelopes[i].release(releaseMs);
    }

    mixerAToFinal = new AudioConnection(mixerA, 0, finalMixer, 0);
    mixerBToFinal = new AudioConnection(mixerB, 0, finalMixer, 1);

    updateMixerGains();
    updateVoiceMixerGains();
}

SoundfontPadSynthesizer::~SoundfontPadSynthesizer() {
    for (int i = 0; i < NUM_VOICES; ++i) {
        delete mainToVoiceMixer[i];
        delete subToVoiceMixer[i];
        delete voiceMixerToEnvelope[i];
        delete envelopeToMixer[i];
    }
    delete mixerAToFinal;
    delete mixerBToFinal;
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
        voiceMixers[i].gain(0, mainGain);             // main
        voiceMixers[i].gain(1, mainGain * octaveMix); // sub
        voiceMixers[i].gain(2, 0.0f);
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
    return &finalMixer;
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
    // Whitesnake VS samples already have >12 dB of internal headroom, so the
    // usual per-voice 0.25 attenuation isn't needed. Run unity here and let
    // master/mode gains downstream handle balance.
    for (int i = 0; i < 4; ++i) {
        mixerA.gain(i, volume);
        mixerB.gain(i, volume);
    }
    finalMixer.gain(0, 1.0f);
    finalMixer.gain(1, 1.0f);
    finalMixer.gain(2, 0.0f);
    finalMixer.gain(3, 0.0f);
}
