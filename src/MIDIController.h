#pragma once

// Third-party libraries
#include <Arduino.h>
#include "USBHost_t36.h"

// Project headers
#include "HybridSynthesizer.h"

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
    
private:
    HybridSynthesizer& synth;
    USBHost& usbHost;
    MIDIDevice& midiDevice;
    
    // Helper functions for MIDI processing
    float midiNoteToFrequency(byte note);
    float midiVelocityToFloat(byte velocity);
};
