#include "MIDIControlCallbacks.h"
#include "StringPadSynthesizer.h"

// ============================================================================
// Control Change Callback Functions
// ============================================================================

void CC_Attenuation(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().setAttenuation(value);
}

void CC_FilterStrength(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().setFilterStrength(value);
}

void CC_MasterVolume(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().setMasterVolume((float)value / 64.0f);
    Serial.printf("Master volume: %.2f\n", (float)value / 64.0f);
}

void CC_DroneVolume(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().setDroneVolume((float)value / 127.0f);
    Serial.printf("Drone volume: %.2f\n", (float)value / 127.0f);
}

void CC_FilterCutoff(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().getDrone().setFilterCutoff((float)value / 127.0f);
    Serial.printf("Filter cutoff: %.2f\n", (float)value / 127.0f);
}

void CC_LFORate(MIDIController* controller, byte channel, byte control, byte value) {
    float lfoRate;
    if (value == 0) {
        lfoRate = 0.0f; // Disable LFO when value is 0
    } else {
        lfoRate = ((float)value / 127.0f) * 10.0f; // 0 to 10 Hz
    }
    controller->getSynth().getDrone().setLFORate(lfoRate);
    if (lfoRate == 0.0f) {
        Serial.println("LFO disabled");
    } else {
        Serial.printf("LFO rate: %.2f Hz\n", lfoRate);
    }
}

void CC_OscillatorDetune(MIDIController* controller, byte channel, byte control, byte value) {
    float detune = ((float)value / 128.0f - 0.5f) * 2.0f; // -1.0 to +1.0
    controller->getSynth().getDrone().setOscillatorDetune(detune);
    Serial.printf("Oscillator detune: %.3f semitones\n", detune);
}

void CC_LFODepth(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().getDrone().setLFODepth((float)value / 127.0f);
    Serial.printf("LFO depth: %.2f\n", (float)value / 127.0f);
}

void CC_SoundfontAttack(MIDIController* controller, byte channel, byte control, byte value) {
    // Map 0-127 to 0.5ms - 2000ms (exponential)
    float attackMs = 0.5f * pow(4000.0f, (float)value / 127.0f);
    controller->getSynth().getSoundfont().setAttack(attackMs);
    Serial.printf("Soundfont Attack: %.1f ms (CC %d = %d)\n", attackMs, control, value);
}

void CC_SoundfontDecay(MIDIController* controller, byte channel, byte control, byte value) {
    // Map 0-127 to 0.5ms - 2000ms (exponential)
    float decayMs = 0.5f * pow(4000.0f, (float)value / 127.0f);
    controller->getSynth().getSoundfont().setDecay(decayMs);
    Serial.printf("Soundfont Decay: %.1f ms (CC %d = %d)\n", decayMs, control, value);
}

void CC_SoundfontSustain(MIDIController* controller, byte channel, byte control, byte value) {
    // Map 0-127 to 0.0 - 1.0 (linear)
    float sustainLevel = (float)value / 127.0f;
    controller->getSynth().getSoundfont().setSustain(sustainLevel);
    Serial.printf("Soundfont Sustain: %.2f (CC %d = %d)\n", sustainLevel, control, value);
}

void CC_SoundfontRelease(MIDIController* controller, byte channel, byte control, byte value) {
    // Map 0-127 to 5ms - 5000ms (exponential)
    float releaseMs = 5.0f * pow(1000.0f, (float)value / 127.0f);
    controller->getSynth().getSoundfont().setRelease(releaseMs);
    Serial.printf("Soundfont Release: %.1f ms (CC %d = %d)\n", releaseMs, control, value);
}

void CC_SoundfontFilterFrequency(MIDIController* controller, byte channel, byte control, byte value) {
    // Map 0-127 to 0.5 - 20.0 (exponential) - multiplier of note frequency
    float multiplier = 0.5f * pow(40.0f, (float)value / 127.0f);
    controller->getSynth().getSoundfont().setFilterMultiplier(multiplier);
    Serial.printf("Soundfont Filter Multiplier: %.2fx (CC %d = %d)\n", multiplier, control, value);
}

