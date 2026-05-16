#pragma once

// Standard library
#include <map>

// Third-party libraries
#include <Audio.h>
#include <output_i2s.h>

// Project headers
#include "AudioPeakMonitor.h"
#include "DroneSynthesizer.h"
#include "StringsSynthesizer.h"
#include "StringPadSynthesizer.h"
#include "SoundfontSynthesizer.h"
#include "SoundfontPadSynthesizer.h"

class HybridSynthesizer {
public:
    enum SynthMode {
        PLUCKED_STRINGS,    // Karplus-Strong plucked strings (renamed from STRINGS_ONLY)
        DRONE,              // Analog-style drone synthesizer  
        STRING_PADS,        // Classic 80s string pads
        SOUNDFONT,          // SoundFont (.sf2) synthesizer
        SPLIT,              // Split mode: drone below split point, string pads above
        IRAN,               // Special mode for I Ran
        TUSK,               // Soundfont split mode: instrument 0 above split, instrument 1 below
        TUSK_CHORD,         // Soundfont split mode with chord support (same as TUSK for now)
        WHITESNAKE          // Whitesnake VS pad: soundfont instrument 0 from whitesnake.sf2
    };

private:
    // Karplus-Strong string synthesis
    StringsSynthesizer strings;
    
    // Drone synthesizer
    DroneSynthesizer drone;
    
    // String pad synthesizer
    StringPadSynthesizer stringPad;
    
    // Soundfont synthesizer
    SoundfontSynthesizer soundfont;

    // Whitesnake pad synthesizer (lean, no filter/crossfade, 8 voices)
    SoundfontPadSynthesizer whitesnakePad;
    
    // Audio mixing and output
    AudioMixer4 mixerL4, mixerR4;  // Drone mixers
    AudioMixer4 mixerL5, mixerR5;  // String pad mixers
    AudioMixer4 mixerL6, mixerR6;  // Soundfont mixers
    AudioMixer4 sumL, sumR;
    AudioPeakMonitor peakMonitorL, peakMonitorR;  // Peak monitors for left and right channels
    AudioPeakMonitor whitesnakePadPeakMonitor;    // Peak monitor for the Whitesnake pad (mono)
    AudioOutputI2S i2s1;
#ifdef USB_AUDIO
    AudioOutputUSB usb2;
#endif

    // Audio connections
    AudioConnection* stringPatchCordL;     // Strings to mixer L
    AudioConnection* stringPatchCordR;     // Strings to mixer R
    AudioConnection* dronePatchCordL;      // Drone to mixer
    AudioConnection* dronePatchCordR;      // Drone to mixer
    AudioConnection* stringPadPatchCordL;  // String pad to mixer
    AudioConnection* stringPadPatchCordR;  // String pad to mixer
    AudioConnection* soundfontPatchCordL;  // Soundfont to mixer
    AudioConnection* soundfontPatchCordR;  // Soundfont to mixer
    AudioConnection* whitesnakePadPatchCordL;  // Whitesnake pad to mixer
    AudioConnection* whitesnakePadPatchCordR;  // Whitesnake pad to mixer
    AudioConnection* patchCordSumL1, *patchCordSumL4, *patchCordSumL5, *patchCordSumL6;
    AudioConnection* patchCordSumR1, *patchCordSumR4, *patchCordSumR5, *patchCordSumR6;
    AudioConnection* patchCordMonitorL, *patchCordMonitorR;  // Connections to peak monitors
    AudioConnection* patchCordWhitesnakePadMonitor;          // Whitesnake pad to its peak monitor
    AudioConnection* patchCordL;
    AudioConnection* patchCordR;
    AudioConnection *patchCordUsbL;
    AudioConnection *patchCordUsbR;
#
    std::map<int, int> keyToString; // key -> string index (deprecated - managed internally now)
    std::map<int, float> keyToBaseFreq; // key -> base frequency (deprecated - managed internally now)
    float pitchBendFactor = 1.0f; // Current pitch bend multiplier (deprecated - managed internally now)
    float masterVolume = 1.0f;
    float stringVolume = 1.0f;
    float droneVolume = 1.0f;
    float stringPadVolume = 1.0f;
    float soundfontVolume = 1.0f;
    
