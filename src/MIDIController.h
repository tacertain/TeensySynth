#pragma once

// Third-party libraries
#include <Arduino.h>
#include "USBHost_t36.h"

// Project headers
#include "HybridSynthesizer.h"
#include "ChordVelocityCapture.h"
#include "VelocitySmoother.h"
#include "LaunchkeyDisplay.h"

// Forward declaration
class MIDIController;

// Callback function type for control change handlers
typedef void (*MIDIControlChangeCallback)(MIDIController* controller, byte channel, byte control, byte value);

// Structure to map a control number to its callback
struct MIDIControllerChannelCallback {
    byte control;  // Control change number
    MIDIControlChangeCallback callback;  // Handler function
};

/**
 * MIDIController
 * 
 * Encapsulates MIDI input handling and USB device management.
 * Handles MIDI messages (Note On/Off, Control Change, Pitch Bend) and 
 * routes them to the appropriate synthesizer controls.
 */
class MIDIController {
public:
    MIDIController(HybridSynthesizer& synth, USBHost& usbHost, MIDIDevice& midiDevice);
    
    // Setup and initialization
    void begin();
    void update();
    
    // MIDI message handlers (called by USB MIDI library callbacks)
    void handleNoteOn(byte channel, byte note, byte velocity);
    void handleNoteOff(byte channel, byte note, byte velocity);
    void handleControlChange(byte channel, byte control, byte value);
    void handlePitchChange(byte channel, int bend);
    
    // Install a new set of control change callbacks for a specific bank
    // Bank numbers correspond to CC ranges: Bank 0 = CC 0-9, Bank 1 = CC 10-19, etc.
    void installCallbacks(byte bank, const MIDIControllerChannelCallback* callbacks, size_t count);
    
    // Accessor for synthesizer (needed by callback functions)
    HybridSynthesizer& getSynth() { return synth; }

    // Accessor for the WHITESNAKE velocity smoother (CC26 tunes its time constant).
    VelocitySmoother& getVelocitySmoother() { return velocitySmoother; }

    // Accessor for the Launchkey MK2 pad display (driven by the pad synth observer).
    LaunchkeyDisplay& getLaunchkeyDisplay() { return launchkeyDisplay; }
    
private:
    HybridSynthesizer& synth;
    USBHost& usbHost;
    MIDIDevice& midiDevice;
    
    // Control change callback tables organized by banks (CC ranges)
    // Bank 0 = CC 0-9, Bank 1 = CC 10-19, Bank 2 = CC 20-29, etc.
    static const size_t MAX_BANKS = 13;  // Covers CC 0-127
    const MIDIControllerChannelCallback* callbackTables[MAX_BANKS];
    size_t callbackCounts[MAX_BANKS];

    // Velocity-unification for chords. Active only in WHITESNAKE mode — the
    // toggle is a runtime check on synth.getCurrentMode() inside the three
    // call sites (handleNoteOn, handleNoteOff, update), not a separate enable
    // flag. Whenever any of those sees a non-WHITESNAKE mode it calls
    // chordCapture.reset() so the next entry into WHITESNAKE starts from a
    // clean IDLE state rather than picking up whatever HOLDING/CAPTURING
    // state was active when the mode last switched away.
    ChordVelocityCapture chordCapture;

    // Continuous-time EMA on chord-pinned velocities. Updates once per
    // chord-fire from chordCapture.tick(); its output re-levels all held
    // voices via pad.setHeldLevel(). CC26 sets the time constant (tau). Reset
    // alongside chordCapture when leaving WHITESNAKE so re-entering starts clean.
    VelocitySmoother velocitySmoother;

    // Drives the Launchkey MK2 pads as a velocity visualization for the
    // Whitesnake pad (Basic-mode pad lighting). Activated/deactivated on
    // WHITESNAKE entry/exit; fed from the chord-fire path in update().
    LaunchkeyDisplay launchkeyDisplay;
    HybridSynthesizer::SynthMode lastSynthMode;

    // Helper functions for MIDI processing
    float midiNoteToFrequency(byte note);
    float midiVelocityToFloat(byte velocity);
    void fireSynthNoteOn(byte channel, byte note, byte velocity);
};
