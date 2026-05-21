#pragma once

#include <Arduino.h>

/**
 * ChordVelocityCapture
 *
 * State machine that unifies velocity across the notes of a chord, to
 * compensate for inconsistent key-strike velocities on a poorly-curved
 * keyboard.
 *
 * Flow:
 *  - Any noteOn (from IDLE) opens a WINDOW_MS capture window; that note plus
 *    any others arriving inside the window are buffered (not fired).
 *  - When the window closes, the buffered notes all fire at the median of
 *    their velocities (the "pinned" velocity), and capture resets to IDLE.
 *  - So a note struck while an earlier chord is still held opens a *new*
 *    window and produces a *new* pinned velocity. The caller feeds that into
 *    the smoother and applies the result to all currently-held voices; this
 *    class does not pin subsequent notes to a prior chord's velocity.
 *  - Held-note tracking is kept only so the caller can ask hasHeldNotes()
 *    (e.g. to know when all keys are released). It does not gate firing.
 *
 * Driver responsibilities:
 *  - Call onNoteOn / onNoteOff for every MIDI event when capture should
 *    be active.
 *  - Call tick() regularly (every loop iteration). When it returns >0
 *    pending notes, fire them via the synth.
 *  - When capture should NOT be active (e.g. non-WHITESNAKE modes), call
 *    reset() and route notes to the synth directly, bypassing this class.
 */
class ChordVelocityCapture {
public:
    static constexpr uint32_t WINDOW_MS = 30;
    static constexpr int MAX_BUFFER = 16;   // chord size ceiling during window
    static constexpr int MAX_HELD = 32;     // held-note ceiling
    static constexpr int MAX_PENDING_OUT = MAX_BUFFER;

    struct PendingNote {
        byte channel;
        byte note;
        byte velocity;
    };

    ChordVelocityCapture() = default;

    // Buffers the note into the current (or a new) capture window. Notes never
    // fire from here; they fire from tick() when the window closes.
    void onNoteOn(byte channel, byte note, byte velocity, uint32_t nowMs);

    // The noteOff itself should always be forwarded to the synth by the caller;
    // this just updates internal held-note tracking and buffer cleanup.
    void onNoteOff(byte note);

    // Drains buffered notes if the capture window has expired.
    // Returns the number of PendingNote entries written into `out`.
    int tick(uint32_t nowMs, PendingNote* out, int maxOut);

    void reset();

    // True while at least one note from the current chord is still held.
    bool hasHeldNotes() const { return heldCount > 0; }

private:
    enum State { IDLE, CAPTURING };

    struct BufferedNote {
        byte channel;
        byte note;
        byte velocity;
    };

    State state = IDLE;

    BufferedNote buffer[MAX_BUFFER];
    int bufferCount = 0;
    uint32_t windowDeadline = 0;

    byte pinnedVelocity = 0;

    byte heldNotes[MAX_HELD];
    int heldCount = 0;

    void addHeld(byte note);
    bool removeHeld(byte note);
    void removeFromBuffer(byte note);
};