    SynthMode currentMode = TUSK_CHORD; 
    uint8_t splitPoint = 60; // C4
    int defaultInstrument = 0; // Default instrument slot for SOUNDFONT mode
    
    // TUSK_CHORD mode: static chord mapping structure for 12 semitones
    static const int MAX_CHORD_NOTES = 8;
    static const int NUM_SEMITONES = 12;
    
    struct ChordNotes {
        int8_t offsets[MAX_CHORD_NOTES];  // Semitone offsets from root note
        uint8_t count;                      // Number of notes in the chord
    };
    
    ChordNotes chordPatterns[NUM_SEMITONES]; // One pattern per semitone (0-11)
    
    struct ActiveChord {
        int8_t notes[MAX_CHORD_NOTES];  // Actual MIDI notes being played
        uint8_t count;                   // Number of notes
    };
    ActiveChord activeChords[128]; // Track active chords for each MIDI key 

public:
    HybridSynthesizer() :
        stringPatchCordL(nullptr),
        stringPatchCordR(nullptr),
        dronePatchCordL(nullptr),
        dronePatchCordR(nullptr),
        stringPadPatchCordL(nullptr),
        stringPadPatchCordR(nullptr),
        soundfontPatchCordL(nullptr),
        soundfontPatchCordR(nullptr),
        whitesnakePadPatchCordL(nullptr),
        whitesnakePadPatchCordR(nullptr),
        patchCordSumL1(nullptr), patchCordSumL4(nullptr), patchCordSumL5(nullptr), patchCordSumL6(nullptr),
        patchCordSumR1(nullptr), patchCordSumR4(nullptr), patchCordSumR5(nullptr), patchCordSumR6(nullptr),
        patchCordMonitorL(nullptr), patchCordMonitorR(nullptr),
        patchCordWhitesnakePadMonitor(nullptr),
        patchCordL(nullptr),
        patchCordR(nullptr),
        patchCordUsbL(nullptr),
        patchCordUsbR(nullptr)
    {
        // Initialize chord patterns to empty
        for (int i = 0; i < NUM_SEMITONES; i++) {
            chordPatterns[i].count = 0;
        }
        
        // Initialize active chords to empty
        for (int i = 0; i < 128; i++) {
            activeChords[i].count = 0;
        }
        
        // Connect strings to sum mixers
        stringPatchCordL = new AudioConnection(*strings.getLeftOutput(), 0, sumL, 0);
        stringPatchCordR = new AudioConnection(*strings.getRightOutput(), 0, sumR, 0);
        
        // Connect drone to mixerL4 and mixerR4
        dronePatchCordL = new AudioConnection(*drone.getLeftOutput(), 0, mixerL4, 0);
        dronePatchCordR = new AudioConnection(*drone.getRightOutput(), 0, mixerR4, 0);
        
        // Connect string pad to mixerL5 and mixerR5 (mono to both channels)
        stringPadPatchCordL = new AudioConnection(*stringPad.getOutput(), 0, mixerL5, 0);
        stringPadPatchCordR = new AudioConnection(*stringPad.getOutput(), 0, mixerR5, 0);
        
        // Connect soundfont to mixerL6 and mixerR6 (mono to both channels)
        soundfontPatchCordL = new AudioConnection(*soundfont.getLeftOutput(), 0, mixerL6, 0);
        soundfontPatchCordR = new AudioConnection(*soundfont.getRightOutput(), 0, mixerR6, 0);

        // Whitesnake pad shares mixerL6/R6 (slot 1) with soundfont — modes are mutually exclusive
        whitesnakePadPatchCordL = new AudioConnection(*whitesnakePad.getOutput(), 0, mixerL6, 1);
        whitesnakePadPatchCordR = new AudioConnection(*whitesnakePad.getOutput(), 0, mixerR6, 1);
        
        // Sum all mixers
        patchCordSumL1 = new AudioConnection(mixerL4, 0, sumL, 1);
        patchCordSumL4 = new AudioConnection(mixerL5, 0, sumL, 2);
        patchCordSumL6 = new AudioConnection(mixerL6, 0, sumL, 3);
        
        patchCordSumR1 = new AudioConnection(mixerR4, 0, sumR, 1);
        patchCordSumR4 = new AudioConnection(mixerR5, 0, sumR, 2);
        patchCordSumR6 = new AudioConnection(mixerR6, 0, sumR, 3);
        
        // Set initial mixer gains
        updateMixerGains();
        
        // Connect sum outputs to peak monitors
        patchCordMonitorL = new AudioConnection(sumL, 0, peakMonitorL, 0);
        patchCordMonitorR = new AudioConnection(sumR, 0, peakMonitorR, 0);

        // Tap the Whitesnake pad output for its own peak monitor (mono)
        patchCordWhitesnakePadMonitor = new AudioConnection(*whitesnakePad.getOutput(), 0, whitesnakePadPeakMonitor, 0);
        
        // Final output to I2S
        patchCordL = new AudioConnection(sumL, 0, i2s1, 0);
        patchCordR = new AudioConnection(sumR, 0, i2s1, 1);

#ifdef USB_AUDIO
        patchCordUsbL = new AudioConnection(sumL, 0, usb2, 0);
        patchCordUsbR = new AudioConnection(sumR, 0, usb2, 1);
#endif
    }

