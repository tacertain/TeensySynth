# TeensySynth MIDI Controls for Musicians

## Overview
The TeensySynth is a polyphonic hybrid synthesizer running on a Teensy 4.1 microcontroller. It combines three distinct synthesis methods: Karplus-Strong plucked strings, analog-style drone synthesis, and classic 80s string pads.

## Synthesis Modes
The synthesizer has multiple synthesis modes:

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
- 6-voice polyphony with intelligent voice allocation (Phase 2)
- Warm, sustained string pad sounds with 3-layer ensemble detuning per voice
- Supports full chord playing and complex harmonies
- **Three chord modes**: Off (single notes), Major (6-note chords), Octave (2-note octaves)

### 4. **SOUNDFONT** Mode
- SoundFont 2 (.sf2) file synthesizer
- 8-voice polyphony with wavetable synthesis
- Dynamic instrument loading from SD card
- Supports multiple instruments loaded simultaneously

### 5. **SPLIT** Mode
- **Hybrid mode** combining drone and string pad synthesizers
- **Split point**: Middle C (MIDI note 60)
- **Below Middle C**: Drone synthesizer (bass/pad sounds)
- **Above Middle C+12**: String pad synthesizer (lead/melody sounds)
- **Perfect for solo performance** with bass accompaniment and lead melodies

### 6. **TUSK** Mode ✅ **NEW**
- **Soundfont split mode** using two loaded instruments
- **Split point**: Middle C (MIDI note 60)
- **Below Middle C**: Soundfont instrument 1 (typically bass/low sounds)
- **Above Middle C**: Soundfont instrument 0 (typically lead/high sounds)
- **Perfect for expressive performance** with different timbres across the keyboard

### 7. **WHITESNAKE** Mode
- **Prophet VS pad** mode targeting the keys sound from "Here I Go Again" (1987)
- Loads `whitesnake.sf2` via a dedicated lean pad synth (`SoundfontPadSynthesizer`) — no per-voice filter or crossfade
- **8-voice polyphony** to absorb note overlap during the long 800 ms release
- Standard polyphonic playback across the full keyboard — no split point
- CC 21-24 control the pad ADSR live (see Soundfont Synthesizer Controls). CC 25-27 are inert in WHITESNAKE since the pad has no filter or crossfade.

## Control Methods

### MIDI Control Change Messages (CC)
Connect a MIDI controller or DAW for real-time parameter control:

#### Mode Switching (Channel 1)
- **CC 51** (value 127): Switch to Plucked Strings
- **CC 52** (value 127): Switch to Drone mode
- **CC 53** (value 127): String Pads + **"Bright Strings"** preset
- **CC 54** (value 127): Switch to Soundfont mode and load `trombone.sf2` (experimental SF slot)
- **CC 55** (value 127): Switch to **TUSK mode** - Soundfont split mode with instrument 0 above split point, instrument 1 below
- **CC 56** (value 127): Switch to Soundfont mode and load `trombone_tusk.sf2` into slot 1 (experimental SF slot)
- **CC 57** (value 127): Switch to **WHITESNAKE mode** - loads `whitesnake.sf2` (Prophet VS pad for "Here I Go Again")
- **CC 58** (value 127): Switch to **TUSK_CHORD mode** - TUSK split with chord-mapping above the split point
- **CC 59** (value 127): Cycle String Pad Chord Mode (see below)

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

> **Note:** CC 21-27 are dual-purpose. The mappings above (and the String CC 21/22 / Drone Volume CC 23 above them) apply in non-soundfont modes. When any soundfont mode is active (SOUNDFONT, TUSK, TUSK_CHORD, WHITESNAKE), CC 21-27 reroute to the Soundfont Synthesizer Controls below.

#### Soundfont Synthesizer Controls (Channel 1) — active in SOUNDFONT, TUSK, TUSK_CHORD, WHITESNAKE
- **CC 21**: Attack (0-127) — exponential, 0.5 ms – 2000 ms
- **CC 22**: Decay (0-127) — exponential, 0.5 ms – 2000 ms
- **CC 23**: Sustain (0-127) — linear, 0.0 – 1.0
- **CC 24**: Release (0-127) — exponential, 5 ms – 5000 ms
- **CC 25**: Filter Cutoff (0-127) — exponential multiplier of note frequency, 0.5× – 20× *(SOUNDFONT/TUSK/TUSK_CHORD only)*
- **CC 26**: Filter Resonance (0-127) — linear Q, 0.7 – 5.0 *(SOUNDFONT/TUSK/TUSK_CHORD only)*
- **CC 27**: Crossfade Duration (0-127) — exponential, 10 ms – 5000 ms *(SOUNDFONT/TUSK/TUSK_CHORD only)*

