# TeensySynth Audio Wiring Diagram

This document describes the complete audio signal flow from synthesis sources through mixers to the final output, including all gain stages and potential overflow points.

## Overview

The TeensySynth uses a hierarchical mixing architecture with 4 independent synthesis engines that are combined in the final output stage. The audio flows from sources → per-source mixers → sum mixers → peak monitors → I2S output (and optionally USB output).

## Signal Flow Architecture

```
┌───────────────────────────────────────────────────────────────────────────┐
│                         HYBRID SYNTHESIZER                                │
│                                                                           │
│  ┌──────────────────┐      ┌──────────────────┐                           │
│  │ Strings          │      │ Drone            │                           │
│  │ (8 voices)       │      │ (1 voice)        │                           │
│  │ [0-3] → mixerL1  │      │ osc1(0.4) ┐      │                           │
│  │ [4-7] → mixerL2  │      │ osc2(0.3) ├→mixer│                           │
│  │    ↓   ↓         │      │ subOsc(0.2)┘     │                           │
│  │   sumL/sumR      │      │    ↓  filter     │                           │
│  │    (vol*gain)    │      │    ↓  envAmp     │                           │
│  │    ↓      ↓      │      │    ↓   ↓         │                           │
│  └────┼──────┼──────┘      └────┼───┼─────────┘                           │
│       │      │                  │   │                                     │
│       │      │             ┌────┘   └────┐                                │
│       │      │             ↓             ↓                                │
│       │      │        leftAmp(vol)   rightAmp(vol)                        │
│       │      │             ↓             ↓                                │
│       │      │         mixerL4[0]    mixerR4[0]                           │
│       │      │          (droneVol)    (droneVol)                          │
│       │      │             ↓             ↓                                │
│       ↓      ↓             ↓             ↓                                │
│    sumL[0]  sumR[0]    sumL[1]        sumR[1]                             │
│     (1.0)    (1.0)      (1.0)          (1.0)                              │
│       │       │           │              │                                │
│       │       │           │              │                                │
│  ┌────┴───────┴───────────┴──────────────┴─────────────┐                  │
│  │                                                     │                  │
│  │  ┌──────────────────┐      ┌──────────────────┐     │                  │
│  │  │ StringPad        │      │ Soundfont        │     │                  │
│  │  │ (6 voices)       │      │ (4 instruments)  │     │                  │
│  │  │ Each voice:      │      │ Each instrument: │     │                  │
│  │  │  osc1(0.35) ┐    │      │  4 voices→mixer  │     │                  │
│  │  │  osc2(0.33) ├mix │      │   (0.25 each)    │     │                  │
│  │  │  osc3(0.32)─┘    │      │    ↓             │     │                  │
│  │  │   ↓ lowpass      │      │  finalMixer      │     │                  │
│  │  │   ↓ highpass     │      │ (1.0/loadedCnt)  │     │                  │
│  │  │   ↓ envAmp       │           │                   │                  │
│  │  │ [0-3]→voiceMixL1 │           │                   │                  │
│  │  │ [4-5]→voiceMixL2 │           │                   │                  │
│  │  │  (mono→stereo)   │           │                   │                  │
│  │  │    ↓      ↓      │           │                   │                  │
│  │  └────┼──────┼──────┘           │                   │                  │
│  │       │      │                  │                   │                  │
│  │       ↓      ↓                  ↓                   │                  │
│  │   mixerL5[0] mixerR5[0]    mixerL6[0]  mixerR6[0]   │                  │
│  │   (padVol)   (padVol)      (sfVol)     (sfVol)      │                  │
│  │       ↓         ↓              ↓           ↓        │                  │
│  │   sumL[2]   sumR[2]        sumL[3]     sumR[3]      │                  │
│  │    (1.0)     (1.0)          (1.0)       (1.0)       │                  │
│  └───────┼─────────┼──────────────┼───────────┼────────┘                  │
│          │         │              │           │                           │
│          └─────────┴──────────────┴───────────┘                           │
│                    │              │                                       │
│               ┌────▼──────┐  ┌───▼───────┐                                │
│               │   sumL    │  │   sumR    │                                │
│               │ (4 inputs)│  │(4 inputs) │                                │
│               └─────┬─────┘  └─────┬─────┘                                │
│                     │              │                                      │
│              ┌──────┴──────┬───────┴──────┐                               │
│              ↓             ↓              ↓                               │
│        peakMonitorL   AudioOutputI2S  peakMonitorR                        │
│                          (DAC)                                            │
│                            │                                              │
│                    ┌───────┴────────┐                                     │
│                    ↓                ↓                                     │
│             (if USB_AUDIO)   AudioOutputUSB                               │
│                                                                           │
└───────────────────────────────────────────────────────────────────────────┘
```

