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
   ┌─────────────────────────────────────────────────────────────┐
   │  SoundfontPadSynthesizer  (SoundfontPadSynthesizer.cpp)     │
   │                                                             │
   │   Per voice (i = 0..7) — each is its own sub-osc cell:      │
   │                                                             │
   │       mainVoices[i]──►┐                                     │
   │         plays note N   ├ voiceMixers[i] ── envelopes[i] ──► │
   │                        │   gain(0)=1.0                      │
   │       subVoices[i] ──►┘   gain(1)=octaveMix                 │
   │         plays note N−12      (default 1/3, CC 25 live)      │
   │         (skipped if N<12)                                   │
   │                                                             │
   │   Voices 0–3 sum into mixerA, 4–7 into mixerB, both into    │
   │   finalMixer; per-voice slot gains on mixerA/B = volume     │
   │   (defaults to 1.0). finalMixer slots [0]=[1]=1.0.          │
   │                                                             │
   │       v0──►envA[0]──┐                                       │
   │       v1──►envA[1]──┤──►mixerA──┐                           │
   │       v2──►envA[2]──┤            │                          │
   │       v3──►envA[3]──┘            ├──►finalMixer──► getOutput()
   │       v4──►envA[4]──┐            │                          │
   │       v5──►envA[5]──┤──►mixerB──┘                           │
   │       v6──►envA[6]──┤                                       │
   │       v7──►envA[7]──┘                                       │
   │                                                             │
   │   `volume` defaults to 1.0. The Whitesnake VS samples carry │
   │   >12 dB of internal headroom, so per-voice unity is safe — │
   │   pad voices don't sum to clipping in practice. Sub-osc at  │
   │   mix=1.0 adds up to ~6 dB on top of main; still inside     │
   │   the headroom envelope.                                    │
   └─────────────────────────────────────────────────────────────┘
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

    voice → env → mixerA/B (×volume) → finalMixer (×1.0)
         → mixerL6/R6 (×masterVolume × soundfontVolume)
         → sumL/R (×1.0) → DAC

At default `volume=1.0`, `masterVolume=1.0`, `soundfontVolume=1.0` the
effective gain per voice into the DAC is **1.0** (unity). The pad relies on
the >12 dB of internal headroom in the source samples to prevent the eight-
voice sum from clipping; ADSR envelopes and the asymmetric VS waveforms make
in-phase peak summing rare in practice. If you ever do see clipping in the
`whitesnakePadPeakMonitor`, drop `volume` (or master/mode gains) rather than
re-introducing a per-voice attenuation.

## Code references

- Per-voice sub-osc graph: `SoundfontPadSynthesizer.cpp` constructor
  (`mainToVoiceMixer`/`subToVoiceMixer`/`voiceMixerToEnvelope`)
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

### Velocity shaping lives in two layers, on purpose
1. **Keyboard-correction layer** — `MIDIController::midiVelocityToFloat`,
   piecewise: MIDI≤20 → 0.2, MIDI≥70 → 1.0, linear between. Compensates
   for the Launchkey's poor native curve. Applies globally to all modes.
2. **Musical-dynamics layer** — `SoundfontPadSynthesizer::noteOn` does a
   quadratic remap `119.0625·v² + 7.9375` (endpoints 0.2→12.7, 1.0→127).
   Pad-specific; gives soft strikes meaningfully quieter than mediums.

They look like duplication if you only see one of them. Don't collapse
without touching both.

### Sustain pedal is not implemented
CC 64 is unhandled anywhere in `MIDIController`. The word "sustain" in
the codebase (e.g. `CC_SoundfontSustain`, CC 23) refers to ADSR sustain
*level*, not the damper pedal. When pedal support gets added, the
`ChordVelocityCapture` reset condition needs to become "held count zero
**AND** pedal up" — currently it's just "held count zero," which would
prematurely reset the captured chord velocity when the player lifts all
keys while holding the pedal. Same caveat for any future "all notes
off" detection in the pad voice allocator.

### `ChordVelocityCapture` is WHITESNAKE-only and gated by mode check
The capture is enabled by `MIDIController::handleNoteOn/Off` checking
`synth.getCurrentMode() == WHITESNAKE`, not by a separate enable flag.
On every `update()` outside WHITESNAKE, `chordCapture.reset()` is called
(cheap when already idle) so re-entering the mode always starts from a
clean state. Median (not mean) is used for the cluster velocity —
rejects an outlier strike from a missed key in a chord.
