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
        installBank2ForMode(controller, false);
        Serial.print("Mode: PLUCKED_STRINGS (CC ");
        Serial.print(control);
        Serial.println(")");
    }
}

void CC_ModeDrone(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        controller->getSynth().setSynthMode(HybridSynthesizer::DRONE);
        installBank2ForMode(controller, false);
        Serial.print("Mode: DRONE (CC ");
        Serial.print(control);
        Serial.println(")");
    }
}

void CC_ModeStringPadsBright(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        controller->getSynth().setSynthMode(HybridSynthesizer::STRING_PADS);
        installBank2ForMode(controller, false);
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
        controller->getSynth().setSynthMode(HybridSynthesizer::SOUNDFONT);        installBank2ForMode(controller, true);        
        Serial.println("Unloading all instruments...");
        for (int i = 0; i < 4; i++) {
            controller->getSynth().getSoundfont().unloadInstrument(i);
        }
        
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
        installBank2ForMode(controller, true);
        controller->getSynth().setSplitPoint(60);
        Serial.print("Mode: TUSK - Soundfont split mode (instrument 0 above split, instrument 1 below) (CC ");
        Serial.print(control);
        Serial.println(")");
        controller->getSynth().loadTuskInstruments();
    }
}

void CC_ModeSoundfontTromboneTusk(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        Serial.print("CC ");
        Serial.print(control);
        Serial.println(" triggered - Switching to Soundfont mode");
        controller->getSynth().setSynthMode(HybridSynthesizer::SOUNDFONT);
        installBank2ForMode(controller, true);
        
        Serial.println("Unloading all instruments...");
        for (int i = 0; i < 4; i++) {
            controller->getSynth().getSoundfont().unloadInstrument(i);
        }
        
        Serial.println("Mode switched to SOUNDFONT, loading trombone_tusk.sf2...");
        bool success = controller->getSynth().getSoundfont().loadInstrument(1, "trombone_tusk.sf2", 0);
        if (success) {
            Serial.print("Mode: SOUNDFONT + trombone_tusk.sf2 (CC ");
            Serial.print(control);
            Serial.println(") - SUCCESS");
        } else {
            Serial.print("Mode: SOUNDFONT + trombone_tusk.sf2 (CC ");
            Serial.print(control);
            Serial.println(") - FAILED");
        }
        controller->getSynth().setDefaultInstrument(1);
    }
}

void CC_ModeSoundfontTrumpetTusk(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        Serial.print("CC ");
        Serial.print(control);
        Serial.println(" triggered - Switching to Soundfont mode");
        controller->getSynth().setSynthMode(HybridSynthesizer::SOUNDFONT);
        installBank2ForMode(controller, true);
        
        Serial.println("Unloading all instruments...");
        for (int i = 0; i < 4; i++) {
            controller->getSynth().getSoundfont().unloadInstrument(i);
        }
        
        Serial.println("Mode switched to SOUNDFONT, loading trumpet_tusk.sf2...");
        bool success = controller->getSynth().getSoundfont().loadInstrument(0, "trumpet_tusk.sf2", 0);
        if (success) {
            Serial.print("Mode: SOUNDFONT + trumpet_tusk.sf2 (CC ");
            Serial.print(control);
            Serial.println(") - SUCCESS");
        } else {
            Serial.print("Mode: SOUNDFONT + trumpet_tusk.sf2 (CC ");
            Serial.print(control);
            Serial.println(") - FAILED");
        }
        controller->getSynth().setDefaultInstrument(0);
    }
}

void CC_ModeTuskChord(MIDIController* controller, byte channel, byte control, byte value) {
    if (value == 127) {
        Serial.print("CC ");
        Serial.print(control);
        Serial.println(" triggered - Switching to TUSK_CHORD mode");
        controller->getSynth().setSynthMode(HybridSynthesizer::TUSK_CHORD);
        installBank2ForMode(controller, true);
        controller->getSynth().setSplitPoint(60);
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
};
const size_t channel1Bank_21_30_SF_count = sizeof(channel1Bank_21_30_SF) / sizeof(channel1Bank_21_30_SF[0]);

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

// Bank 5: CC 50-59 - Mode switching controls
const MIDIControllerChannelCallback channel1Bank_51_60[] = {
    { 51, CC_ModePluckedStrings },
    { 52, CC_ModeDrone },
    { 53, CC_ModeStringPadsBright },
    { 54, CC_ModeSoundfontTrombone },
    { 55, CC_ModeTusk },
    { 56, CC_ModeSoundfontTromboneTusk },
    { 57, CC_ModeSoundfontTrumpetTusk },
    { 58, CC_ModeTuskChord },
    { 59, CC_CycleStringPadChordMode },
};
const size_t channel1Bank_51_60_count = sizeof(channel1Bank_51_60) / sizeof(channel1Bank_51_60[0]);

// ============================================================================
// Helper Functions
// ============================================================================

void installBank2ForMode(MIDIController* controller, bool isSoundfontMode) {
    if (isSoundfontMode) {
        controller->installCallbacks(2, channel1Bank_21_30_SF, channel1Bank_21_30_SF_count);
    } else {
        controller->installCallbacks(2, channel1Bank_21_30, channel1Bank_21_30_count);
    }
}
