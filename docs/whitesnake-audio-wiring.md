# Whitesnake Mode — Audio Signal Path

End-to-end wiring from the loaded SF2 instrument to the I2S/USB outputs.
Read top-to-bottom; every arrow is a live `AudioConnection`. Gain values are
the multiplicative factor applied at each mixer slot.

```
   whitesnake.sf2 (instrument 0, sorted sample zones)
   borrowed by SoundfontPadSynthesizer as instrumentData
                        │
                        │  (8 polyphonic voices, mono)
                        ▼
   ┌──────────────────────────────────────────────────────────────────┐
   │  SoundfontPadSynthesizer  (SoundfontPadSynthesizer.cpp)          │
   │                                                                  │
   │   Per voice (i = 0..7) — main is split into a dry path and a     │
   │   highpass branch, then summed with the sub-octave:              │
   │                                                                  │
   │       mainVoices[i] ──┬─────────────────────► voiceMixers[i].0   │
   │         plays note N  │                       gain = mainAmp     │
   │                       │                                          │
   │                       └─► hpFilters[i] ─────► voiceMixers[i].2   │
   │                          (state-variable HP)  gain = mainAmp     │
   │                          cutoff = noteHz             * hpMix     │
   │                                 * hpMultiplier      (CC 28,      │
   │                          (CC 27, 1.0x–4.0x,          0.0–2.0,    │
   │                           default 3.1x)              default 1.0)│
   │                                                                  │
   │       subVoices[i]  ────────────────────────► voiceMixers[i].1   │
   │         plays note N−12                       gain = mainAmp     │
   │         (skipped if N<12)                            * octaveMix │
   │                                                     (CC 25)      │
   │                                                                  │
   │                          voiceMixers[i] ──► envelopes[i] ──►     │
   │                                                                  │
   │   `mainAmp` = `voiceBaseAmp[i] * expression` (square-law         │
   │   velocity curve captured at noteOn; `expression` is the live    │
   │   swell scalar). Voices 0–3 sum into mixerA, 4–7 into mixerB,    │
   │   both into finalMixer; per-voice slot gains on mixerA/B =       │
   │   `volume * 0.9` (volume defaults to 1.0; the 0.9 is a HP-       │
   │   overlay safety pad — see headroom note below). finalMixer      │
   │   slots [0]=[1]=1.0.                                             │
   │                                                                  │
   │       v0──►envA[0]──┐                                            │
   │       v1──►envA[1]──┤──►mixerA──┐                                │
   │       v2──►envA[2]──┤            │                               │
   │       v3──►envA[3]──┘            ├──►finalMixer──► getOutput()   │
   │       v4──►envA[4]──┐            │                               │
   │       v5──►envA[5]──┤──►mixerB──┘                                │
   │       v6──►envA[6]──┤                                            │
   │       v7──►envA[7]──┘                                            │
   │                                                                  │
   │   The Whitesnake VS samples carry >12 dB of internal headroom,   │
   │   which used to make per-voice unity safe. With the HP overlay   │
   │   live, a single voice can now sum main + 2×HP + sub against     │
   │   the same headroom budget (at hpMix=2.0, octaveMix=1.0, full    │
   │   velocity), which CAN clip on dense chords at full master +     │
   │   soundfont volume. The per-voice 0.9 pad on mixerA/B is the     │
   │   safety margin. If `whitesnakePadPeakMonitor` reports clipping, │
   │   drop `volume` or downstream gains further — do not raise the   │
   │   pad back to unity.                                             │
   └──────────────────────────────────────────────────────────────────┘
                        │
                        │  mono pad signal
                        │
        ┌───────────────┼───────────────────────────┐
        │               │                           │
        ▼               ▼                           ▼
   whitesnakePad     mixerL6 slot 1            mixerR6 slot 1
   PeakMonitor       gain = soundfontGain      gain = soundfontGain
   (debug tap;       = masterVolume            = masterVolume
   no audio          * soundfontVolume         * soundfontVolume
   downstream)       (mixerL6 slot 0 also      (mixerR6 slot 0 also
                      carries Soundfont-        carries Soundfont-
                      Synthesizer L at the      Synthesizer R at the
                      same gain, but is         same gain, but is
                      silent in this mode)      silent in this mode)
                          │                           │
                          ▼                           ▼
                     sumL slot 3                 sumR slot 3
                     gain = 1.0                  gain = 1.0
                     (slots 0/1/2 are            (slots 0/1/2 are
                      strings/drone/             strings/drone/
                      stringPad — silent         stringPad — silent
                      in this mode)              in this mode)
                          │                           │
                  ┌───────┼───────┐           ┌───────┼───────┐
                  │       │       │           │       │       │
                  ▼       ▼       ▼           ▼       ▼       ▼
              peakMon-  i2s1    usb2      peakMon-  i2s1    usb2
              itorL     ch 0    ch 0      itorR     ch 1    ch 1
              (debug    │       (USB_     (debug    │       (USB_
              tap)      │       AUDIO     tap)      │       AUDIO
                        │       only)               │       only)
                        ▼                           ▼
                     DAC L                       DAC R
```

## Effective per-voice gain to the DAC

For one active pad voice, the multiplicative chain to either DAC channel is:

    voice → voiceMixer (dry + sub + HP sum) → env
         → mixerA/B (×volume × 0.9)
         → finalMixer (×1.0)
         → mixerL6/R6 (×masterVolume × soundfontVolume)
         → sumL/R (×1.0) → DAC

