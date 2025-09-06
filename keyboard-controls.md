# TeensySynth Keyboard Controls for Musicians

## Overview
The TeensySynth is a polyphonic hybrid synthesizer running on a Teensy 4.1 microcontroller. It combines Karplus-Strong string synthesis with an analog-style drone synthesizer, giving you access to both plucked string sounds and classic 1980s analog pad sounds.

## Synthesis Modes
The synthesizer has four main modes that can be switched between:

### 1. **STRINGS_ONLY** Mode
- Only the Karplus-Strong string synthesizer is active
- 8-voice polyphony with realistic string pluck sounds
- Each note triggers a physical string simulation

### 2. **DRONE** Mode  
- Only the analog-style drone synthesizer is active
- 6-voice polyphony with rich analog pad sounds
- Designed to recreate classic Korg MS-10 sounds like those in "I Ran" by Flock of Seagulls

### 3. **LAYERED** Mode
- Both string and drone synthesizers play simultaneously
- Creates rich, layered textures combining plucked strings with analog pads
- Full polyphony for both engines

### 4. **SPLIT** Mode
- Keyboard split at Middle C (C4/Note 60)
- **Lower keys (below C4)**: String synthesizer
- **Upper keys (C4 and above)**: Drone synthesizer
- Perfect for bass strings with lead drone sounds

## Control Methods

### Keyboard Function Keys (USB Keyboard)
Connect a USB keyboard to switch between modes:
- **F1**: Strings Only mode
- **F3**: Layered mode (strings + drone)
- **F4**: Split mode
- **F5**: Drone mode

### MIDI Control Change Messages (CC)
Connect a MIDI controller or DAW for real-time parameter control:

#### Mode Switching (Channel 1)
- **CC 51** (value 127): Switch to Strings Only
- **CC 53** (value 127): Switch to Layered mode  
- **CC 54** (value 127): Switch to Split mode
- **CC 55** (value 127): Switch to Drone mode

#### Volume Controls (Channel 1)
- **CC 7**: Master Volume (0-127) - Controls overall output level
- **CC 23**: Drone Volume (0-127) - Controls drone synthesizer level

#### String Synthesizer Controls (Channel 1)
- **CC 21**: String Attenuation (0-127) - Controls string damping/sustain
- **CC 22**: String Filter Strength (0-127) - Controls string filtering

#### Drone Synthesizer Controls (Channel 1)
- **CC 24**: Filter Cutoff (0-127) - Primary expressive control for analog sound
- **CC 25**: LFO Rate (0-127) - Speed of filter sweep (0.1-10 Hz)
- **CC 26**: Oscillator Detune (0-127) - Analog warmth control
  - Value 64 = no detune
  - Values 0-63 = negative detune
  - Values 65-127 = positive detune

#### Global Controls (Channel 1)
- **Pitch Bend**: Pitch bend wheel affects all active voices
  - Range: ±4 semitones
  - Works in all synthesis modes

## MIDI Note Input
- **MIDI Channel 1**: All note input
- **Note Range**: Full 88-key piano range supported (A0-C8)
- **Velocity Sensitive**: All synthesizers respond to note velocity
- **Polyphonic**: 
  - Strings: Up to 8 simultaneous notes
  - Drone: Up to 6 simultaneous notes

## Current Sound Characteristics

### String Synthesizer
- **Sound**: Realistic plucked string simulation using Karplus-Strong algorithm
- **Attack**: Immediate pluck attack
- **Sustain**: Natural string decay with controllable damping
- **Timbre**: Warm, organic string sound

### Drone Synthesizer (Current Phase 2 Implementation)
- **Oscillators**: Dual sawtooth + pulse waves with sub-oscillator
- **Polyphony**: 6-voice with intelligent voice allocation  
- **Envelope**: Slow attack (200ms), high sustain (0.8), medium release (800ms)
- **Sound**: Rich analog-style pads perfect for atmospheric sounds
- **Detuning**: Slight oscillator detuning for analog warmth

## Usage Tips for Musicians

### For Classic Rock/New Wave Sounds
1. Use **DRONE** mode with **CC 24** (Filter Cutoff) mapped to a knob or slider
2. Play sustained chords while sweeping the filter for that classic "I Ran" sound
3. Use **CC 25** (LFO Rate) to add automatic filter movement

### for String Ensemble Sounds
1. Use **STRINGS_ONLY** mode for realistic string section
2. Adjust **CC 21** (Attenuation) for longer or shorter string sustain
3. Use **CC 22** (Filter Strength) to darken or brighten the string tone

### For Rich Layered Textures  
1. Use **LAYERED** mode to combine both synthesizers
2. Balance the levels with **CC 7** (Master) and **CC 23** (Drone Volume)
3. Perfect for ambient, cinematic, and progressive rock sounds

### For Performance Flexibility
1. Use **SPLIT** mode for bass strings (left hand) and lead drone (right hand)
2. The split point is fixed at Middle C (note 60)
3. Great for solo performances with accompaniment

## Planned Future Features (Phase 3)
The following features are planned for the next update:

- **Per-voice filtering** for more dynamic drone sounds
- **LFO modulation** of filter cutoff for automatic sweeps
- **Chorus and reverb effects** for wider, more spacious sounds  
- **Enhanced ADSR envelopes** with full attack/decay/sustain/release control

## Technical Notes
- **Latency**: Ultra-low latency audio processing
- **Sample Rate**: 44.1 kHz
- **Audio Memory**: Optimized for real-time performance
- **CPU Usage**: Efficient processing leaves room for future expansion

## Connection Setup
1. **Audio Output**: Connect headphones or speakers to Teensy audio output
2. **MIDI Input**: Connect MIDI keyboard/controller to Teensy USB host port
3. **USB Keyboard**: Connect standard USB keyboard for mode switching
4. **Power**: Use quality USB power supply for stable operation

## Troubleshooting
- If no sound: Check audio connections and master volume (CC 7)
- If MIDI not responding: Ensure MIDI device is on Channel 1
- If mode switching not working: Try both function keys and MIDI CC controls
- For best results: Use MIDI controller with knobs/sliders for real-time control
