#include "ChordVelocityCapture.h"

void ChordVelocityCapture::onNoteOn(
    byte channel, byte note, byte velocity, uint32_t nowMs) {

    addHeld(note);  // track held keys independent of the capture window

    switch (state) {
        case IDLE:
            // Open a fresh window. Note: do NOT clear heldCount here -- earlier
            // chords may still be held while this new window opens.
            state = CAPTURING;
            bufferCount = 0;
            buffer[bufferCount++] = {channel, note, velocity};
            windowDeadline = nowMs + WINDOW_MS;
            return;

        case CAPTURING:
            if (bufferCount < MAX_BUFFER) {
                buffer[bufferCount++] = {channel, note, velocity};
            }
            return;
    }
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

    // Sort buffered velocities so we can read median and extremes.
    byte sorted[MAX_BUFFER];
    for (int i = 0; i < bufferCount; ++i) sorted[i] = buffer[i].velocity;
    for (int i = 1; i < bufferCount; ++i) {
        byte key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    // Clamp bounds track SoundfontPadSynthesizer's velocity window (20..100):
    // a chord that brackets the window pins to the median, but a one-sided
    // chord pins to the relevant boundary so the smoother sees usable values.
    byte minVel = sorted[0];
    byte maxVel = sorted[bufferCount - 1];
    bool hasLow = minVel < 20;
    bool hasHigh = maxVel > 100;

    if (hasLow && !hasHigh) {
        pinnedVelocity = 20;
    } else if (hasHigh && !hasLow) {
        pinnedVelocity = 100;
    } else if (bufferCount % 2 == 1) {
        pinnedVelocity = sorted[bufferCount / 2];
    } else {
        pinnedVelocity = (byte)(((uint16_t)sorted[bufferCount / 2 - 1]
                                + sorted[bufferCount / 2]) / 2);
    }

    int n = bufferCount < maxOut ? bufferCount : maxOut;
    for (int i = 0; i < n; ++i) {
        out[i] = {buffer[i].channel, buffer[i].note, pinnedVelocity};
    }
    bufferCount = 0;
    state = IDLE;  // window closed; next struck note opens a new window
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