void CC_SoundfontFilterResonance(MIDIController* controller, byte channel, byte control, byte value) {
    // Map 0-127 to 0.7 - 5.0 (linear)
    float q = 0.7f + ((float)value / 127.0f) * 4.3f;
    controller->getSynth().getSoundfont().setFilterResonance(q);
    Serial.printf("Soundfont Filter Resonance: %.2f (CC %d = %d)\n", q, control, value);
}

void CC_SoundfontCrossfadeDuration(MIDIController* controller, byte channel, byte control, byte value) {
    // Map 0-127 to 10ms - 5000ms (exponential)
    float durationMs = 10.0f * pow(500.0f, (float)value / 127.0f);
    controller->getSynth().getSoundfont().setCrossfadeDuration(durationMs);
    Serial.printf("Soundfont Crossfade Duration: %.1f ms (CC %d = %d)\n", durationMs, control, value);
}

void CC_SoundfontVolume(MIDIController* controller, byte channel, byte control, byte value) {
    // Linear, step = 4/128. value=0 -> 0.0, value=32 -> 1.0 (unity), value=127 -> ~3.97.
    // Affects both regular soundfont and Whitesnake pad outputs (shared mixerL6/R6 gain).
    float gain = (float)value * (4.0f / 128.0f);
    controller->getSynth().setSoundfontVolume(gain);
    Serial.printf("Soundfont Volume: %.3f (CC %d = %d)\n", gain, control, value);
}

void CC_WhitesnakePadAttack(MIDIController* controller, byte channel, byte control, byte value) {
    float attackMs = 0.5f * pow(4000.0f, (float)value / 127.0f);
    controller->getSynth().getWhitesnakePad().setAttack(attackMs);
    Serial.printf("Whitesnake Pad Attack: %.1f ms (CC %d = %d)\n", attackMs, control, value);
}

void CC_WhitesnakePadDecay(MIDIController* controller, byte channel, byte control, byte value) {
    float decayMs = 0.5f * pow(4000.0f, (float)value / 127.0f);
    controller->getSynth().getWhitesnakePad().setDecay(decayMs);
    Serial.printf("Whitesnake Pad Decay: %.1f ms (CC %d = %d)\n", decayMs, control, value);
}

void CC_WhitesnakePadSustain(MIDIController* controller, byte channel, byte control, byte value) {
    float sustainLevel = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setSustain(sustainLevel);
    Serial.printf("Whitesnake Pad Sustain: %.2f (CC %d = %d)\n", sustainLevel, control, value);
}

void CC_WhitesnakeOctaveMix(MIDIController* controller, byte channel, byte control, byte value) {
    float mix = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setOctaveMix(mix);
    Serial.printf("Whitesnake Octave Mix: %.3f (CC %d = %d)\n", mix, control, value);
}

void CC_WhitesnakePadRelease(MIDIController* controller, byte channel, byte control, byte value) {
    float releaseMs = 5.0f * pow(1000.0f, (float)value / 127.0f);
    controller->getSynth().getWhitesnakePad().setRelease(releaseMs);
    Serial.printf("Whitesnake Pad Release: %.1f ms (CC %d = %d)\n", releaseMs, control, value);
}

void CC_WhitesnakePadVelocityFloor(MIDIController* controller, byte channel, byte control, byte value) {
    // Linear 0..127 -> 0..1.0. This is the pre-square floor of the velocity curve;
    // amp floor is its square. Default ~32 -> 0.25 -> 24 dB range.
    float floor = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setVelocityFloor(floor);
    Serial.printf("Whitesnake Velocity Floor: %.3f (amp floor %.4f) (CC %d = %d)\n",
                  floor, floor * floor, control, value);
}

void CC_WhitesnakePadHighpassMultiplier(MIDIController* controller, byte channel, byte control, byte value) {
    // Linear 0..127 -> 1.0x..4.0x cutoff multiplier of note frequency.
    float m = 1.0f + (float)value * (3.0f / 127.0f);
    controller->getSynth().getWhitesnakePad().setHighpassMultiplier(m);
    Serial.printf("Whitesnake HP Multiplier: %.2fx (CC %d = %d)\n", m, control, value);
}

