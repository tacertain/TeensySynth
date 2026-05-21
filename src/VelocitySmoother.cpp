#include "VelocitySmoother.h"
#include <math.h>

// CC26=64 default: tauMs = 500 * 30^(64/127) ~= 2715 ms.
static constexpr float DEFAULT_TAU_MS = 2715.0f;

VelocitySmoother::VelocitySmoother()
    : smoothed(0.0f), lastUpdateMs(0), tauMs(DEFAULT_TAU_MS), initialized(false) {}

byte VelocitySmoother::onChordFire(byte pinnedVelocity, uint32_t nowMs) {
    if (!initialized) {
        smoothed = (float)pinnedVelocity;
        initialized = true;
    } else {
        // uint32 subtraction is wraparound-safe.
        uint32_t dt = nowMs - lastUpdateMs;
        float alpha = 1.0f - expf(-(float)dt / tauMs);
        smoothed += alpha * ((float)pinnedVelocity - smoothed);
    }
    lastUpdateMs = nowMs;
    int v = (int)(smoothed + 0.5f);
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    return (byte)v;
}

void VelocitySmoother::setTauMs(float newTauMs) {
    // Guard against zero/negative which would blow up expf.
    if (newTauMs < 1.0f) newTauMs = 1.0f;
    tauMs = newTauMs;
}

void VelocitySmoother::reset() {
    smoothed = 0.0f;
    lastUpdateMs = 0;
    initialized = false;
}
