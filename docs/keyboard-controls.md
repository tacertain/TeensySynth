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

### 6. **IRAN** Mode
- **Split mode** targeting "I Ran" by Flock of Seagulls
- **Split point**: Middle C (MIDI note 60)
- **Below Middle C**: Drone synthesizer at fixed full velocity
- **Above Middle C**: String pads transposed up two octaves at fixed full velocity
  - **F-A (notes 53-57)**: Auto-expanded to major chords
  - **Middle C (note 60)**: Auto-expanded to root + octave
  - **All other keys**: Single notes

### 7. **TUSK** Mode
- **Soundfont split mode** using two loaded instruments
- **Split point**: Middle C (MIDI note 60)
- **Below Middle C**: Soundfont instrument 1 (typically bass/low sounds)
- **Above Middle C**: Soundfont instrument 0 (typically lead/high sounds)
- **Perfect for expressive performance** with different timbres across the keyboard

### 8. **TUSK_CHORD** Mode
- **TUSK split with automatic chord expansion** above the split point
- **Split point**: Middle C (MIDI note 60)
- **Below Middle C**: Soundfont instrument 1, single note at max velocity
- **Above Middle C**: Soundfont instrument 0, with the played note expanded to a 5-note voicing at max velocity
- **Chord-mapped root notes** (others play as single notes):
  - **A** → A, D, F, A-2oct, D-2oct
  - **G** → G, C, E, G-2oct, C-2oct
  - **F** → F, A, D, F-2oct, A-2oct
  - **B** → G, B, D, G-2oct, B-2oct
  - **C#** → A, C#, E, A-2oct, C#-2oct
- Targets the "Tusk" trumpet voicing — one finger triggers the recorded chord stab

### 9. **WHITESNAKE** Mode
- **Prophet VS pad** mode targeting the keys sound from "Here I Go Again" (1987)
- Loads `whitesnake.sf2` via a dedicated lean pad synth (`SoundfontPadSynthesizer`) — no per-voice filter or crossfade
- **8-voice polyphony** to absorb note overlap during the long 800 ms release
- Each voice plays both the pressed note and a sub-octave layer mixed by CC 25
- Standard polyphonic playback across the full keyboard — no split point
- **Chord-velocity capture**: notes arriving within 30 ms of each other are buffered and fired together at one shared velocity. Compensates for keyboards that strike chord notes at uneven velocities. The shared velocity is:
  - **20** if any note in the chord is below 20 (and none above 100)
  - **100** if any note is above 100 (and none below 20)
  - the **median** otherwise (including when the chord spans both extremes)

  While any captured note is still held, subsequent notes fire immediately at the pinned velocity.
- **Velocity smoothing**: chord-pinned velocities feed a continuous-time EMA with a fixed τ ≈ 2.7 s. The pad plays each chord at the smoothed value, not the raw chord velocity, so the macro arc of a song (e.g. intro → chorus build) is decoupled from per-chord velocity variation. Held notes added to an already-fired chord match the smoothed level. Long pauses naturally reset the smoother (α → 1 on the next chord).
- **Velocity floor (dB range)**: CC 26 sets the pre-square floor of the velocity curve (0.0 = full dynamic range, 1.0 = flat). Default ~0.39 (CC 26 = 50) → amp floor ~0.155 → ~16 dB. Higher values compress the dynamic range; lower values open it up.
- **Highpass split on main**: each voice's main signal is split into a dry path and a parallel highpass branch, summed at the voice mixer (sub-octave is untouched). HP cutoff = noteHz × multiplier, set per voice at noteOn. CC 27 sets the multiplier (1.0×–4.0×, default 3.1×); CC 28 sets the HP branch mix (0.0–2.0, default ~0.94 at CC 28 = 60).
- CC 21-28 control pad parameters live (see Soundfont Synthesizer Controls).

## Control Methods

### MIDI Control Change Messages (CC)
Connect a MIDI controller or DAW for real-time parameter control:

#### Mode Switching (Channel 1)
- **CC 51** (value 127): Switch to Plucked Strings
- **CC 52** (value 127): Switch to Drone mode
- **CC 53** (value 127): String Pads + **"Bright Strings"** preset
- **CC 54** (value 127): Switch to Soundfont mode and load `trombone.sf2` (experimental SF slot)
- **CC 55** (value 127): Switch to **TUSK mode** - Soundfont split mode with instrument 0 above split point, instrument 1 below
- **CC 56** (value 127): Switch to **IRAN mode** - drone below Middle C, string pads above (with chord modes on F-A and Middle C; see Channel 7 routing)
- **CC 57** (value 127): Switch to **WHITESNAKE mode** - loads `whitesnake.sf2` (Prophet VS pad for "Here I Go Again")
- **CC 58** (value 127): Switch to **TUSK_CHORD mode** - TUSK split with chord-mapping above the split point
- **CC 59** (value 127): Cycle String Pad Chord Mode (see below)

