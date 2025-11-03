#include "MIDIController.h"

// Project headers
#include "StringPadSynthesizer.h"

// Static instance pointer for C-style callbacks
static MIDIController* midiControllerInstance = nullptr;

// C-style callback wrappers
static void OnNoteOnWrapper(byte channel, byte note, byte velocity) {
    if (midiControllerInstance) {
        midiControllerInstance->handleNoteOn(channel, note, velocity);
    }
}

static void OnNoteOffWrapper(byte channel, byte note, byte velocity) {
    if (midiControllerInstance) {
        midiControllerInstance->handleNoteOff(channel, note, velocity);
    }
}

static void OnControlChangeWrapper(byte channel, byte control, byte value) {
    if (midiControllerInstance) {
        midiControllerInstance->handleControlChange(channel, control, value);
    }
}

static void OnPitchChangeWrapper(byte channel, int bend) {
    if (midiControllerInstance) {
        midiControllerInstance->handlePitchChange(channel, bend);
    }
}

MIDIController::MIDIController(HybridSynthesizer& synth, USBHost& usbHost, MIDIDevice& midiDevice)
    : synth(synth), usbHost(usbHost), midiDevice(midiDevice) {
    midiControllerInstance = this;
}

void MIDIController::begin() {
    usbHost.begin();
    midiDevice.setHandleNoteOff(OnNoteOffWrapper);
    midiDevice.setHandleNoteOn(OnNoteOnWrapper);
    midiDevice.setHandleControlChange(OnControlChangeWrapper);
    midiDevice.setHandlePitchChange(OnPitchChangeWrapper);
}

void MIDIController::update() {
    usbHost.Task();
    midiDevice.read();
}

float MIDIController::midiNoteToFrequency(byte note) {
    return 440.0f * exp2f((float)(note - 69) * 0.0833333f);
}

float MIDIController::midiVelocityToFloat(byte velocity) {
    return (float)velocity / 128.0f;
}

void MIDIController::handleNoteOn(byte channel, byte note, byte velocity) {
    Serial.print("Note On, ch=");
    Serial.print(channel);
    Serial.print(", note=");
    Serial.print(note);
    Serial.print(", velocity=");
    Serial.print(velocity);
    Serial.println();

    float freq = midiNoteToFrequency(note);
    float vel = midiVelocityToFloat(velocity);
    Serial.print("Synthesizer.noteOn(");
    Serial.print(freq);
    Serial.print(", ");
    Serial.print(vel);
    Serial.print(")");
    Serial.println();
    synth.noteOn(channel, note, freq, vel);
}

void MIDIController::handleNoteOff(byte channel, byte note, byte velocity) {
    Serial.print("Note Off, ch=");
    Serial.print(channel);
    Serial.print(", note=");
    Serial.print(note);
    Serial.println();
    synth.noteOff(channel, note);
}

