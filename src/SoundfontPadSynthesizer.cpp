#include "SoundfontPadSynthesizer.h"
#include <Arduino.h>

SoundfontPadSynthesizer::SoundfontPadSynthesizer()
    : instrumentData(nullptr)
    , volume(1.0f)
    , attackMs(5.0f)
    , decayMs(0.0f)
    , sustainLevel(1.0f)
    , releaseMs(800.0f)
{
    for (int i = 0; i < NUM_VOICES; ++i) {
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
        voiceStates[i].noteOnTime = 0;
        voiceToEnvelope[i] = nullptr;
        envelopeToMixer[i] = nullptr;
    }
    mixerAToFinal = nullptr;
    mixerBToFinal = nullptr;

    for (int i = 0; i < NUM_VOICES; ++i) {
        voiceToEnvelope[i] = new AudioConnection(voices[i], 0, envelopes[i], 0);

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
}

SoundfontPadSynthesizer::~SoundfontPadSynthesizer() {
    for (int i = 0; i < NUM_VOICES; ++i) {
        delete voiceToEnvelope[i];
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
        voices[voiceIndex].stop();
    }

    voices[voiceIndex].setInstrument(*instrumentData);

    // Linear remap: 0.2 -> 12.7, 1.0 -> 127. Pulls the soft end of the
    // MIDIController piecewise curve further down for more dynamic range.
    int vel = constrain(12.7f + (velocity - 0.2f) * 142.875f, 0, 127);
    voices[voiceIndex].playNote(midiNote, vel);
    envelopes[voiceIndex].noteOn();

    voiceStates[voiceIndex].active = true;
    voiceStates[voiceIndex].midiNote = midiNote;
    voiceStates[voiceIndex].noteOnTime = millis();
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
        voices[i].stop();
        envelopes[i].noteOff();
        voiceStates[i].active = false;
        voiceStates[i].midiNote = -1;
    }
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
