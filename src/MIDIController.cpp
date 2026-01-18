#include "MIDIController.h"

// Project headers
#include "MIDIControlCallbacks.h"

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
    
    // Initialize all callback tables to null
    for (size_t i = 0; i < MAX_BANKS; i++) {
        callbackTables[i] = nullptr;
        callbackCounts[i] = 0;
    }
    
    // Install default callbacks by bank
    // Bank 0: CC 0-9
    installCallbacks(0, channel1Bank_01_10, channel1Bank_01_10_count);
    // Bank 2: CC 20-29
    installCallbacks(2, channel1Bank_21_30, channel1Bank_21_30_count);
    // Bank 4: CC 40-49
    installCallbacks(4, channel1Bank_41_50, channel1Bank_41_50_count);
    // Bank 5: CC 50-59
    installCallbacks(5, channel1Bank_51_60, channel1Bank_51_60_count);
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

void MIDIController::installCallbacks(byte bank, const MIDIControllerChannelCallback* callbacks, size_t count) {
    // Validate bank number (0-12 covers CC 0-127)
    if (bank >= MAX_BANKS) {
        return;
    }
    
    // Install new callback table (or nullptr to clear)
    callbackTables[bank] = callbacks;
    callbackCounts[bank] = count;
}

void MIDIController::handleControlChange(byte channel, byte control, byte value) {
    Serial.print("Control Change, ch=");
    Serial.print(channel);
    Serial.print(", control=");
    Serial.print(control);
    Serial.print(", value=");
    Serial.print(value);
    Serial.println();

    // Determine which bank this control belongs to (CC 0-9 = bank 0, CC 10-19 = bank 1, etc.)
    byte bank = control / 10;
    
    // Validate bank
    if (bank >= MAX_BANKS) {
        return;
    }
    
    // Get callback table for this bank
    const MIDIControllerChannelCallback* callbacks = callbackTables[bank];
    size_t count = callbackCounts[bank];
    
    // If no callbacks installed for this bank, return
    if (callbacks == nullptr || count == 0) {
        return;
    }
    
    // Search for matching control number in the callback table
    for (size_t i = 0; i < count; i++) {
        if (callbacks[i].control == control) {
            // Found matching control, invoke callback
            if (callbacks[i].callback != nullptr) {
                callbacks[i].callback(this, channel, control, value);
            }
            return;
        }
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