    ~HybridSynthesizer() {
        delete stringPatchCordL;
        delete stringPatchCordR;
        delete dronePatchCordL;
        delete dronePatchCordR;
        delete stringPadPatchCordL;
        delete stringPadPatchCordR;
        delete soundfontPatchCordL;
        delete soundfontPatchCordR;
        delete whitesnakePadPatchCordL;
        delete whitesnakePadPatchCordR;
        delete patchCordSumL1;
        delete patchCordSumL4;
        delete patchCordSumL5;
        delete patchCordSumL6;
        delete patchCordSumR1;
        delete patchCordSumR4;
        delete patchCordSumR5;
        delete patchCordSumR6;
        delete patchCordMonitorL;
        delete patchCordMonitorR;
        delete patchCordWhitesnakePadMonitor;
        delete patchCordL;
        delete patchCordR;
        delete patchCordUsbL;
        delete patchCordUsbR;
    }
    
    // Synthesis mode control
    SynthMode getCurrentMode() const { return currentMode; }

    void setSynthMode(SynthMode mode) {
        // Print current peak levels before switching modes
        const char* modeNames[] = {"PLUCKED_STRINGS", "DRONE", "STRING_PADS", "SOUNDFONT", "SPLIT", "IRAN", "TUSK", "TUSK_CHORD", "WHITESNAKE"};
        constexpr size_t numModeNames = sizeof(modeNames) / sizeof(modeNames[0]);
        const char* currentModeName = (currentMode >= 0 && (size_t)currentMode < numModeNames) ? modeNames[currentMode] : "UNKNOWN";
        const char* newModeName = (mode >= 0 && (size_t)mode < numModeNames) ? modeNames[mode] : "UNKNOWN";
        
        Serial.print("Switching from ");
        Serial.print(currentModeName);
        Serial.print(" to ");
        Serial.print(newModeName);
        Serial.print(" - ");
        printPeakLevels();
        
        currentMode = mode; 
        resetPeakMonitors(); // Reset peak monitors when mode changes
    }
    
    void setSplitPoint(uint8_t note) { splitPoint = note; }
    void setDefaultInstrument(int instrumentSlot) { defaultInstrument = instrumentSlot; }
    
    // Volume controls
    void setMasterVolume(float volume) {
        masterVolume = volume;
        updateMixerGains();
    }
    
    void setStringVolume(float volume) {
        stringVolume = volume;
        updateMixerGains();
    }
    
    void setDroneVolume(float volume) {
        drone.setVolume(volume);
    }
    