*Note: ADSR settings layer on top of the SF2's own envelope. The SF2's envelope shapes the wavetable output; the Teensy envelope shapes that further before the filter.*

*Note: CC 21-24 dispatch to the active soundfont engine — the recorded-instrument `SoundfontSynthesizer` in SOUNDFONT/TUSK/TUSK_CHORD modes, the lean `SoundfontPadSynthesizer` in WHITESNAKE.*

#### String Pad Synthesizer Controls (Channel 1) - 6-Voice Polyphonic
- **CC 41**: String Pad Volume (0-127) - Controls overall string pad level
- **CC 42**: String Pad Filter Cutoff (0-127) - Filter brightness for all voices (200-2000 Hz range) 
- **CC 43**: String Pad Filter Resonance (0-127) - Filter resonance for all voices
- **CC 44**: String Pad Detune Amount (0-127) - Ensemble detuning for all voices (0-15 cents range)
- **CC 45**: High-Pass Filter Multiplier (0-127) - Controls high-pass filtering per voice
  - **Value 0**: Multiplier = 0.05 (minimal high-pass, more bass content)
  - **Value 64**: Multiplier = 1.0 (default, 80% of fundamental frequency)
  - **Value 127**: Multiplier = 4.0 (aggressive high-pass, tighter sound)
  - **Uses exponential mapping** for smooth control across the range

#### Mode and Chord Controls (Channel 1)
- **CC 59** (value 127): Cycle **String Pad Chord Mode**:
  - **First press**: Major Chord Mode (6-note major chords: root + 3rd + 5th across two octaves)
  - **Second press**: Octave Mode (2-note: root + octave)
  - **Third press**: Off (single notes only)
  - **Cycles continuously** with each CC 59 trigger

*Note: All string pad parameter changes affect all 6 active voices simultaneously for real-time performance control.*

*Note: String pad presets are selected via mode switching (CC 53-57). You can manually adjust CC 41-44 after switching to a preset mode for fine-tuning.*

#### Global Controls (Channel 1)
- **Pitch Bend**: Pitch bend wheel affects all active voices
  - Range: ±4 semitones
  - Works in all synthesis modes

## MIDI Note Input
- **MIDI Channel 1**: All note input
- **Note Range**: Full 88-key piano range supported (A0-C8)
- **Velocity Sensitive**: All synthesizers respond to note velocity
- **Polyphonic Voice Limits**: 
  - **Plucked Strings**: Up to 8 simultaneous notes
  - **Drone**: Up to 6 simultaneous notes  
  - **String Pads**: Up to 6 simultaneous notes with intelligent voice allocation

### String Pad Polyphonic Behavior
- **Voice Allocation**: New notes automatically find available voices
- **Voice Stealing**: When all 6 voices are playing, the oldest note is smoothly replaced
- **Note Tracking**: Each MIDI note is tracked individually for proper note-off behavior
- **Chord Playing**: Full support for 6-note chords and complex harmonies


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

### For String Pad Chord Playing ✅ **NEW**
1. Use **CC 59** to cycle through chord modes in String Pad mode:
   - **Major Chord Mode**: Play full 6-note major chords (great for rich harmonies)
   - **Octave Mode**: Play root note + octave (adds fullness without complexity)
   - **Off Mode**: Traditional single-note playing
2. Use **CC 45** to adjust bass content:
   - **Low values (0-30)**: Fuller, warmer sound with more bass
   - **Mid values (around 64)**: Balanced sound (default)
   - **High values (90-127)**: Tighter, more focused sound

### For Anti-Aliasing and Sound Shaping ✅ **NEW**
1. **High-pass filtering** is now available per voice with **CC 45**
2. **Final anti-aliasing filter** at 8kHz removes harsh high frequencies
3. Adjust **CC 45** to taste:
   - Higher values for cleaner, more defined sound
   - Lower values for warmer, fuller sound with more low-frequency content

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
- **Anti-Aliasing**: Dual-stage filtering system ✅ **NEW**
  - **Per-voice high-pass filters** remove sub-harmonic interference and control bass content
  - **Final 8kHz low-pass filter** eliminates aliasing from sawtooth harmonics
  - **Result**: Clean, professional sound quality across all note ranges

## Connection Setup
1. **Audio Output**: Connect headphones or speakers to Teensy audio output
2. **MIDI Input**: Connect MIDI keyboard/controller to Teensy USB host port
3. **Power**: Use quality USB power supply for stable operation

## Troubleshooting
- If no sound: Check audio connections and master volume (CC 7)
- If MIDI not responding: Ensure MIDI device is on Channel 1
- If mode switching not working: Use MIDI CC controls (51/54/55)
- For best results: Use MIDI controller with knobs/sliders for real-time control
