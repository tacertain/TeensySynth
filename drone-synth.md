# Drone Synthesizer Mode - Design Document

## Overview
This document outlines the design for a new synthesizer mode that recreates the classic 1980s analog drone sound as heard in "I Ran" by Flock of Seagulls, originally produced by a Korg MS-10 synthesizer. The drone mode will be integrated into the existing `HybridSynthesizer` architecture alongside the current STRINGS_ONLY, SOUNDFONT_ONLY, LAYERED, and SPLIT modes.

## Sound Characteristics
The target sound has these key characteristics from the Korg MS-10:
- **Sustained drone/pad sound** with slow attack and long sustain
- **Rich harmonic content** from analog-style oscillators
- **Sweeping filter cutoff** creating the characteristic "swoosh" effect
- **Chorus/ensemble effect** for width and movement
- **Analog warmth** through slight detuning and filtering
- **Low-frequency modulation** for subtle movement and life

## Technical Architecture

### 1. New Synthesizer Mode
Add `DRONE` to the existing `SynthMode` enum:
```cpp
enum SynthMode {
    STRINGS_ONLY,
    SOUNDFONT_ONLY,
    LAYERED,
    SPLIT,
    DRONE          // New analog-style drone synthesizer
};
```

### 2. Drone Synthesizer Components
Create a new `DroneSynthesizer` class that utilizes Teensy Audio Library components:

#### Core Audio Chain:
1. **Dual Oscillators**: 
   - `AudioSynthWaveform osc1` (sawtooth wave)
   - `AudioSynthWaveform osc2` (pulse wave with adjustable width)
   - Slight detuning between oscillators for analog warmth

2. **Sub-Oscillator**: 
   - `AudioSynthWaveform subOsc` (square wave, 1 octave down)
   - Mixed at lower level for bass foundation

3. **Filter Section**:
   - `AudioFilterStateVariable filter` (lowpass mode)
   - Envelope and LFO modulation for sweeping effects

4. **Envelope Generator**:
   - `AudioEffectEnvelope env` for amplitude shaping
   - Slow attack, high sustain, medium release

5. **Low Frequency Oscillator (LFO)**:
   - `AudioSynthWaveform lfo` (triangle or sine wave)
   - Modulates filter cutoff for movement

6. **Effects Chain**:
   - `AudioEffectChorus chorus` for width and ensemble effect
   - `AudioEffectReverb reverb` for ambience
   - `AudioEffectDelay delay` for subtle echo

#### Polyphony Management:
- 4-6 voice polyphony using voice allocation
- Each voice has complete oscillator + filter + envelope chain
- Voices mixed through `AudioMixer4` objects

### 3. Integration with HybridSynthesizer

#### Audio Routing:
- Add new mixer channels for drone synthesis
- Route drone output through existing `sumL` and `sumR` mixers
- Maintain compatibility with existing volume and mode switching

#### Control Interface:
- Extend existing control methods for drone-specific parameters:
  - Filter cutoff and resonance
  - LFO rate and depth
  - Oscillator detuning amount
  - Chorus depth and rate
  - Pulse width modulation

### 4. Parameter Control

#### Real-time Controls:
- **Filter Cutoff**: Primary expressive control (MIDI CC1 or similar)
- **Resonance**: Filter feedback amount
- **LFO Rate**: Speed of filter sweep
- **LFO Depth**: Amount of filter modulation
- **Detune**: Oscillator detuning for warmth
- **Pulse Width**: PWM for osc2
- **Chorus Mix**: Wet/dry balance for ensemble effect

#### Voice Parameters:
- **Attack Time**: Envelope attack (typically 100-500ms)
- **Sustain Level**: High sustain for pad sounds
- **Release Time**: Medium release (500-1000ms)
- **Sub Oscillator Level**: Amount of sub-bass

### 5. Implementation Plan

#### Phase 1: Core Drone Engine ✅ COMPLETED
1. ✅ Create `DroneSynthesizer.h` and `DroneSynthesizer.cpp`
2. ✅ Implement single-voice prototype with basic oscillator + filter
3. ✅ Test audio output and basic functionality

