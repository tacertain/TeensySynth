#include "LaunchkeyDisplay.h"
#include "hardware_config.h"
#include <math.h>

// Pad note numbers in Basic mode (Programmer's Reference, "Lighting Pads in
// Basic Mode"). Columns 0-3 and 4-7 are non-contiguous (two 2x4 pad groups).
static constexpr byte TOP_ROW_NOTES[8]    = { 0x28, 0x29, 0x2A, 0x2B, 0x30, 0x31, 0x32, 0x33 };
static constexpr byte BOTTOM_ROW_NOTES[8] = { 0x24, 0x25, 0x26, 0x27, 0x2C, 0x2D, 0x2E, 0x2F };

// Column hue gradient (left -> right): green, green, green, yellow, yellow,
// orange, red, red.
enum Hue { HUE_GREEN = 0, HUE_YELLOW, HUE_ORANGE, HUE_RED, NUM_HUES };
static constexpr byte COLUMN_HUE[8] = {
    HUE_GREEN, HUE_GREEN, HUE_GREEN,
    HUE_YELLOW, HUE_YELLOW,
    HUE_ORANGE,
    HUE_RED, HUE_RED,
};

// Palette indices, per hue, from brightest (level 0) to dimmest (level 2).
// From the MK2 color palette, each hue is a group of 4 consecutive indices in
// the order [pale, bright, mid, dim]. So for each hue the full-saturated index
// N gives the bright/mid/dim run N, N+1, N+2.
static constexpr int NUM_BRIGHTNESS = 3;  // FULL, MID, DIM (off is handled separately)
static constexpr byte PALETTE[NUM_HUES][NUM_BRIGHTNESS] = {
    //  FULL  MID  DIM
    { 0x15, 0x16, 0x17 },  // green
    { 0x0D, 0x0E, 0x0F },  // yellow
    { 0x09, 0x0A, 0x0B },  // orange
    { 0x05, 0x06, 0x07 },  // red
};
static constexpr byte COLOR_OFF = 0;

// Pad-lighting messages go on the InControl port (cable 1), channel 16, even
// in Basic mode (per the Programmer's Reference).
static constexpr uint8_t INCONTROL_CABLE = 1;
static constexpr uint8_t LK_CHANNEL = 16;       // 1-indexed in USBHost_t36

// Effective velocity window for the bar meter. Tracks SoundfontPadSynthesizer's
// velocity curve window (20..100) and the chord-capture clamp so the 8 columns
// span the actually-usable range instead of the full 0..127.
static constexpr int VEL_MIN = 20;
static constexpr int VEL_MAX = 100;

// Above this, sendNoteOn almost certainly spun inside write_packed waiting for
// a USB tx buffer to drain. Normal enqueue is sub-microsecond, so 100 is well
// clear of noise while still catching partial-spin sends that didn't drag long
// enough to trip a higher bar.
static constexpr uint32_t SEND_SLOW_THRESHOLD_US = 100;

LaunchkeyDisplay::LaunchkeyDisplay()
    : device(nullptr), active(false), haveChord(false),
      topSteps(0), bottomSteps(0), lastChordMs(0), lastBottomLevel(-1),
      sendCount(0), sendCountStartMs(0), lastReportedRateZero(false) {}

void LaunchkeyDisplay::begin(MIDIDevice* dev) {
    device = dev;
    pinMode(SCOPE_TRIG_PIN, OUTPUT);
    digitalWriteFast(SCOPE_TRIG_PIN, LOW);
}

void LaunchkeyDisplay::activate() {
    if (device == nullptr || active) return;
    active = true;
    haveChord = false;
    lastBottomLevel = -1;
    clearAllPads();
}

void LaunchkeyDisplay::deactivate() {
    if (device == nullptr || !active) return;
    clearAllPads();
    active = false;
    haveChord = false;
    lastBottomLevel = -1;
}

void LaunchkeyDisplay::onChord(byte rawVel, byte blendedVel, uint32_t nowMs) {
    if (device == nullptr || !active) return;
    topSteps = stepsForVelocity(rawVel);
    bottomSteps = stepsForVelocity(blendedVel);
    lastChordMs = nowMs;
    haveChord = true;
    renderTopRow();
    lastBottomLevel = 0;  // full brightness
    renderBottomRow(0);
}

