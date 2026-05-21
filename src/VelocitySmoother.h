#pragma once

#include <Arduino.h>

/**
 * VelocitySmoother
 *
 * Continuous-time first-order low-pass on chord-pinned velocities.
 * Sits downstream of ChordVelocityCapture in WHITESNAKE mode so that the
 * macro arc of a song (intro -> verse -> chorus build) is decoupled from
 * per-chord velocity variation.
 *
 * On each new chord, the smoother updates:
 *     dt    = nowMs - lastUpdateMs
 *     alpha = 1 - exp(-dt / tauMs)
 *     smoothed += alpha * (pinnedVelocity - smoothed)
 *
 * The first chord initializes smoothed to its own velocity (no lag on start).
 * Held chords with no new note-ons do not update -- smoothed simply holds.
 * Long gaps cause alpha -> 1, so the next chord effectively resets state
 * without any explicit reset logic.
 *
 * Tau is driven by CC26 in WHITESNAKE mode (0..127 -> 0.5..15 s exponential).
 */
class VelocitySmoother {
public:
    VelocitySmoother();

    // Update with a new chord's pinned velocity and return the smoothed
    // value to play. Call once per chord-fire event.
    byte onChordFire(byte pinnedVelocity, uint32_t nowMs);

    // tauMs is the time constant in milliseconds. Larger = slower tracking.
    void setTauMs(float tauMs);
    float getTauMs() const { return tauMs; }

    void reset();

private:
    float smoothed;       // current smoothed velocity (float, 0..127)
    uint32_t lastUpdateMs;
    float tauMs;
    bool initialized;
};