#### Volume Controls (Channel 1)
- **CC 7**: Master Volume (0-127) — value/64 mapping, so **64 = unity** and 127 ≈ 2× boost (intentional headroom)
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

CC 21-24 are shared across all four soundfont modes. CC 25-28 differ by mode and are documented in the subsections below.

**Shared (all soundfont modes):**
- **CC 21**: Attack (0-127) — exponential, 0.5 ms – 2000 ms
- **CC 22**: Decay (0-127) — exponential, 0.5 ms – 2000 ms
- **CC 23**: Sustain (0-127) — linear, 0.0 – 1.0
- **CC 24**: Release (0-127) — exponential, 5 ms – 5000 ms

##### SOUNDFONT / TUSK / TUSK_CHORD
- **CC 25**: Filter Cutoff (0-127) — exponential multiplier of note frequency, 0.5× – 20×
- **CC 26**: Filter Resonance (0-127) — linear Q, 0.7 – 5.0
- **CC 27**: Crossfade Duration (0-127) — exponential, 10 ms – 5000 ms
- **CC 28**: Soundfont/Pad Volume (0-127) — linear, value × (4/128). **32 = unity**, 127 ≈ 4× boost. Shares the downstream mixer gain with the Whitesnake pad path (but is not bound in WHITESNAKE — see below).

##### WHITESNAKE
- **CC 25**: Sub-Octave Mix (0-127) — linear, 0.0 (no sub) – 1.0 (sub at unity with main). Default at CC 25 = 95 (~0.75).
- **CC 26**: Velocity Floor (0-127) — linear 0.0 – 1.0, pre-square floor of the velocity curve. Default at CC 26 = 50 (~0.39) → amp floor ~0.155 → **~16 dB** range. Higher values compress range (e.g. 64 → 12 dB, 127 → flat); lower values expand it.
- **CC 27**: Highpass Multiplier (0-127) — linear 1.0× – 4.0× of note frequency. Per-voice HP cutoff = noteHz × multiplier, pinned at noteOn. Default at CC 27 = 89 (~3.1×).
- **CC 28**: Highpass Mix (0-127) — linear 0.0 – 2.0, gain on the HP branch summed with the dry main at the voice mixer. Default at CC 28 = 60 (~0.94). Note: CC 28 is the HP mix in WHITESNAKE; the soundfont/pad volume control is not exposed in this mode — use CC 7 master volume instead.

The WHITESNAKE velocity smoother runs at a fixed τ ≈ 2.7 s (no CC binding).

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

> **Note:** CC 41-48 are dual-purpose. The mappings above apply in non-WHITESNAKE modes. In WHITESNAKE mode, CC 41-48 reroute to the path-B pad FX chain documented below.

#### WHITESNAKE Pad FX Controls (Channel 1) — bank 4, active only in WHITESNAKE mode

Pair structure: low CC in each pair = baseline/amount, high CC = modulation/character.

**LP filter (always inline):**
- **CC 41**: LP Cutoff Multiplier (0-127) — exponential, 1.0× – 20.0× of note frequency. Clamped internally to 8 kHz to keep the Chamberlin SVF stable. Default 7.6× (CC 41 = 86).
- **CC 42**: LP Resonance (Q) (0-127) — linear, 0.7 – 4.0. Default Q = 1.12 (CC 42 = 16). Above ~3.5 risks self-oscillation.

**LP filter modulation (per-voice free-running sine, randomized phase per voice):**
- **CC 43**: LP LFO Depth (0-127) — linear, 0.0 – 1.0. 0 = filter static; 1 = full ±octaveControl swing. Default 0.5 (CC 43 ≈ 64).
- **CC 44**: LP LFO Rate (0-127) — exponential, 0.05 – 1.5 Hz. Default 0.20 Hz (CC 44 ≈ 52).

**Chorus (two parallel `AudioEffectFlange` instances, dry-cancelled at the chorus bus):**
- **CC 45**: Chorus Wet Mix (0-127) — linear, 0.0 – 1.0. Default 0.433 (CC 45 = 55).
- **CC 46**: Chorus Depth (0-127) — linear, 0.0 – 1.0 fraction. 0.5 = default flange depths (132/176 samples). Re-inits both flanges on every change — **clicks audibly on slider sweeps**. Intended as a set-once sound-design knob, not a live performance control. Default 0.567 (CC 46 = 72).

