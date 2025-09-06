# TeensySynth MIDI Controls for Musicians

## Overview
The TeensySynth is a polyphonic hybrid synthesizer running on a Teensy 4.1 microcontroller. It combines three distinct synthesis methods: Karplus-Strong plucked strings, analog-style drone synthesis, and classic 80s string pads.

## Synthesis Modes
The synthesizer has three main modes:

### 1. **PLUCKED_STRINGS** Mode
- Karplus-Strong string synthesizer for realistic plucked string sounds
- 8-voice polyphony with physical string simulation
- Each note triggers a string pluck with natural decay

### 2. **DRONE** Mode  
- Analog-style polyphonic drone synthesizer
- 6-voice polyphony with rich analog pad sounds
- Designed to recreate classic Korg MS-10 sounds like those in "I Ran" by Flock of Seagulls

### 3. **STRING_PADS** Mode
- Classic 80s string synthesizer with ensemble chorus effect
- Single voice (Phase 1 implementation)
- Warm, sustained string pad sounds with 3-layer ensemble detuning

## Control Methods

### MIDI Control Change Messages (CC)
Connect a MIDI controller or DAW for real-time parameter control:

#### Mode Switching (Channel 1)
- **CC 51** (value 127): Switch to Plucked Strings
- **CC 54** (value 127): Switch to Drone mode
- **CC 55** (value 127): Switch to String Pads mode

#### Volume Controls (Channel 1)
- **CC 7**: Master Volume (0-127) - Controls overall output level
- **CC 23**: Drone Volume (0-127) - Controls drone synthesizer level

#### String Synthesizer Controls (Channel 1)
- **CC 21**: String Attenuation (0-127) - Controls string damping/sustain
- **CC 22**: String Filter Strength (0-127) - Controls string filtering

#### Drone Synthesizer Controls (Channel 1)
- **CC 24**: Filter Cutoff (0-127) - Primary expressive control for analog sound with per-voice filtering
- **CC 25**: LFO Rate (0-127) - Speed of automatic filter sweep (0.1-10 Hz) for breathing/pulsing effects
- **CC 26**: Oscillator Detune (0-127) - Analog warmth control
  - Value 64 = no detune
  - Values 0-63 = negative detune
  - Values 65-127 = positive detune
- **CC 27**: LFO Depth (0-127) - Amount of filter modulation (0 = no LFO effect, 127 = maximum sweep)

#### String Pad Synthesizer Controls (Channel 1)
- **CC 41**: String Pad Volume (0-127) - Controls string pad synthesizer level
- **CC 42**: String Pad Filter Cutoff (0-127) - Filter brightness (200-2000 Hz range)
- **CC 43**: String Pad Filter Resonance (0-127) - Filter resonance amount
- **CC 44**: String Pad Detune Amount (0-127) - Ensemble detuning (0-15 cents range)

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


## Default Synth Parameters

When the TeensySynth starts, the following default values are set for the drone synthesizer:

- **Detune:** -0.11 (slight negative detuning for analog warmth)
- **LFO Depth:** 0.75 (strong filter modulation by default)
- **Filter Cutoff:** 0.3 (low/mid filter position for mellow pad sound)
- **LFO Rate:** 0.33 Hz (slow breathing/pulsing effect)

You can override these defaults at any time using the MIDI CC controls:
- **CC 24:** Filter Cutoff
- **CC 25:** LFO Rate
- **CC 26:** Detune
- **CC 27:** LFO Depth

---
## Current Sound Characteristics

### String Synthesizer
- **Sound**: Realistic plucked string simulation using Karplus-Strong algorithm
- **Attack**: Immediate pluck attack
- **Sustain**: Natural string decay with controllable damping
- **Timbre**: Warm, organic string sound

### Drone Synthesizer (Phase 3 Implementation - Steps 1-2 Complete)
- **Oscillators**: Dual sawtooth + pulse waves with sub-oscillator
- **Polyphony**: 6-voice with intelligent voice allocation  
- **Filtering**: ✅ **NEW** - Individual lowpass filters per voice with cutoff and resonance control
- **LFO Modulation**: ✅ **NEW** - Automatic filter cutoff modulation for breathing/sweeping effects
- **Envelope**: Slow attack (200ms), high sustain (0.8), **fast release (50ms)** ✅ **UPDATED**
- **Sound**: Rich analog-style pads with dynamic filter sweeps and responsive note releases
- **Detuning**: Slight oscillator detuning for analog warmth

## Usage Tips for Musicians

### For Classic Rock/New Wave Sounds
1. Use **DRONE** mode with **CC 24** (Filter Cutoff) mapped to a knob or slider
2. Set **CC 27** (LFO Depth) to mid-range (64) for moderate automatic movement
3. Adjust **CC 25** (LFO Rate) for desired breathing speed - try value 32 for slow, atmospheric sweeps
4. Play sustained chords while sweeping **CC 24** for that classic "I Ran" sound
5. Use **CC 27** set to 0 to disable LFO for static filter sounds, or 127 for maximum movement

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

## Planned Future Features (Phase 3 - Steps 3-4)
The following features are planned for the next update:

- ✅ **Per-voice filtering** - COMPLETED: Each voice now has individual lowpass filtering
- ✅ **LFO modulation** - COMPLETED: Automatic filter cutoff sweeps now active  
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
3. **Power**: Use quality USB power supply for stable operation

## Troubleshooting
- If no sound: Check audio connections and master volume (CC 7)
- If MIDI not responding: Ensure MIDI device is on Channel 1
- If mode switching not working: Use MIDI CC controls (51/54/55)
- For best results: Use MIDI controller with knobs/sliders for real-time control
