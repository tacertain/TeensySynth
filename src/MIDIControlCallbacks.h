#pragma once

#include "MIDIController.h"

// ============================================================================
// Control Change Callback Function Declarations
// ============================================================================

// General controls
void CC_MasterVolume(MIDIController* controller, byte channel, byte control, byte value);

// Attenuation and filter controls
void CC_Attenuation(MIDIController* controller, byte channel, byte control, byte value);
void CC_FilterStrength(MIDIController* controller, byte channel, byte control, byte value);

// Drone controls
void CC_DroneVolume(MIDIController* controller, byte channel, byte control, byte value);
void CC_FilterCutoff(MIDIController* controller, byte channel, byte control, byte value);
void CC_LFORate(MIDIController* controller, byte channel, byte control, byte value);
void CC_OscillatorDetune(MIDIController* controller, byte channel, byte control, byte value);
void CC_LFODepth(MIDIController* controller, byte channel, byte control, byte value);

// Soundfont ADSR controls
void CC_SoundfontAttack(MIDIController* controller, byte channel, byte control, byte value);
void CC_SoundfontDecay(MIDIController* controller, byte channel, byte control, byte value);
void CC_SoundfontSustain(MIDIController* controller, byte channel, byte control, byte value);
void CC_SoundfontRelease(MIDIController* controller, byte channel, byte control, byte value);

// Soundfont filter controls
void CC_SoundfontFilterFrequency(MIDIController* controller, byte channel, byte control, byte value);

// String pad controls
void CC_StringPadVolume(MIDIController* controller, byte channel, byte control, byte value);
void CC_StringPadFilterCutoff(MIDIController* controller, byte channel, byte control, byte value);
void CC_StringPadFilterResonance(MIDIController* controller, byte channel, byte control, byte value);
void CC_StringPadDetuneAmount(MIDIController* controller, byte channel, byte control, byte value);
void CC_HighpassMultiplier(MIDIController* controller, byte channel, byte control, byte value);

// Mode switching controls
void CC_ModePluckedStrings(MIDIController* controller, byte channel, byte control, byte value);
void CC_ModeDrone(MIDIController* controller, byte channel, byte control, byte value);
void CC_ModeStringPadsBright(MIDIController* controller, byte channel, byte control, byte value);
void CC_ModeSoundfontTrombone(MIDIController* controller, byte channel, byte control, byte value);
void CC_ModeTusk(MIDIController* controller, byte channel, byte control, byte value);
void CC_ModeSoundfontTromboneTusk(MIDIController* controller, byte channel, byte control, byte value);
void CC_ModeSoundfontTrumpetTusk(MIDIController* controller, byte channel, byte control, byte value);
void CC_ModeTuskChord(MIDIController* controller, byte channel, byte control, byte value);
void CC_CycleStringPadChordMode(MIDIController* controller, byte channel, byte control, byte value);

// ============================================================================
// Control Change Callback Bank Arrays
// Bank N contains callbacks for CC numbers N*10 to N*10+9
// ============================================================================

// Bank 0: CC 0-9 - General controls
extern const MIDIControllerChannelCallback channel1Bank_01_10[];
extern const size_t channel1Bank_01_10_count;

// Bank 1: CC 10-19 - (currently unused)
extern const MIDIControllerChannelCallback channel1Bank_11_20[];
extern const size_t channel1Bank_11_20_count;

// Bank 2: CC 20-29 - Attenuation, filter, and drone controls
extern const MIDIControllerChannelCallback channel1Bank_21_30[];
extern const size_t channel1Bank_21_30_count;

// Bank 2 (alternate): CC 20-29 - Soundfont ADSR controls
extern const MIDIControllerChannelCallback channel1Bank_21_30_SF[];
extern const size_t channel1Bank_21_30_SF_count;

// Bank 3: CC 30-39 - (currently unused)
extern const MIDIControllerChannelCallback channel1Bank_31_40[];
extern const size_t channel1Bank_31_40_count;

// Bank 4: CC 40-49 - String pad controls
extern const MIDIControllerChannelCallback channel1Bank_41_50[];
extern const size_t channel1Bank_41_50_count;

// Bank 5: CC 50-59 - Mode switching controls
extern const MIDIControllerChannelCallback channel1Bank_51_60[];
extern const size_t channel1Bank_51_60_count;

// ============================================================================
// Helper Functions
// ============================================================================

// Install bank 2 callbacks appropriate for soundfont or non-soundfont modes
void installBank2ForMode(MIDIController* controller, bool isSoundfontMode);