void CC_WhitesnakePadHighpassMix(MIDIController* controller, byte channel, byte control, byte value) {
    // Linear 0..127 -> 0.0..2.0 gain on the HP branch. Default at value=64 -> ~1.0.
    float mix = (float)value * (2.0f / 127.0f);
    controller->getSynth().getWhitesnakePad().setHighpassMix(mix);
    Serial.printf("Whitesnake HP Mix: %.3f (CC %d = %d)\n", mix, control, value);
}

void CC_WhitesnakeFilterLfoRate(MIDIController* controller, byte channel, byte control, byte value) {
    // Exponential 0.05..1.5 Hz (ratio 30).
    float hz = 0.05f * pow(30.0f, (float)value / 127.0f);
    controller->getSynth().getWhitesnakePad().setPadLpLfoRate(hz);
    Serial.printf("Whitesnake LP LFO Rate: %.3f Hz (CC %d = %d)\n", hz, control, value);
}

void CC_WhitesnakeFilterLfoDepth(MIDIController* controller, byte channel, byte control, byte value) {
    // Linear 0..127 -> 0..1.0. 0 = static filter, 1 = full ±octaveControl swing.
    float depth = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setPadLpLfoDepth(depth);
    Serial.printf("Whitesnake LP LFO Depth: %.3f (CC %d = %d)\n", depth, control, value);
}

void CC_WhitesnakeFilterCutoff(MIDIController* controller, byte channel, byte control, byte value) {
    // Exponential 1.0..20.0× note frequency (ratio 20). Default 6× at value ~98.
    float m = 1.0f * pow(20.0f, (float)value / 127.0f);
    controller->getSynth().getWhitesnakePad().setPadLpMultiplier(m);
    Serial.printf("Whitesnake LP Cutoff: %.2fx note (CC %d = %d)\n", m, control, value);
}

void CC_WhitesnakeFilterResonance(MIDIController* controller, byte channel, byte control, byte value) {
    // Linear 0..127 -> 0.7..4.0. Watch self-oscillation above ~3.5.
    float q = 0.7f + (float)value * (3.3f / 127.0f);
    controller->getSynth().getWhitesnakePad().setPadLpResonance(q);
    Serial.printf("Whitesnake LP Q: %.2f (CC %d = %d)\n", q, control, value);
}

void CC_WhitesnakeChorusMix(MIDIController* controller, byte channel, byte control, byte value) {
    float mix = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setChorusMix(mix);
    Serial.printf("Whitesnake Chorus Mix: %.3f (CC %d = %d)\n", mix, control, value);
}

void CC_WhitesnakeChorusDepth(MIDIController* controller, byte channel, byte control, byte value) {
    // Linear 0..127 -> 0..1 fraction. 0.5 = default depths (132/176 samples).
    // Re-inits both flanges -> audible click on each message; not for sweeps.
    float fraction = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setChorusDepth(fraction);
    Serial.printf("Whitesnake Chorus Depth: %.3f (CC %d = %d)\n", fraction, control, value);
}

void CC_WhitesnakeReverbMix(MIDIController* controller, byte channel, byte control, byte value) {
    float mix = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setReverbMix(mix);
    Serial.printf("Whitesnake Reverb Mix: %.3f (CC %d = %d)\n", mix, control, value);
}

void CC_WhitesnakeReverbSize(MIDIController* controller, byte channel, byte control, byte value) {
    float size = (float)value / 127.0f;
    controller->getSynth().getWhitesnakePad().setReverbRoomSize(size);
    Serial.printf("Whitesnake Reverb Size: %.3f (CC %d = %d)\n", size, control, value);
}

void CC_StringPadVolume(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().setStringPadVolume((float)value / 127.0f);
    Serial.printf("String pad volume: %.2f\n", (float)value / 127.0f);
}

