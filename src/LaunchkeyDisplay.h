#pragma once

#include <Arduino.h>
#include "USBHost_t36.h"

/**
 * LaunchkeyDisplay
 *
 * Drives the 16 RGB drum-pads on a Novation Launchkey MK2 as a two-row
 * velocity visualization for the Whitesnake pad:
 *
 *   - Top row    = the raw chord velocity (the pinned median, before
 *                  smoothing), as a 0..8 bar at full brightness. Holds until
 *                  the next chord.
 *   - Bottom row = the blended velocity (the VelocitySmoother EMA output),
 *                  as a 0..8 bar. Its brightness resets to full on each chord
 *                  and decays toward dark following exp(-dt/tau), matching the
 *                  smoother's time constant (CC26). The bar *length* holds
 *                  between chords; only brightness fades.
 *
 * Both bars use a green->yellow->red column gradient. Brightness is quantized
 * to a few discrete palette levels (the pads can't do smooth PWM dimming).
 *
 * Protocol per Novation's Launchkey MK2 Programmer's Reference Guide. We use
 * BASIC-mode pad lighting (NOT Extended/InControl mode) deliberately:
 *   - Entering Extended mode reroutes the pots/sliders to the InControl port,
 *     which would break the keyboard's CC controls (CC 21-28, mode switches).
 *   - The Extended-mode online message also gets echoed back by the keyboard
 *     as a NoteOn ch16 note 0x0C, which would be misread as a played note.
 *   Lighting pads in Basic mode avoids both problems: keys/pots/sliders stay
 *   on the normal MIDI port, and no mode message (hence no echo) is sent.
 *
 *   - Pad-lighting messages go on USB-MIDI cable 1 ("InControl"), channel 16.
 *   - Basic-mode pad addresses (MIDI notes), per "Lighting Pads in Basic Mode":
 *       Top row    (left->right): 0x28 0x29 0x2A 0x2B 0x30 0x31 0x32 0x33
 *       Bottom row (left->right): 0x24 0x25 0x26 0x27 0x2C 0x2D 0x2E 0x2F
 *     (Note the gap: pads are two 2x4 groups, so columns 0-3 and 4-7 are
 *     non-contiguous.)
 *   - LED set: NoteOn ch16 <pad> <color>  (color = palette index, 0 = off).
 */
class LaunchkeyDisplay {
public:
    static constexpr int NUM_COLUMNS = 8;

    LaunchkeyDisplay();

    void begin(MIDIDevice* device);

    // Begin/stop driving the pad display (called on WHITESNAKE entry/exit).
    // Both just clear the pads; no keyboard mode change is involved.
    void activate();
    void deactivate();

    // Called once per new chord. Sets both bars and resets the bottom-row
    // brightness to full. rawVel/blendedVel are MIDI velocities (0..127).
    void onChord(byte rawVel, byte blendedVel, uint32_t nowMs);

    // Called every loop. Decays the bottom-row brightness based on time since
    // the last chord and the current smoother time constant. Re-sends the
    // bottom row only when the quantized brightness level changes.
    void tick(uint32_t nowMs, float tauMs);

    // Turn the top row off (called when all keys are released). The bottom
    // row keeps decaying independently.
    void clearTopRow();

private:
    MIDIDevice* device;
    bool active;
    bool haveChord;
    int topSteps;
    int bottomSteps;
    uint32_t lastChordMs;
    int lastBottomLevel;  // -1 = off / not yet drawn

    static int stepsForVelocity(byte velocity);
    static int brightnessLevel(float decay);

    void renderTopRow();
    void renderBottomRow(int level);
    void setTopPad(int column, byte color);
    void setBottomPad(int column, byte color);
    void clearAllPads();
};