    void setStringPadVolume(float volume) {
        stringPadVolume = volume;
        updateMixerGains();
    }
    
    void setSoundfontVolume(float volume) {
        soundfontVolume = volume;
        updateMixerGains();
    }
    
    // Soundfont synthesizer access
    SoundfontSynthesizer& getSoundfont() { return soundfont; }
    bool initializeSoundfont() { return soundfont.begin(); }
    bool loadSoundfontInstrument(int instrumentSlot, const char* filename, int instrumentIndex) {
        return soundfont.loadInstrument(instrumentSlot, filename, instrumentIndex);
    }
    void loadTuskInstruments() {
        // Always unload all instruments and load fresh
        Serial.println("Unloading all instruments...");
        for (int i = 0; i < 4; i++) {
            soundfont.unloadInstrument(i);
        }

        // Load instrument 0 from trombone_tusk.sf2 into slot 1
        bool success1 = soundfont.loadInstrument(1, "trombone_tusk.sf2", 0);
        if (success1) {
            Serial.println("Instrument 0 from trombone_tusk.sf2 loaded successfully into slot 1");
        } else {
            Serial.println("Failed to load instrument 0 from trombone_tusk.sf2 into slot 1");
        }

        // Load instrument 0 from trumpet_tusk.sf2 into slot 0
        bool success2 = soundfont.loadInstrument(0, "trumpet_tusk.sf2", 0);
        if (success2) {
            Serial.println("Instrument 0 from trumpet_tusk.sf2 loaded successfully into slot 0");
        } else {
            Serial.println("Failed to load instrument 0 from trumpet_tusk.sf2 into slot 0");
        }
    }
    
    void loadTuskTrumpetMap() {
        // Clear existing chord patterns
        for (int i = 0; i < NUM_SEMITONES; i++) {
            chordPatterns[i].count = 0;
        }
        
        // Load Tusk Trumpet chord mappings (semitones: C=0, C#=1, D=2, D#=3, E=4, F=5, F#=6, G=7, G#=8, A=9, A#=10, B=11)
        // A -> A, D, F, A-2oct, D-2oct (offsets: 0, +5, +8, -24, -19)
        chordPatterns[9].offsets[0] = 0;
        chordPatterns[9].offsets[1] = 5;
        chordPatterns[9].offsets[2] = 8;
        chordPatterns[9].offsets[3] = -24;
        chordPatterns[9].offsets[4] = -19;
        chordPatterns[9].count = 5;
        
        // G -> G, C, E, G-2oct, C-2oct (offsets: 0, +5, +9, -24, -19)
        chordPatterns[7].offsets[0] = 0;
        chordPatterns[7].offsets[1] = 5;
        chordPatterns[7].offsets[2] = 9;
        chordPatterns[7].offsets[3] = -24;
        chordPatterns[7].offsets[4] = -19;
        chordPatterns[7].count = 5;
        
        // F -> F, A, D, F-2oct, A-2oct (offsets: 0, +4, +9, -24, -20)
        chordPatterns[5].offsets[0] = 0;
        chordPatterns[5].offsets[1] = 4;
        chordPatterns[5].offsets[2] = 9;
        chordPatterns[5].offsets[3] = -24;
        chordPatterns[5].offsets[4] = -20;
        chordPatterns[5].count = 5;
        
        // B -> G, B, D, G-2oct, B-2oct (offsets: -4, 0, +3, -28, -24)
        chordPatterns[11].offsets[0] = -4;
        chordPatterns[11].offsets[1] = 0;
        chordPatterns[11].offsets[2] = 3;
        chordPatterns[11].offsets[3] = -28;
        chordPatterns[11].offsets[4] = -24;
        chordPatterns[11].count = 5;
        
        // C# -> A, C#, E, A-2oct, C#-2oct (offsets: -4, 0, +3, -28, -24)
        chordPatterns[1].offsets[0] = -4;
        chordPatterns[1].offsets[1] = 0;
        chordPatterns[1].offsets[2] = 3;
        chordPatterns[1].offsets[3] = -28;
        chordPatterns[1].offsets[4] = -24;
        chordPatterns[1].count = 5;
        
        Serial.println("Tusk Trumpet chord map loaded");
    }