void CC_StringPadFilterCutoff(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().getStringPad().setFilterCutoff((float)value / 127.0f);
    Serial.printf("String pad filter cutoff: %.2f\n", (float)value / 127.0f);
}

void CC_StringPadFilterResonance(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().getStringPad().setFilterResonance((float)value / 127.0f);
    Serial.printf("String pad filter resonance: %.2f\n", (float)value / 127.0f);
}

void CC_StringPadDetuneAmount(MIDIController* controller, byte channel, byte control, byte value) {
    controller->getSynth().getStringPad().setDetuneAmount((float)value / 127.0f);
    Serial.printf("String pad detune amount: %.2f\n", (float)value / 127.0f);
}

void CC_HighpassMultiplier(MIDIController* controller, byte channel, byte control, byte value) {
    float multiplier;
    if (value <= 64) {
        float t = (float)value / 64.0f;
        multiplier = 0.05f * pow(20.0f, t);
    } else {
        float t = (float)(value - 64) / 63.0f;
        multiplier = 1.0f * pow(4.0f, t);
    }
    controller->getSynth().getStringPad().setHighpassMultiplier(multiplier);
    Serial.print("Highpass Multiplier: ");
    Serial.print(multiplier, 3);
    Serial.print(" (CC ");
    Serial.print(control);
    Serial.print(" = ");
    Serial.print(value);
    Serial.println(")");
}

void CC_ModePluckedStrings(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        controller->getSynth().setSynthMode(HybridSynthesizer::PLUCKED_STRINGS);
        installBanksForMode(controller, HybridSynthesizer::PLUCKED_STRINGS);
        Serial.print("Mode: PLUCKED_STRINGS (CC ");
        Serial.print(control);
        Serial.println(")");
    }
}

void CC_ModeDrone(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        controller->getSynth().setSynthMode(HybridSynthesizer::DRONE);
        installBanksForMode(controller, HybridSynthesizer::DRONE);
        Serial.print("Mode: DRONE (CC ");
        Serial.print(control);
        Serial.println(")");
    }
}

void CC_ModeStringPadsBright(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        controller->getSynth().setSynthMode(HybridSynthesizer::STRING_PADS);
        installBanksForMode(controller, HybridSynthesizer::STRING_PADS);
        controller->getSynth().getStringPad().loadPreset(StringPadSynthesizer::PRESET_BRIGHT_STRINGS);
        Serial.print("Mode: STRING_PADS + Bright Strings preset (CC ");
        Serial.print(control);
        Serial.println(")");
    }
}

void CC_ModeSoundfontTrombone(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        Serial.print("CC ");
        Serial.print(control);
        Serial.println(" triggered - Switching to Soundfont mode");
        controller->getSynth().setSynthMode(HybridSynthesizer::SOUNDFONT);
        installBanksForMode(controller, HybridSynthesizer::SOUNDFONT);
        Serial.println("Unloading all instruments...");
        for (int i = 0; i < 4; i++) {
            controller->getSynth().getSoundfont().unloadInstrument(i);
        }
        
        // Reset ADSR to defaults
        controller->getSynth().getSoundfont().setAttack(5.0f);
        controller->getSynth().getSoundfont().setDecay(200.0f);
        controller->getSynth().getSoundfont().setSustain(0.4f);
        controller->getSynth().getSoundfont().setRelease(300.0f);
        
        Serial.println("Mode switched to SOUNDFONT, loading trombone.sf2...");
        bool success = controller->getSynth().getSoundfont().loadInstrument(0, "trombone.sf2", 0);
        if (success) {
            Serial.print("Mode: SOUNDFONT + trombone.sf2 (CC ");
            Serial.print(control);
            Serial.println(") - SUCCESS");
        } else {
            Serial.print("Mode: SOUNDFONT + trombone.sf2 (CC ");
            Serial.print(control);
            Serial.println(") - FAILED");
        }
        controller->getSynth().setDefaultInstrument(0);
    }
}

