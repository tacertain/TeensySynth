#include "ChordVelocityCapture.h"

ChordVelocityCapture::NoteOnResult ChordVelocityCapture::onNoteOn(
    byte channel, byte note, byte velocity, uint32_t nowMs) {

    switch (state) {
        case IDLE:
            state = CAPTURING;
            bufferCount = 0;
            heldCount = 0;
            buffer[bufferCount++] = {channel, note, velocity};
            addHeld(note);
            windowDeadline = nowMs + WINDOW_MS;
            return {false, 0};

        case CAPTURING:
            if (bufferCount < MAX_BUFFER) {
                buffer[bufferCount++] = {channel, note, velocity};
            }
            addHeld(note);
            return {false, 0};

        case HOLDING:
            addHeld(note);
            return {true, pinnedVelocity};
    }
    return {true, velocity};  // unreachable; defensive
}

void ChordVelocityCapture::onNoteOff(byte note) {
    bool wasHeld = removeHeld(note);

    if (state == CAPTURING) {
        // Sub-window tap: drop the note from the buffer so it never sounds.
        removeFromBuffer(note);
    }

    if (wasHeld && heldCount == 0) {
        reset();
    }
}

int ChordVelocityCapture::tick(uint32_t nowMs, PendingNote* out, int maxOut) {
    if (state != CAPTURING) return 0;
    // Signed compare so wraparound (every ~49 days) doesn't fire spuriously.
    if ((int32_t)(nowMs - windowDeadline) < 0) return 0;

    if (bufferCount == 0) {
        // Whole chord was tapped off within the window. Nothing to fire.
        reset();
        return 0;
    }

    uint32_t sum = 0;
    for (int i = 0; i < bufferCount; ++i) sum += buffer[i].velocity;
    pinnedVelocity = (byte)(sum / bufferCount);

    int n = bufferCount < maxOut ? bufferCount : maxOut;
    for (int i = 0; i < n; ++i) {
        out[i] = {buffer[i].channel, buffer[i].note, pinnedVelocity};
    }
    bufferCount = 0;
    state = HOLDING;
    return n;
}

void ChordVelocityCapture::reset() {
    state = IDLE;
    bufferCount = 0;
    heldCount = 0;
    pinnedVelocity = 0;
    windowDeadline = 0;
}

void ChordVelocityCapture::addHeld(byte note) {
    for (int i = 0; i < heldCount; ++i) {
        if (heldNotes[i] == note) return;  // already tracked
    }
    if (heldCount >= MAX_HELD) return;
    heldNotes[heldCount++] = note;
}

bool ChordVelocityCapture::removeHeld(byte note) {
    for (int i = 0; i < heldCount; ++i) {
        if (heldNotes[i] == note) {
            for (int j = i; j < heldCount - 1; ++j) {
                heldNotes[j] = heldNotes[j + 1];
            }
            heldCount--;
            return true;
        }
    }
    return false;
}

void ChordVelocityCapture::removeFromBuffer(byte note) {
    for (int i = 0; i < bufferCount; ++i) {
        if (buffer[i].note == note) {
            for (int j = i; j < bufferCount - 1; ++j) {
                buffer[j] = buffer[j + 1];
            }
            bufferCount--;
            return;
        }
    }
}