#### Phase 2: Polyphony and Voices ✅ COMPLETED
1. ✅ Implement voice allocation system with 6-voice polyphony
2. ✅ Add polyphonic capability using DroneVoice structure
3. ✅ Integrate with existing MIDI note on/off system
4. ✅ Update HybridSynthesizer to use MIDI note numbers instead of frequency

#### Phase 3: Effects and Modulation (NEXT)
1. Add proper filter components (AudioFilterStateVariable)
2. Implement LFO modulation system for filter sweeps
3. Add chorus and reverb effects
4. Enhanced envelope generators with proper ADSR timing

#### Phase 4: Integration and Polish
1. Add real-time parameter control mapping
2. Update UI/control interface for new polyphonic features
3. Optimize voice allocation and CPU usage
4. Testing and final optimization

### 6. Memory and CPU Considerations

#### Audio Memory:
- Each voice requires ~8-10 audio objects
- 4-6 voices = ~40-60 total audio objects
- Estimate additional 10-15 audio memory blocks needed

#### CPU Usage:
- Oscillators: Low overhead
- Filters: Medium overhead per voice
- Effects: Medium overhead (shared across voices)
- Should fit within Teensy 4.x processing budget

### 7. File Structure
```
src/
├── DroneSynthesizer.h        // New drone synth class
├── DroneSynthesizer.cpp      // Implementation
├── HybridSynthesizer.h       // Updated with DRONE mode
├── HybridSynthesizer.cpp     // Updated integration
└── main.cpp                  // Updated mode switching
```

## Expected Sound Result
The drone mode will produce:
- Warm, analog-style pad sounds
- Characteristic filter sweeps reminiscent of the MS-10
- Rich harmonic content with movement and life
- Suitable for atmospheric, ambient, and classic 80s sounds
- Polyphonic capability for chord pads and drones

## Testing Strategy
1. **Single Voice Testing**: Verify basic oscillator + filter chain
2. **Polyphonic Testing**: Ensure voice allocation works correctly  
3. **Modulation Testing**: Verify LFO and envelope behavior
4. **Effects Testing**: Test chorus and reverb integration
5. **Integration Testing**: Verify mode switching and volume controls
6. **Performance Testing**: Monitor CPU and memory usage

## Phase 2 Implementation Summary

**Completed:** Polyphonic voice architecture with advanced voice management

### Key Features Implemented:
- **6-Voice Polyphony**: Full polyphonic capability allowing chord pads and layered drones
- **Voice Allocation System**: Intelligent voice stealing using oldest-voice algorithm
- **MIDI Note Interface**: Now accepts MIDI note numbers instead of frequencies for proper polyphonic tracking
- **Per-Voice Audio Chain**: Each voice has independent oscillators (sawtooth + pulse + sub) and envelope
- **Hierarchical Mixing**: Voice mixers -> Master mixers -> Stereo outputs
- **Voice State Management**: Tracks note on/off times, release phases, and active states

### Technical Architecture:
- `DroneVoice` structure encapsulates complete oscillator + envelope chain per voice
- Two-tier mixer system handles 6 voices efficiently (4+2 voices per mixer level)
- Voice allocation supports note-on/note-off tracking for proper polyphonic behavior
- Global parameters (detune, LFO, etc.) affect all voices simultaneously
- Envelope processing with release phase timing (800ms release in Phase 2)

### Audio Memory Usage:
- **Phase 1**: ~10 audio objects (single voice)
- **Phase 2**: ~70 audio objects (6 voices + mixing infrastructure)
- Memory efficient design using shared LFO and effects (Phase 3)

### Next Steps for Phase 3:
- Add `AudioFilterStateVariable` per voice for proper filter sweeps
- Implement LFO modulation of filter cutoff
- Add chorus and reverb effects for ensemble sound
- Enhanced ADSR envelopes with proper timing curves