At default `volume=1.0`, `masterVolume=1.0`, `soundfontVolume=1.0` the
effective gain per dry voice into the DAC is **0.9** — the per-voice 0.9
pad on mixerA/B is HP-overlay headroom (see the headroom note in the
diagram). At hpMix=1.0 the voiceMixer sum is roughly `mainAmp * (1 + 1)` 
before the sub, so the 0.9 pad still leaves comfortable margin. At hpMix=2.0
the sum can approach `mainAmp * (1 + 2 + octaveMix)` per voice, and dense
chords at full velocity + full master + full soundfont volume CAN clip.
If you see clipping in the `whitesnakePadPeakMonitor`, drop `volume` or the
downstream master/mode gains rather than relaxing the 0.9 pad.

## Code references

- Per-voice sub-osc + HP graph: `SoundfontPadSynthesizer.cpp` constructor
  (`mainToVoiceMixer`/`mainToHpFilter`/`hpFilterToVoiceMixer`/
  `subToVoiceMixer`/`voiceMixerToEnvelope`)
- Per-voice HP cutoff set: `SoundfontPadSynthesizer::noteOn` and
  `setHighpassMultiplier` (recomputes for every sounding voice)
- Per-voice mixer gains: `SoundfontPadSynthesizer::updateMixerGains` and
  `updateVoiceMixerGains`
- L/R fanout and mode mixers: `HybridSynthesizer.h` — pad to `mixerL6/R6` slot 1,
  then `mixerL6/R6` to `sumL/R` slot 3
- Mode-level gain math: `HybridSynthesizer::updateMixerGains`
- Pad peak tap: `whitesnakePadPeakMonitor` (debug-only, no audio downstream)

## Cross-cutting context (not in code)

### `sf22aswt` is a pinned fork commit
`platformio.ini` points `lib_deps` at
`https://github.com/tacertain/sf22aswt.git#026725f4…` rather than the
`manicken/sf22aswt` registry release. The fork carries a one-line fix to
`PER_HERTZ_PHASE_INCREMENT` in `sf22aswt_converter.cpp` — upstream had a
stray `+ 0.5f` left over from when the field was integer, which now causes
notes to drift sharper with rising root frequency (~0.1 cents per Hz of
root). The fix is a PR open against `manicken/sf22aswt`; once merged and
re-released, this line can revert to a registry tag. **Heads up:** the
old per-sample `CENTS_OFFSET` tweaks dialed in by ear on the
trumpet/trombone SF2s were compensating for this bug and now over-correct
in the opposite direction — they need re-checking after the fix.

### Velocity shaping is all in the pad
`MIDIController::midiVelocityToFloat` is now a plain `v / 128` —
no clipping, no compensation, identical across all modes. The whole
velocity-to-amplitude curve for WHITESNAKE lives in
`SoundfontPadSynthesizer::noteOn`:

- Inputs ≤ 30/128 floor to amp = (0.25)² = 0.0625
- Inputs ≥ 70/128 ceiling to amp = 1.0
- Between: `amp = (0.25 + 0.75·t)²` where `t = (v − 30/128) / (40/128)`
  — a quadratic in v with positive second derivative (square of a
  linear ramp 0.25 → 1.0).
- The amp is then quantized to a 0–127 int and passed to
  `AudioSynthWavetable::playNote(note, vel)`.

Note that the pad's input clip points (V_LOW = 30/128, V_HIGH = 70/128)
deliberately mirror the chord-velocity-capture clamp values (30 and 70).
That makes the two stages partially redundant: `ChordVelocityCapture`'s
"any vel < 30 → 30 / any vel > 70 → 70" rule produces bytes the pad
would clip to the same boundary anyway. Audible output is identical;
keep an eye on this if you ever change either threshold.

### Sustain pedal is not implemented
CC 64 is unhandled anywhere in `MIDIController`. The word "sustain" in
the codebase (e.g. `CC_SoundfontSustain`, CC 23) refers to ADSR sustain
*level*, not the damper pedal. When pedal support gets added, the
`ChordVelocityCapture` reset condition needs to become "held count zero
**AND** pedal up" — currently it's just "held count zero," which would
prematurely reset the captured chord velocity when the player lifts all
keys while holding the pedal. Same caveat for any future "all notes
off" detection in the pad voice allocator.

### Per-voice HP cutoff is pinned at noteOn
`hpFilters[i].frequency(noteHz * hpMultiplier)` is called from `noteOn` using
the just-pressed MIDI note. It does NOT retune during a held note. WHITESNAKE
doesn't use pitch bend or modulation, so this is fine — if either gets added
later, every sounding voice's filter would need to retune from the global
bend value (the per-voice MIDI note is already kept in
`voiceStates[i].midiNote`). `setHighpassMultiplier` already does this kind of
fan-out for the CC 27 case.

### Velocity smoother τ is fixed (no CC binding)
`VelocitySmoother` still runs in WHITESNAKE — chord-pinned velocities flow
through the EMA before reaching the pad. But the τ is no longer driven by
a CC; it stays at the constructor default (~2.7 s, matching what CC 26 = 64
used to set). CC 26 is now the velocity floor (formerly on CC 27); CC 27
is the HP cutoff multiplier; CC 28 (PAD bank only) is the HP branch mix.

### `ChordVelocityCapture` is WHITESNAKE-only and gated by mode check
The capture is enabled by `MIDIController::handleNoteOn/Off` checking
`synth.getCurrentMode() == WHITESNAKE`, not by a separate enable flag.
On every `update()` outside WHITESNAKE, `chordCapture.reset()` is called
(cheap when already idle) so re-entering the mode always starts from a
clean state. Median (not mean) is used for the cluster velocity —
rejects an outlier strike from a missed key in a chord.
