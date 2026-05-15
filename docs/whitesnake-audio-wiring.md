# Whitesnake Mode — Audio Signal Path

End-to-end wiring from the loaded SF2 instrument to the I2S/USB outputs.
Read top-to-bottom; every arrow is a live `AudioConnection`. Gain values are
the multiplicative factor applied at each mixer slot.

```
   whitesnake.sf2 (instrument 0, sorted sample zones)
   loaded into AudioSynthWavetable voices[0..7]
                        │
                        │  (8 polyphonic voices, mono)
                        ▼
   ┌────────────────────────────────────────────────────┐
   │  SoundfontPadSynthesizer  (SoundfontPadSynthesizer.cpp)
   │                                                    │
   │   voices[0]──►envelopes[0]──┐                      │
   │   voices[1]──►envelopes[1]──┤                      │
   │   voices[2]──►envelopes[2]──┤──►mixerA──┐          │
   │   voices[3]──►envelopes[3]──┘ gains:    │          │
   │                               [0..3]=   │          │
   │                               volume    │          │
   │                                (unity)  │          │
   │                                         ├──►finalMixer──► getOutput()
   │                                         │  gains:           │
   │                                         │  [0]=1.0          │
   │                                         │  [1]=1.0          │
   │                                         │  [2]=0.0 (unused) │
   │                                         │  [3]=0.0 (unused) │
   │   voices[4]──►envelopes[4]──┐           │                   │
   │   voices[5]──►envelopes[5]──┤──►mixerB──┘                   │
   │   voices[6]──►envelopes[6]──┤ gains:                        │
   │   voices[7]──►envelopes[7]──┘ [0..3]= volume (unity)        │
   │                                                             │
   │   volume defaults to 1.0. The Whitesnake VS samples already │
   │   carry >12 dB of internal headroom, so per-voice unity is  │
   │   safe — pad voices don't sum to clipping in practice.      │
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

- Per-voice mixer gains: `SoundfontPadSynthesizer.cpp:202–211` (`updateMixerGains`)
- L/R fanout and mode mixers: `HybridSynthesizer.h:155–156` (pad to mixerL6/R6 slot 1),
  `:161,165` (mixerL6/R6 to sumL/R slot 3)
- Mode-level gain math: `HybridSynthesizer.h:693–726` (`updateMixerGains`)
- Output connections: `HybridSynthesizer.h:171–180` (peak monitors, I2S, USB)
- Pad peak tap: `HybridSynthesizer.h` (`patchCordWhitesnakePadMonitor`,
  `whitesnakePadPeakMonitor`)