void LaunchkeyDisplay::tick(uint32_t nowMs, float tauMs) {
    // Per-second LK MIDI send-rate report. Runs whenever the display is
    // active (independent of haveChord) so we catch send bursts from activate/
    // deactivate too. Spikes near USB bandwidth are an early warning for the
    // write_packed-spin hang.
    if (active) {
        if (sendCountStartMs == 0) sendCountStartMs = nowMs;
        if ((nowMs - sendCountStartMs) >= 1000) {
            // Suppress consecutive zero-rate lines so the log stays clean
            // during idle periods. The first zero after activity still prints
            // as a "things quieted down" marker; further zeros are dropped
            // until activity resumes.
            bool isZero = (sendCount == 0);
            if (!isZero || !lastReportedRateZero) {
                Serial.printf("LK send rate: %lu/s\n", (unsigned long)sendCount);
            }
            lastReportedRateZero = isZero;
            sendCount = 0;
            sendCountStartMs = nowMs;
        }
    }

    if (device == nullptr || !active || !haveChord) return;
    if (tauMs < 1.0f) tauMs = 1.0f;
    float dt = (float)(nowMs - lastChordMs);  // uint32 subtraction is wrap-safe
    float decay = expf(-dt / tauMs);
    int level = brightnessLevel(decay);
    if (level == lastBottomLevel) return;
    renderBottomRow(level);
    lastBottomLevel = level;
}

void LaunchkeyDisplay::clearTopRow() {
    if (device == nullptr || !active) return;
    topSteps = 0;
    renderTopRow();
}

// Map the effective velocity window across all 8 columns: vel <= VEL_MIN -> 1
// column, vel >= VEL_MAX -> 8 columns, linear in between. vel 0 (no note) -> 0.
int LaunchkeyDisplay::stepsForVelocity(byte velocity) {
    if (velocity == 0) return 0;
    int v = constrain((int)velocity, VEL_MIN, VEL_MAX);
    float frac = (float)(v - VEL_MIN) / (float)(VEL_MAX - VEL_MIN);  // 0..1
    int steps = 1 + (int)(frac * (NUM_COLUMNS - 1) + 0.5f);          // 1..8
    return steps;
}

// Map decay (1.0 just after a chord -> 0 as it goes stale) to a brightness
// level index, or -1 for fully off.
int LaunchkeyDisplay::brightnessLevel(float decay) {
    if (decay >= 0.66f) return 0;  // FULL
    if (decay >= 0.33f) return 1;  // MID
    if (decay >= 0.10f) return 2;  // DIM
    return -1;                     // OFF
}

void LaunchkeyDisplay::renderTopRow() {
    for (int c = 0; c < NUM_COLUMNS; ++c) {
        byte color = (c < topSteps) ? PALETTE[COLUMN_HUE[c]][0] : COLOR_OFF;
        setTopPad(c, color);
    }
}

void LaunchkeyDisplay::renderBottomRow(int level) {
    for (int c = 0; c < NUM_COLUMNS; ++c) {
        byte color;
        if (level < 0 || c >= bottomSteps) {
            color = COLOR_OFF;
        } else {
            color = PALETTE[COLUMN_HUE[c]][level];
        }
        setBottomPad(c, color);
    }
}

void LaunchkeyDisplay::setTopPad(int column, byte color) {
    sendPadNoteOn(TOP_ROW_NOTES[column], color, 't', column);
}

void LaunchkeyDisplay::setBottomPad(int column, byte color) {
    sendPadNoteOn(BOTTOM_ROW_NOTES[column], color, 'b', column);
}

void LaunchkeyDisplay::sendPadNoteOn(byte padNote, byte color, char row, int column) {
    // Trigger pin HIGH spans the exact duration of the underlying USB call:
    // on the scope this gives one pulse per outbound packet, and a permanently
    // HIGH line is a positive marker that we're stuck inside write_packed.
    digitalWriteFast(SCOPE_TRIG_PIN, HIGH);
    uint32_t t0 = micros();
    device->sendNoteOn(padNote, color, LK_CHANNEL, INCONTROL_CABLE);
    uint32_t dt = micros() - t0;
    digitalWriteFast(SCOPE_TRIG_PIN, LOW);
    sendCount++;
    if (dt > SEND_SLOW_THRESHOLD_US) {
        Serial.printf("LK send slow: %luus %c%d color=%02X\n",
                      (unsigned long)dt, row, column, color);
    }
}

void LaunchkeyDisplay::clearAllPads() {
    // The CC0 reset is an Extended-mode feature; in Basic mode turn each pad
    // off explicitly (velocity 0).
    for (int c = 0; c < NUM_COLUMNS; ++c) {
        setTopPad(c, COLOR_OFF);
        setBottomPad(c, COLOR_OFF);
    }
}