    void loadWhitesnakeInstruments() {
        Serial.println("Loading Whitesnake VS Pad...");
        // Load via SoundfontSynthesizer slot 0 — same path that already works.
        // Reusing slot 0's reader frees its prior samples (FreePrevSampleData) so the
        // global SF22ASWT::samples_usedRam budget gets recycled properly.
        bool ok = soundfont.loadInstrument(0, "whitesnake.sf2", 0);
        if (ok) {
            // Hand the loaded instrument_data pointer to the pad synth (borrowed; not owned)
            whitesnakePad.setInstrumentData(soundfont.getInstrumentData(0));
        } else {
            Serial.println("Failed to load whitesnake.sf2 via SoundfontSynthesizer");
            whitesnakePad.setInstrumentData(nullptr);
        }

        // Pad envelope on top of the sample's natural envelope
        whitesnakePad.setAttack(5.0f);
        whitesnakePad.setDecay(0.0f);
        whitesnakePad.setSustain(1.0f);
        whitesnakePad.setRelease(800.0f);
    }

    // TUSK_CHORD helper methods
    void setChordPattern(int semitone, const int8_t* offsets, int noteCount) {
        // Set chord pattern for a specific semitone (0-11)
        if (semitone < 0 || semitone >= NUM_SEMITONES) return;
        
        chordPatterns[semitone].count = min(noteCount, MAX_CHORD_NOTES);
        for (int i = 0; i < chordPatterns[semitone].count; i++) {
            chordPatterns[semitone].offsets[i] = offsets[i];
        }
    }
    
    void getChordForNote(int note, ActiveChord& outChord) {
        // Get semitone (0-11) from MIDI note
        int semitone = note % NUM_SEMITONES;
        
        // Check if there's a chord pattern for this semitone
        if (chordPatterns[semitone].count > 0) {
            outChord.count = chordPatterns[semitone].count;
            for (int i = 0; i < outChord.count; i++) {
                outChord.notes[i] = note + chordPatterns[semitone].offsets[i];
            }
        } else {
            // No pattern, return single note
            outChord.count = 1;
            outChord.notes[0] = note;
        }
    }
    
    // Synthesizer access
    DroneSynthesizer& getDrone() { return drone; }
    StringPadSynthesizer& getStringPad() { return stringPad; }
    SoundfontPadSynthesizer& getWhitesnakePad() { return whitesnakePad; }

    // Processing - call this regularly from main loop
    void update() {
        drone.processEnvelopes();
        stringPad.processEnvelope();
    }

    HybridSynthesizer::SynthMode modeFromChannel(byte channel, HybridSynthesizer::SynthMode defaultMode)
    {
        auto mode = currentMode;
        switch (channel)
        {
        case 2:
            mode = PLUCKED_STRINGS;
            break;
        case 3:
            mode = DRONE;
            break;
        case 4:
            mode = STRING_PADS;
            break;
        case 5:
            mode = SOUNDFONT;
            break;
        case 6:
            mode = SPLIT;
            break;
        case 7:
            mode = IRAN;
            break;
        case 8:
            mode = TUSK;
            break;
        case 9:
            mode = WHITESNAKE;
            break;
        default:
            break;
        }
        return mode;
    }