void CC_ModeTusk(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        Serial.print("CC ");
        Serial.print(control);
        Serial.println(" triggered - Switching to TUSK mode");
        controller->getSynth().setSynthMode(HybridSynthesizer::TUSK);
        installBanksForMode(controller, HybridSynthesizer::TUSK);
        controller->getSynth().setSplitPoint(60);
        
        // Reset ADSR to defaults
        controller->getSynth().getSoundfont().setAttack(5.0f);
        controller->getSynth().getSoundfont().setDecay(200.0f);
        controller->getSynth().getSoundfont().setSustain(0.4f);
        controller->getSynth().getSoundfont().setRelease(300.0f);
        
        Serial.print("Mode: TUSK - Soundfont split mode (instrument 0 above split, instrument 1 below) (CC ");
        Serial.print(control);
        Serial.println(")");
        controller->getSynth().loadTuskInstruments();
    }
}

void CC_ModeIran(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        controller->getSynth().setSynthMode(HybridSynthesizer::IRAN);
        installBanksForMode(controller, HybridSynthesizer::IRAN);
        controller->getSynth().setSplitPoint(60);
        Serial.print("Mode: IRAN (CC ");
        Serial.print(control);
        Serial.println(")");
    }
}

void CC_ModeWhitesnake(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        Serial.print("CC ");
        Serial.print(control);
        Serial.println(" triggered - Switching to WHITESNAKE mode");
        controller->getSynth().setSynthMode(HybridSynthesizer::WHITESNAKE);
        installBanksForMode(controller, HybridSynthesizer::WHITESNAKE);
        // No state reset here: soundfont volume, pad CC params (octave mix,
        // velocity floor, HP mix, HP multiplier), and master volume all
        // persist from wherever the user left them. Boot defaults live in
        // SoundfontPadSynthesizer's constructor and the HybridSynthesizer
        // setup. Headroom for the VS samples + multi-voice sum is meant to
        // be absorbed at the pad's per-voice mixerA/B volume.
        controller->getSynth().loadWhitesnakeInstruments();
    }
}

void CC_ModeTuskChord(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        Serial.print("CC ");
        Serial.print(control);
        Serial.println(" triggered - Switching to TUSK_CHORD mode");
        controller->getSynth().setSynthMode(HybridSynthesizer::TUSK_CHORD);
        installBanksForMode(controller, HybridSynthesizer::TUSK_CHORD);
        controller->getSynth().setSplitPoint(60);
        
        // Reset ADSR to defaults
        controller->getSynth().getSoundfont().setAttack(5.0f);
        controller->getSynth().getSoundfont().setDecay(200.0f);
        controller->getSynth().getSoundfont().setSustain(0.4f);
        controller->getSynth().getSoundfont().setRelease(300.0f);
        
        Serial.print("Mode: TUSK_CHORD - Soundfont split mode with chord support (CC ");
        Serial.print(control);
        Serial.println(")");
        controller->getSynth().loadTuskInstruments();
        controller->getSynth().loadTuskTrumpetMap();
    }
}

void CC_CycleStringPadChordMode(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        StringPadSynthesizer::ChordMode currentMode = controller->getSynth().getStringPad().getChordMode();
        switch (currentMode) {
            case StringPadSynthesizer::CHORD_MODE_OFF:
                controller->getSynth().getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_MAJOR);
                Serial.print("String Pad Chord Mode: MAJOR - Playing major chords (CC ");
                Serial.print(control);
                Serial.println(")");
                break;
            case StringPadSynthesizer::CHORD_MODE_MAJOR:
                controller->getSynth().getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_OCTAVE);
                Serial.print("String Pad Chord Mode: OCTAVE - Playing octave notes (CC ");
                Serial.print(control);
                Serial.println(")");
                break;
            case StringPadSynthesizer::CHORD_MODE_OCTAVE:
                controller->getSynth().getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_OFF);
                Serial.print("String Pad Chord Mode: OFF - Playing single notes (CC ");
                Serial.print(control);
                Serial.println(")");
                break;
        }
    }
}

// ============================================================================
// Default Control Change Callback Tables (organized by banks)
// Bank N contains callbacks for CC numbers N*10 to N*10+9
// ============================================================================