**Reverb (`AudioEffectFreeverb`, fed from dry + chorus pre-reverb sum):**
- **CC 47**: Reverb Wet Mix (0-127) — linear, 0.0 – 1.0. Default 0.591 (CC 47 = 75).
- **CC 48**: Reverb Room Size (0-127) — linear, 0.0 – 1.0 → `Freeverb::roomsize()`. Default 0.591 (CC 48 = 75).

See `docs/prophet-vs/path-b-tuning-guide.md` for the exploration workflow.

#### Mode and Chord Controls (Channel 1)
- **CC 59** (value 127): Cycle **String Pad Chord Mode**:
  - **First press**: Major Chord Mode (6-note major chords: root + 3rd + 5th across two octaves)
  - **Second press**: Octave Mode (2-note: root + octave)
  - **Third press**: Off (single notes only)
  - **Cycles continuously** with each CC 59 trigger

*Note: All string pad parameter changes affect all 6 active voices simultaneously for real-time performance control.*

*Note: String pad presets are selected by entering STRING_PADS mode (CC 53). You can manually adjust CC 41-44 after switching for fine-tuning.*

#### Global Controls (Channel 1)
- **Pitch Bend**: Pitch bend wheel affects all active voices
  - Range: ±4 semitones
  - Works in all synthesis modes

## MIDI Note Input
- **MIDI Channel 1**: Default note input; the current mode (set via CC 51-58) decides which engine plays.
- **MIDI Channels 2-9**: Per-channel mode override — notes on these channels are routed to a specific engine regardless of the active mode. Useful for split keyboards, sequencers, or multi-zone controllers.
  - **Ch 2**: PLUCKED_STRINGS
  - **Ch 3**: DRONE
  - **Ch 4**: STRING_PADS
  - **Ch 5**: SOUNDFONT
  - **Ch 6**: SPLIT
  - **Ch 7**: IRAN
  - **Ch 8**: TUSK
  - **Ch 9**: WHITESNAKE
- **Note Range**: Full 88-key piano range supported (A0-C8)
- **Velocity Sensitive**: All synthesizers respond to note velocity
- **Polyphonic Voice Limits**:
  - **Plucked Strings**: Up to 8 simultaneous notes
  - **Drone**: Up to 6 simultaneous notes
  - **String Pads**: Up to 6 simultaneous notes with intelligent voice allocation
  - **Soundfont (SOUNDFONT, TUSK, TUSK_CHORD)**: 4 voices per loaded instrument slot. TUSK/TUSK_CHORD use two slots, giving 4 voices below the split and 4 above. TUSK_CHORD voicings consume one voice each, so a 5-note chord exhausts the upper slot.
  - **Whitesnake Pad**: 8 voices. Voice stealing prefers releasing voices before active ones.

### Velocity Response
- **Input curve** (all modes): a plain linear normalization — float velocity = `MIDI byte / 128`. No clipping, no compensation.
- **Whitesnake pad output curve**: a square-law amplitude curve, aligned with the `/128` input shaper. Raw MIDI velocity ≤ 20 floors at amp = `floor²`; ≥ 100 ceilings at amp = 1.0; between, amp = `(floor + (1−floor)·t)²` where t = (v − 20/128) / (80/128). The `floor` is set by CC 26 (default ~0.39 at CC 26 = 50 → amp floor ~0.155 → ~16 dB range). The squared ramp gives a perceptually natural response with a positive second derivative (concave up). The amp is then quantized to a 0-127 MIDI byte before being passed to the wavetable.
- **Whitesnake velocity smoothing**: upstream of the curve, chord-pinned velocities pass through a continuous-time EMA at a fixed τ ≈ 2.7 s. The smoothed value drives every note in the chord, so per-chord variation is absorbed and the macro arc of a song builds across multiple chords rather than within one.

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
- If no sound: Check audio connections and master volume (CC 7, unity at value 64)
- If MIDI not responding: Ensure MIDI device is on Channel 1 (or one of the per-channel override channels 2-9)
- If mode switching not working: Use MIDI CC controls — 51 (Plucked), 52 (Drone), 53 (String Pads), 54 (Soundfont/trombone), 55 (TUSK), 56 (IRAN), 57 (WHITESNAKE), 58 (TUSK_CHORD)
- For best results: Use MIDI controller with knobs/sliders for real-time control