    void noteOn(byte channel, int key, float freq, float velocity) {
        auto mode = modeFromChannel(channel, currentMode);

        switch (mode) {
            case PLUCKED_STRINGS:
                playStringNote(key, freq, velocity);
                break;
            case DRONE:
                // Convert key to MIDI note and use polyphonic interface
                drone.noteOn(key, velocity);
                break;
            case STRING_PADS:
                // Pass the current chord mode directly
                stringPad.noteOn(key, velocity, StringPadSynthesizer::CHORD_MODE_OFF);
                break;
            case SOUNDFONT:
                // Use soundfont synthesizer with configurable default instrument
                soundfont.noteOn(defaultInstrument, key, velocity);
                break;
            case SPLIT:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOn(key, velocity);
                }
                else
                {
                    stringPad.noteOn(key, velocity, StringPadSynthesizer::CHORD_MODE_OFF);
                }
                break;
            case IRAN:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOn(key, 1.0f);
                }
                else
                {
                    // Use chord mode for specific keys in split mode
                    StringPadSynthesizer::ChordMode chordMode = StringPadSynthesizer::CHORD_MODE_OFF;
                    if (key >= 53 && key <= 57)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_MAJOR;
                    }
                    else if (key == 60)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_OCTAVE;
                    }
                    key += 24;
                    stringPad.noteOn(key, 1.0f, chordMode);
                }
                break;
            case TUSK:
                if (key < splitPoint)
                {
                    soundfont.noteOn(1, key + 12, velocity); // Instrument 1 below split point
                }
                else
                {
                    soundfont.noteOn(0, key, velocity); // Instrument 0 above split point
                }
                break;
            case TUSK_CHORD:
                if (key < splitPoint)
                {
                    // Below split point: play single note (no chord) at max velocity
                    soundfont.noteOn(1, key + 12, 1.0f); // Instrument 1 below split point, max velocity
                    activeChords[key].count = 0; // Mark as no chord
                }
                else
                {
                    // Above split point: play chord at max velocity
                    ActiveChord chord;
                    getChordForNote(key, chord);
                    activeChords[key] = chord; // Store for noteOff

                    for (int i = 0; i < chord.count; i++) {
                        int chordNote = chord.notes[i];
                        if (chordNote < splitPoint)
                        {
                            soundfont.noteOn(1, chordNote + 12, 1.0f); // Instrument 1 below split point, max velocity
                        }
                        else
                        {
                            soundfont.noteOn(0, chordNote, 1.0f); // Instrument 0 above split point, max velocity
                        }
                    }
                }
                break;
            case WHITESNAKE:
                whitesnakePad.noteOn(key, velocity);
                break;
            }
    }

    void noteOff(byte channel, int key) {
        auto mode = modeFromChannel(channel, currentMode);

        switch (mode) {
            case PLUCKED_STRINGS:
                // Delegate to StringsSynthesizer
                strings.noteOff(key);
                break;
            case DRONE:
                drone.noteOff(key);
                break;
            case STRING_PADS:
                // Pass the current chord mode directly
                stringPad.noteOff(key, StringPadSynthesizer::CHORD_MODE_OFF);
                break;
            case SOUNDFONT:
                soundfont.noteOff(defaultInstrument, key);  // Use configurable default instrument
                break;
            case SPLIT:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOff(key);
                }
                else
                {

                    stringPad.noteOff(key, StringPadSynthesizer::CHORD_MODE_OFF);
                }
                break;
            case IRAN:
                // Split mode: drone below split point, string pads above
                if (key < splitPoint)
                {
                    drone.noteOff(key);
                }
                else
                {
                    // Use chord mode for specific keys in split mode
                    StringPadSynthesizer::ChordMode chordMode = StringPadSynthesizer::CHORD_MODE_OFF;
                    if (key >= 53 && key <= 57)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_MAJOR;
                    }
                    else if (key == 60)
                    {
                        chordMode = StringPadSynthesizer::CHORD_MODE_OCTAVE;
                    }
                    key += 24;
                    stringPad.noteOff(key, chordMode);
                }
                break;
            case TUSK:
                // TUSK mode: instrument 0 above split point, instrument 1 below
                if (key < splitPoint)
                {
                    soundfont.noteOff(1, key + 12);  // Instrument 1 below split point
                }
                else
                {
                    soundfont.noteOff(0, key);  // Instrument 0 above split point
                }
                break;
            case TUSK_CHORD:
                if (key < splitPoint)
                {
                    // Below split point: turn off single note
                    soundfont.noteOff(1, key + 12);  // Instrument 1 below split point
                }
                else
                {
                    // Above split point: turn off all notes in the chord
                    if (activeChords[key].count > 0) {
                        ActiveChord& chord = activeChords[key];
                        for (int i = 0; i < chord.count; i++) {
                            int chordNote = chord.notes[i];
                            if (chordNote < splitPoint)
                            {
                                soundfont.noteOff(1, chordNote + 12);  // Instrument 1 below split point
                            }
                            else
                            {
                                soundfont.noteOff(0, chordNote);  // Instrument 0 above split point
                            }
                        }
                        activeChords[key].count = 0; // Clear the active chord
                    }
                }
                break;
            case WHITESNAKE:
                whitesnakePad.noteOff(key);
                break;
            }
    }

    void setPitchBend(float bendAmount) {
        // bendAmount is in semitones
        // Delegate to StringsSynthesizer
        strings.setPitchBend(bendAmount);
    }

    // String-specific controls (for backwards compatibility)
    void setAttenuation(uint16_t newAttenuation) {
        strings.setAttenuation(newAttenuation);
    }

    void setFilterStrength(uint16_t newStrength) {
        strings.setFilterStrength(newStrength);
    }
    
    // Direct access to components
    KarplusStrongStringSynth& getString(int index) { 
        return strings.getVoice(index);
    }
    
    // Peak monitoring methods
    int16_t getOutputMinL() const { return peakMonitorL.getMin(); }
    int16_t getOutputMaxL() const { return peakMonitorL.getMax(); }
    int16_t getOutputMinR() const { return peakMonitorR.getMin(); }
    int16_t getOutputMaxR() const { return peakMonitorR.getMax(); }
    
    void resetPeakMonitors() {
        peakMonitorL.reset();
        peakMonitorR.reset();
        whitesnakePadPeakMonitor.reset();
        soundfont.resetPeakMonitors();
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
    }

    void printPeakLevels() {
        Serial.printf("Peak Levels - L: min=%d, max=%d | R: min=%d, max=%d\n",
                      peakMonitorL.getMin(), peakMonitorL.getMax(),
                      peakMonitorR.getMin(), peakMonitorR.getMax());
        Serial.printf("Audio Memory (peak): %u | CPU (peak): %.1f%%\n",
                      (unsigned)AudioMemoryUsageMax(),
                      AudioProcessorUsageMax());
    }