// Bank 0: CC 0-9 - General controls
const MIDIControllerChannelCallback channel1Bank_01_10[] = {
    { 7,  CC_MasterVolume },
};
const size_t channel1Bank_01_10_count = sizeof(channel1Bank_01_10) / sizeof(channel1Bank_01_10[0]);

// Bank 1: CC 10-19 - (currently unused)
const MIDIControllerChannelCallback channel1Bank_11_20[] = {
};
const size_t channel1Bank_11_20_count = sizeof(channel1Bank_11_20) / sizeof(channel1Bank_11_20[0]);

// Bank 2: CC 20-29 - Attenuation, filter, and drone controls (non-soundfont modes)
const MIDIControllerChannelCallback channel1Bank_21_30[] = {
    { 21, CC_Attenuation },
    { 22, CC_FilterStrength },
    { 23, CC_DroneVolume },
    { 24, CC_FilterCutoff },
    { 25, CC_LFORate },
    { 26, CC_OscillatorDetune },
    { 27, CC_LFODepth },
};
const size_t channel1Bank_21_30_count = sizeof(channel1Bank_21_30) / sizeof(channel1Bank_21_30[0]);

// Bank 2 (alternate): CC 20-29 - Soundfont ADSR controls (soundfont modes)
const MIDIControllerChannelCallback channel1Bank_21_30_SF[] = {
    { 21, CC_SoundfontAttack },
    { 22, CC_SoundfontDecay },
    { 23, CC_SoundfontSustain },
    { 24, CC_SoundfontRelease },
    { 25, CC_SoundfontFilterFrequency },
    { 26, CC_SoundfontFilterResonance },
    { 27, CC_SoundfontCrossfadeDuration },
    { 28, CC_SoundfontVolume },
};
const size_t channel1Bank_21_30_SF_count = sizeof(channel1Bank_21_30_SF) / sizeof(channel1Bank_21_30_SF[0]);

// Bank 2 (alternate): CC 20-29 - Whitesnake pad ADSR (21-24), octave mix (25),
// velocity floor / dB-range (26), HP cutoff multiplier (27), HP branch mix (28).
// Velocity smoother runs at its fixed default tau (~2.7 s) -- no CC binding.
// Soundfont/pad volume (formerly CC 28) is not bound in this bank; use CC 7
// master volume in WHITESNAKE.
const MIDIControllerChannelCallback channel1Bank_21_30_PAD[] = {
    { 21, CC_WhitesnakePadAttack },
    { 22, CC_WhitesnakePadDecay },
    { 23, CC_WhitesnakePadSustain },
    { 24, CC_WhitesnakePadRelease },
    { 25, CC_WhitesnakeOctaveMix },
    { 26, CC_WhitesnakePadVelocityFloor },
    { 27, CC_WhitesnakePadHighpassMultiplier },
    { 28, CC_WhitesnakePadHighpassMix },
};
const size_t channel1Bank_21_30_PAD_count = sizeof(channel1Bank_21_30_PAD) / sizeof(channel1Bank_21_30_PAD[0]);

// Bank 3: CC 30-39 - (currently unused)
const MIDIControllerChannelCallback channel1Bank_31_40[] = {
};
const size_t channel1Bank_31_40_count = sizeof(channel1Bank_31_40) / sizeof(channel1Bank_31_40[0]);

// Bank 4: CC 40-49 - String pad controls
const MIDIControllerChannelCallback channel1Bank_41_50[] = {
    { 41, CC_StringPadVolume },
    { 42, CC_StringPadFilterCutoff },
    { 43, CC_StringPadFilterResonance },
    { 44, CC_StringPadDetuneAmount },
    { 45, CC_HighpassMultiplier },
};
const size_t channel1Bank_41_50_count = sizeof(channel1Bank_41_50) / sizeof(channel1Bank_41_50[0]);