void MIDIController::handleControlChange(byte channel, byte control, byte value) {
    Serial.print("Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", value=");
    Serial.print(value);
    Serial.println();

    if (channel == 1 && control == 21)
    {
        synth.setAttenuation(value);
    }
    if (channel == 1 && control == 22)
    {
        synth.setFilterStrength(value);
    }

    if (control == 7)
    {
        synth.setMasterVolume((float)value / 64.0f);
        Serial.printf("Master volume: %.2f\n", (float)value / 64.0f);
    }

    // Drone-specific controls
    if (channel == 1 && control == 23) // CC 23 - Drone Volume
    {
        synth.setDroneVolume((float)value / 127.0f);
        Serial.printf("Drone volume: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 24) // CC 24 - Filter Cutoff
    {
        synth.getDrone().setFilterCutoff((float)value / 127.0f);
        Serial.printf("Filter cutoff: %.2f\n", (float)value / 127.0f);
    }

    if (channel == 1 && control == 25) // CC 25 - LFO Rate
    {
        float lfoRate;
        if (value == 0) {
            lfoRate = 0.0f; // Disable LFO when value is 0
        } else {
            lfoRate = ((float)value / 127.0f) * 10.0f; // 0 to 10 Hz
        }
        synth.getDrone().setLFORate(lfoRate);
        if (lfoRate == 0.0f) {
            Serial.println("LFO disabled");
        } else {
            Serial.printf("LFO rate: %.2f Hz\n", lfoRate);
        }
    }

    if (channel == 1 && control == 26) // CC 26 - Oscillator Detune
    {
        float detune = ((float)value / 128.0f - 0.5f) * 2.0f; // -1.0 to +1.0
        synth.getDrone().setOscillatorDetune(detune);
        Serial.printf("Oscillator detune: %.3f semitones\n", detune);
    }

    if (channel == 1 && control == 27) // CC 27 - LFO Depth
    {
        synth.getDrone().setLFODepth((float)value / 127.0f);
        Serial.printf("LFO depth: %.2f\n", (float)value / 127.0f);
    }

    // String pad-specific controls
    if (channel == 1 && control == 41) // CC 41 - String Pad Volume
    {
        synth.setStringPadVolume((float)value / 127.0f);
        Serial.printf("String pad volume: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 42) // CC 42 - String Pad Filter Cutoff
    {
        synth.getStringPad().setFilterCutoff((float)value / 127.0f);
        Serial.printf("String pad filter cutoff: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 43) // CC 43 - String Pad Filter Resonance
    {
        synth.getStringPad().setFilterResonance((float)value / 127.0f);
        Serial.printf("String pad filter resonance: %.2f\n", (float)value / 127.0f);
    }
    
    if (channel == 1 && control == 44) // CC 44 - String Pad Detune Amount
    {
        synth.getStringPad().setDetuneAmount((float)value / 127.0f);
        Serial.printf("String pad detune amount: %.2f\n", (float)value / 127.0f);
    }

    // Mode Switching Controls
    if (channel == 1 && control == 51 && value == 127) // CC 51 - Plucked Strings
    {
        synth.setSynthMode(HybridSynthesizer::PLUCKED_STRINGS);
        Serial.println("Mode: PLUCKED_STRINGS (CC 51)");
    }
    
    if (channel == 1 && control == 52 && value == 127) // CC 52 - Drone
    {
        synth.setSynthMode(HybridSynthesizer::DRONE);
        Serial.println("Mode: DRONE (CC 52)");
    }
    
    // String Pad Mode with Presets (CC 53-57) - Switch to String Pads and load preset
    if (channel == 1 && control == 53 && value == 127) // CC 53 - String Pads: Bright Strings
    {
        synth.setSynthMode(HybridSynthesizer::STRING_PADS);
        synth.getStringPad().loadPreset(StringPadSynthesizer::PRESET_BRIGHT_STRINGS);
        Serial.println("Mode: STRING_PADS + Bright Strings preset (CC 53)");
    }
    
    if (channel == 1 && control == 54 && value == 127) // CC 54 - Soundfont mode: trumpet.sf2
    {
        Serial.println("CC 54 triggered - Switching to Soundfont mode");
        synth.setSynthMode(HybridSynthesizer::SOUNDFONT);
        Serial.println("Mode switched to SOUNDFONT, attempting to load trumpet.sf2...");
        bool success = synth.getSoundfont().loadInstrument("trombone.sf2", 0);
        if (success) {
            Serial.println("Mode: SOUNDFONT + trumpet.sf2 (CC 54) - SUCCESS");
        } else {
            Serial.println("Mode: SOUNDFONT + trumpet.sf2 (CC 54) - FAILED");
        }
    }
    
    if (channel == 1 && control == 58 && value == 127) // CC 58 - Split Mode
    {
        synth.setSynthMode(HybridSynthesizer::SPLIT);
        Serial.println("Mode: SPLIT - Drone below C5, String Pads above (CC 58)");
    }
    
    if (channel == 1 && control == 59 && value == 127) // CC 59 - Cycle String Pad Chord Mode
    {
        StringPadSynthesizer::ChordMode currentMode = synth.getStringPad().getChordMode();
        switch (currentMode) {
            case StringPadSynthesizer::CHORD_MODE_OFF:
                synth.getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_MAJOR);
                Serial.println("String Pad Chord Mode: MAJOR - Playing major chords (CC 59)");
                break;
            case StringPadSynthesizer::CHORD_MODE_MAJOR:
                synth.getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_OCTAVE);
                Serial.println("String Pad Chord Mode: OCTAVE - Playing octave notes (CC 59)");
                break;
            case StringPadSynthesizer::CHORD_MODE_OCTAVE:
                synth.getStringPad().setChordMode(StringPadSynthesizer::CHORD_MODE_OFF);
                Serial.println("String Pad Chord Mode: OFF - Playing single notes (CC 59)");
                break;
        }
    }
    
    if (channel == 1 && control == 45) // CC 45 - Highpass Filter Multiplier
    {
        // Exponential mapping: CC 0 -> 0.05, CC 64 -> 1.0, CC 127 -> 4.0
        float multiplier;
        
        if (value <= 64) {
            // CC 0-64: map from 0.05 to 1.0 exponentially
            float t = (float)value / 64.0f;  // 0.0 to 1.0
            multiplier = 0.05f * pow(20.0f, t);  // 0.05 * (20^t) gives 0.05 to 1.0
        } else {
            // CC 65-127: map from 1.0 to 4.0 exponentially  
            float t = (float)(value - 64) / 63.0f;  // 0.0 to 1.0
            multiplier = 1.0f * pow(4.0f, t);  // 1.0 * (4^t) gives 1.0 to 4.0
        }
        
        synth.getStringPad().setHighpassMultiplier(multiplier);
        Serial.print("Highpass Multiplier: ");
        Serial.print(multiplier, 3);
        Serial.print(" (CC 45 = ");
        Serial.print(value);
        Serial.println(")");
    }
}

void MIDIController::handlePitchChange(byte channel, int bend) {
    Serial.print("Pitch Bend, ch=");
    Serial.print(channel);
    Serial.print(", bend=");
    Serial.println(bend);

    // Convert MIDI pitch bend (-8192 to +8191) to -4.0-4.0 range
    float bendAmount = (float)bend / 8192.0f * 4.0f;
    synth.setPitchBend(bendAmount);
}