private:
    void playStringNote(int key, float freq, float velocity) {
        // Delegate to StringsSynthesizer (it handles voice allocation internally)
        // The freq parameter is ignored as StringsSynthesizer calculates it from MIDI note
        strings.noteOn(key, velocity);
    }
    
    void updateMixerGains() {
        float stringGain = masterVolume * stringVolume;
        float droneGain = masterVolume * droneVolume;
        float stringPadGain = masterVolume * stringPadVolume;
        float soundfontGain = masterVolume * soundfontVolume;

        // Apply string gains via StringsSynthesizer
        strings.setVolume(stringGain);
        
        // Apply drone gains
        mixerL4.gain(0, droneGain);
        mixerR4.gain(0, droneGain);
        
        // Apply string pad gains
        mixerL5.gain(0, stringPadGain);
        mixerR5.gain(0, stringPadGain);
        
        // Apply soundfont gains (slot 1 carries whitesnakePad output; modes are mutually exclusive)
        mixerL6.gain(0, soundfontGain);
        mixerR6.gain(0, soundfontGain);
        mixerL6.gain(1, soundfontGain);
        mixerR6.gain(1, soundfontGain);
        
        // Sum mixer gains (4 inputs now: strings, drone, string pads, soundfont)
        sumL.gain(0, 1.0f); // strings
        sumL.gain(1, 1.0f); // drone
        sumL.gain(2, 1.0f); // string pads
        sumL.gain(3, 1.0f); // soundfont
        
        sumR.gain(0, 1.0f); // strings
        sumR.gain(1, 1.0f); // drone
        sumR.gain(2, 1.0f); // string pads
        sumR.gain(3, 1.0f); // soundfont
    }
};