// Bank 4 (alternate): CC 40-49 - Whitesnake pad FX (path B chain)
// Pair structure: low CC = baseline/amount, high CC = modulation/character.
// 41/42 = LP base cutoff multiplier / resonance (always-on static character)
// 43/44 = per-voice LP filter LFO depth / rate (free-running, decorrelated per voice)
// 45/46 = chorus wet level / chorus depth (depth re-inits flanges — clicks on change)
// 47/48 = reverb wet level / reverb roomsize
const MIDIControllerChannelCallback channel1Bank_41_50_PAD[] = {
    { 41, CC_WhitesnakeFilterCutoff },
    { 42, CC_WhitesnakeFilterResonance },
    { 43, CC_WhitesnakeFilterLfoDepth },
    { 44, CC_WhitesnakeFilterLfoRate },
    { 45, CC_WhitesnakeChorusMix },
    { 46, CC_WhitesnakeChorusDepth },
    { 47, CC_WhitesnakeReverbMix },
    { 48, CC_WhitesnakeReverbSize },
};
const size_t channel1Bank_41_50_PAD_count = sizeof(channel1Bank_41_50_PAD) / sizeof(channel1Bank_41_50_PAD[0]);

// Bank 5: CC 50-59 - Mode switching controls
const MIDIControllerChannelCallback channel1Bank_51_60[] = {
    { 51, CC_ModePluckedStrings },
    { 52, CC_ModeDrone },
    { 53, CC_ModeStringPadsBright },
    { 54, CC_ModeSoundfontTrombone },
    { 55, CC_ModeTusk },
    { 56, CC_ModeIran },
    { 57, CC_ModeWhitesnake },
    { 58, CC_ModeTuskChord },
    { 59, CC_CycleStringPadChordMode },
};
const size_t channel1Bank_51_60_count = sizeof(channel1Bank_51_60) / sizeof(channel1Bank_51_60[0]);

// ============================================================================
// Per-mode bank table assignments
// ============================================================================
//
// Each row maps a synth mode to the CC callback table that should be active
// in every swap-by-mode bank (today: banks 2 and 4). To add a new
// swap-by-mode bank in the future, extend ModeBankConfig with another
// (table, count) pair, populate the new column in every row below, and add
// an installCallbacks() line in installBanksForMode().

struct ModeBankConfig {
    HybridSynthesizer::SynthMode mode;
    const MIDIControllerChannelCallback* bank2;
    size_t bank2_count;
    const MIDIControllerChannelCallback* bank4;
    size_t bank4_count;
};

static const ModeBankConfig modeBankConfigs[] = {
    { HybridSynthesizer::PLUCKED_STRINGS, channel1Bank_21_30,     channel1Bank_21_30_count,     channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::DRONE,           channel1Bank_21_30,     channel1Bank_21_30_count,     channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::STRING_PADS,     channel1Bank_21_30,     channel1Bank_21_30_count,     channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::SOUNDFONT,       channel1Bank_21_30_SF,  channel1Bank_21_30_SF_count,  channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::SPLIT,           channel1Bank_21_30,     channel1Bank_21_30_count,     channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::IRAN,            channel1Bank_21_30,     channel1Bank_21_30_count,     channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::TUSK,            channel1Bank_21_30_SF,  channel1Bank_21_30_SF_count,  channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::TUSK_CHORD,      channel1Bank_21_30_SF,  channel1Bank_21_30_SF_count,  channel1Bank_41_50, channel1Bank_41_50_count },
    { HybridSynthesizer::WHITESNAKE,      channel1Bank_21_30_PAD, channel1Bank_21_30_PAD_count, channel1Bank_41_50_PAD, channel1Bank_41_50_PAD_count },
};

void installBanksForMode(MIDIController* controller, HybridSynthesizer::SynthMode mode) {
    for (const ModeBankConfig& cfg : modeBankConfigs) {
        if (cfg.mode == mode) {
            controller->installCallbacks(2, cfg.bank2, cfg.bank2_count);
            controller->installCallbacks(4, cfg.bank4, cfg.bank4_count);
            return;
        }
    }
    Serial.printf("installBanksForMode: no config for mode %d\n", (int)mode);
}