## Detailed Gain Stages

### 1. Strings Synthesizer (Karplus-Strong)
**Path:** `voices[8] → mixerL1/mixerL2 → sumL/sumR → HybridSynth.sumL/sumR`

**Gain stages:**
- `mixerL1.gain[i]` = `volume` (set in updateMixerGains)
- `mixerR1.gain[i]` = `volume` (set in updateMixerGains)
- `mixerL2.gain[i]` = `volume` (set in updateMixerGains)
- `mixerR2.gain[i]` = `volume` (set in updateMixerGains)
- `HybridSynth.sumL.gain[0]` = `1.0`
- `HybridSynth.sumR.gain[0]` = `1.0`

**Effective gain:** `masterVolume * stringVolume`

**⚠️ OVERFLOW RISK:** If all 8 voices play simultaneously at full velocity with `volume=1.0`, the mixer output could overflow. Each mixer combines 4 voices, so the theoretical maximum is 4x the signal level.

---

### 2. Drone Synthesizer
**Path:** `osc1/osc2/subOsc → oscMixer → filter → envAmp → leftAmp/rightAmp → mixerL4/mixerR4 → HybridSynth.sumL/sumR`

**Gain stages:**
- `oscMixer.gain[0]` = `0.4` (osc1 - sawtooth)
- `oscMixer.gain[1]` = `0.3` (osc2 - pulse)
- `oscMixer.gain[2]` = `0.2` (subOsc - square)
- `envAmp.gain` = `velocity * 0.8` (during attack)
- `leftAmp.gain` = `masterVolume`
- `rightAmp.gain` = `masterVolume`
- `mixerL4.gain[0]` = `masterVolume * droneVolume`
- `mixerR4.gain[0]` = `masterVolume * droneVolume`
- `HybridSynth.sumL.gain[1]` = `1.0`
- `HybridSynth.sumR.gain[1]` = `1.0`

**Effective gain:** `(0.4 + 0.3 + 0.2) * velocity * 0.8 * masterVolume * masterVolume * droneVolume * 1.0`
                    = `0.72 * velocity * masterVolume² * droneVolume`

**⚠️ OVERFLOW RISK:** The oscMixer sums to 0.9, which is safe. However, the double application of `masterVolume` (once in leftAmp/rightAmp, once in mixerL4/mixerR4) means the effective gain includes `masterVolume²`, which could reach high levels.

**⚠️ DESIGN ISSUE:** The drone volume is applied through both the amp stages AND the mixer stages, resulting in `masterVolume` being applied twice!

---

### 3. String Pad Synthesizer
**Path:** `voices[6] → each voice has 3 oscs → ensembleMixer → lowpass → highpass → envAmp → voiceMixers → mixerL5/mixerR5 → HybridSynth.sumL/sumR`

**Per-voice gain stages:**
- `ensembleMixer.gain[0]` = `0.35` (osc1 - center)
- `ensembleMixer.gain[1]` = `0.33` (osc2 - sharp)
- `ensembleMixer.gain[2]` = `0.32` (osc3 - flat)
- `ensembleMixer.gain[3]` = `0.0` (unused)
- `envAmp.gain` = dynamically controlled by envelope
- Voice mixers: Not explicitly set (default 1.0?)

**System-level gain stages:**
- `mixerL5.gain[0]` = `masterVolume * stringPadVolume`
- `mixerR5.gain[0]` = `masterVolume * stringPadVolume`
- `HybridSynth.sumL.gain[2]` = `1.0`
- `HybridSynth.sumR.gain[2]` = `1.0`

**Effective per-voice gain:** `1.0 * masterVolume * stringPadVolume` (ensemble mixer sums to ~1.0)

**⚠️ OVERFLOW RISK:** With 6 voices active, if all are playing at full envelope level, the combined signal from voice mixers could be 6x the single voice level before being attenuated by the master/stringPad volume controls. This is a significant overflow risk!

---

### 4. Soundfont Synthesizer
**Path:** `instruments[4] → each: voices[4] → mixer → finalMixer → mixerL6/mixerR6 → HybridSynth.sumL/sumR`

**Gain stages:**

Per-instrument (each of 4 instruments):
- `instrument.mixer.gain[i]` = `volume * 0.25` (for i=0..3, set in SoundfontInstrument::updateMixerGains)
  - Each instrument combines 4 voices with 0.25 gain to prevent overflow
  
