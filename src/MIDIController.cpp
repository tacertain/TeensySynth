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
    : synth(synth), usbHost(usbHost), midiDevice(midiDevice),
      lastSynthMode(synth.getCurrentMode()) {
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

    launchkeyDisplay.begin(&midiDevice);
}

void MIDIController::update() {
    // Per-phase timing. The freeze happens somewhere inside this function (it's
    // the only thing in loop() that touches USB), so log any call that drags
    // past 1 ms — the last "slow" line before the LED freezes will identify
    // exactly which call wedged.
    constexpr uint32_t PHASE_SLOW_THRESHOLD_US = 1000;
    uint32_t t = micros();
    usbHost.Task();
    uint32_t dt = micros() - t;
    if (dt > PHASE_SLOW_THRESHOLD_US) {
        Serial.printf("phase slow: usbHost.Task %luus\n", (unsigned long)dt);
    }

    t = micros();
    midiDevice.read();
    dt = micros() - t;
    if (dt > PHASE_SLOW_THRESHOLD_US) {
        Serial.printf("phase slow: midiDevice.read %luus\n", (unsigned long)dt);
    }

    // Detect USB-level Launchkey disconnect/reconnect. Stage-1 of the hang
    // (input dies, output still works) could be either a device disconnect or
    // a device that's still claimed but stopped sending — this disambiguates.
    static bool wasConnected = false;
    bool nowConnected = (bool)midiDevice;
    if (nowConnected != wasConnected) {
        Serial.printf("LK device state -> %s\n",
                      nowConnected ? "connected" : "disconnected");
        wasConnected = nowConnected;
    }

    uint32_t now = millis();

    // Detect mode transitions to drive Extended-mode entry/exit on the Launchkey.
    HybridSynthesizer::SynthMode currentMode = synth.getCurrentMode();
    if (currentMode != lastSynthMode) {
        if (currentMode == HybridSynthesizer::WHITESNAKE) {
            launchkeyDisplay.activate();
        } else if (lastSynthMode == HybridSynthesizer::WHITESNAKE) {
            launchkeyDisplay.deactivate();
        }
        lastSynthMode = currentMode;
    }

    // Drive the chord-velocity capture window. Only meaningful in WHITESNAKE;
    // elsewhere we keep capture state reset so re-entering the mode starts clean.
    t = micros();
    if (currentMode == HybridSynthesizer::WHITESNAKE) {
        ChordVelocityCapture::PendingNote pending[ChordVelocityCapture::MAX_PENDING_OUT];
        int n = chordCapture.tick(now, pending, ChordVelocityCapture::MAX_PENDING_OUT);
        if (n > 0) {
            // All pending notes share the same chord-pinned velocity; feed
            // it into the smoother once and use the smoothed value for the
            // whole chord.
            byte rawVel = pending[0].velocity;
            byte smoothedVel = velocitySmoother.onChordFire(rawVel, now);
            launchkeyDisplay.onChord(rawVel, smoothedVel, now);
            for (int i = 0; i < n; ++i) {
                fireSynthNoteOn(pending[i].channel, pending[i].note, smoothedVel);
            }
            // Re-level every currently-held voice (new + previously held) to
            // the smoothed value, so held chords track the evolving blend.
            synth.getWhitesnakePad().setHeldLevel(smoothedVel / 128.0f);
        }
    } else {
        chordCapture.reset();
        velocitySmoother.reset();
    }
    dt = micros() - t;
    if (dt > PHASE_SLOW_THRESHOLD_US) {
        Serial.printf("phase slow: chord/onChord %luus\n", (unsigned long)dt);
    }

    // Animate the bottom-row brightness decay (no-op outside Extended mode).
    t = micros();
    launchkeyDisplay.tick(now, velocitySmoother.getTauMs());
    dt = micros() - t;
    if (dt > PHASE_SLOW_THRESHOLD_US) {
        Serial.printf("phase slow: launchkey.tick %luus\n", (unsigned long)dt);
    }
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

    if (synth.getCurrentMode() == HybridSynthesizer::WHITESNAKE) {
        // Always buffered into a capture window; the chord fires from tick().
        chordCapture.onNoteOn(channel, note, velocity, millis());
        Serial.println("  (buffered for chord-velocity capture)");
    } else {
        chordCapture.reset();
        velocitySmoother.reset();
        fireSynthNoteOn(channel, note, velocity);
    }
}

void MIDIController::fireSynthNoteOn(byte channel, byte note, byte velocity) {
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

    if (synth.getCurrentMode() == HybridSynthesizer::WHITESNAKE) {
        chordCapture.onNoteOff(note);
        if (!chordCapture.hasHeldNotes()) {
            launchkeyDisplay.clearTopRow();
        }
    }
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