Final mixer (combines all instruments):
- `finalMixer.gain[i]` = `1.0 / loadedCount` (dynamic, for each loaded instrument)
  - Automatically adjusts based on how many instruments are loaded
  - Example: 1 instrument = 1.0 gain, 4 instruments = 0.25 gain each
  
Output stage:
- `mixerL6.gain[0]` = `masterVolume * soundfontVolume`
- `mixerR6.gain[0]` = `masterVolume * soundfontVolume`
- `HybridSynth.sumL.gain[3]` = `1.0`
- `HybridSynth.sumR.gain[3]` = `1.0`

**Effective gain per voice:** `volume * 0.25 * (1.0 / loadedCount) * masterVolume * soundfontVolume`

**✅ OVERFLOW PROTECTION:** 
- Each instrument mixer uses 0.25 gain per voice (4 voices × 0.25 = 1.0 max output)
- Final mixer dynamically adjusts gain based on loaded instrument count
- With all 4 instruments loaded and all 16 voices active: 4 instruments × 1.0 × 0.25 = 1.0 max
- This architecture prevents overflow even with all voices at maximum volume

---

## Final Sum Mixers (sumL and sumR)

**Path:** `sumL/sumR → peakMonitor + AudioOutputI2S + AudioOutputUSB`

All four synthesis sources feed into the sum mixers with gain `1.0`:
- `sumL.gain[0]` = `1.0` (strings)
- `sumL.gain[1]` = `1.0` (drone)
- `sumL.gain[2]` = `1.0` (string pads)
- `sumL.gain[3]` = `1.0` (soundfont)
- `sumR.gain[0]` = `1.0` (strings)
- `sumR.gain[1]` = `1.0` (drone)
- `sumR.gain[2]` = `1.0` (string pads)  
- `sumR.gain[3]` = `1.0` (soundfont)

**⚠️⚠️⚠️ CRITICAL OVERFLOW RISK:** This is the most critical overflow point! If multiple synthesis engines are active simultaneously at high volume levels, their signals add directly in the sum mixer. Worst case:
- All 8 string voices + 1 drone + 6 string pad voices + 8 soundfont voices
- With no attenuation at the sum stage (all gains = 1.0)
- Could easily exceed digital maximum and cause clipping

---

## Overflow Analysis Summary

### High Risk Points:

1. **String Pad voice mixers** - 6 voices with no attenuation before mixerL5/mixerR5
2. **sumL/sumR mixers** - All 4 synthesis sources sum at 1.0 gain
3. **Drone double-gain** - masterVolume applied twice (in amps and mixers)
4. **✅ Soundfont (FIXED)** - Now uses per-instrument mixers with dynamic gain adjustment

### Recommended Fixes:

1. **Add attenuation to voice combiners:**
   - StringPad: Set voice mixer gains to ~0.3 (1.8 total for 6 voices instead of 6.0)
   - ✅ **Soundfont (COMPLETED)**: Now uses 0.25 per voice + dynamic gain (1.0/loadedCount)
   - Strings: Set mixer gains to 0.5 (total 2.0 for 4 voices instead of 4.0)

2. **Fix drone double-gain issue:**
   - Remove masterVolume from leftAmp/rightAmp (set to 1.0)
   - Keep only the mixer stage for volume control

3. **Add attenuation to sum mixers:**
   - Set sumL/sumR gains to 0.25 for all channels
   - Or implement dynamic gain based on which modes are active

4. **Monitor peak levels:**
   - The peakMonitorL/peakMonitorR are already in place
   - Add telemetry to display peak levels and warn of clipping

## Audio Output Specifications

### I2S Output (AudioOutputI2S)
- DAC: Teensy's built-in I2S DAC
- Bit depth: 16-bit signed integer
- Range: -32768 to +32767
- Sample rate: 44.1 kHz (typically)

### USB Output (AudioOutputUSB)
- Only active if `USB_AUDIO` is defined
- Same bit depth and sample rate as I2S
- Uses same signal as I2S (tapped from sumL/sumR)

---

## Volume Control Variables

| Variable | Type | Range | Applied At | Notes |
|----------|------|-------|------------|-------|
| `masterVolume` | float | 0.0-1.0 | Multiple stages | Applied to all sources |
| `stringVolume` | float | 0.0-1.0 | StringsSynth | Source-specific |
| `droneVolume` | float | 0.0-1.0 | Drone mixers | ⚠️ Applied with masterVolume² |
| `stringPadVolume` | float | 0.0-1.0 | StringPad mixers | Source-specific |
| `soundfontVolume` | float | 0.0-1.0 | Soundfont mixers | Source-specific |

**Note:** CC7 (Master Volume) typically sets `masterVolume` to `value/64.0`, which can exceed 1.0 (max ~1.98). This is another overflow risk!
