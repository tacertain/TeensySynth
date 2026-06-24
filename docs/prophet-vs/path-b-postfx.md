# Path B: Free-running post-FX chain on the Whitesnake SF2 pad

Concrete design for adding chorus + slow filter-LFO + reverb downstream of the
existing `SoundfontPadSynthesizer` so that held notes and stacked chords feel
animated rather than locked. Composable with a future path A (vector-envelope
stems): everything in this document sits *downstream* of the pad source, so
swapping a 4-stem vector engine in later replaces only the source, not the FX.

This document is a design-review artifact, not an implementation plan. The
goal is to agree on signal flow, where each block lives in code, CC mapping,
and budget before any edits.

---

## Why path B before path A

Listening to the current WHITESNAKE mode against the 1987 record, the
perceived gap is "patch lacks animation overall" rather than a specific
"chord voices feel locked in lockstep." Both paths address that, but with
different leverage:

- **Path A** (render four oscillator stems from Arturia with the vector
  envelope disabled, runtime XY-mod across them with per-voice independent
  clocks) is the *authentic* mechanism the original VS uses. It cleanly
  solves per-voice independence and runtime-modulated vector motion. But
  it requires an evening of new Arturia rendering + Polyphone work, and
  its primary win is the per-voice lockstep symptom — which is not the
  loudest deficit on this specific record.
- **Path B** (free-running chorus + slow per-voice filter LFO + reverb,
  all downstream of the existing SF2 source) targets the *perceived*
  animation gap directly. Chorus and reverb don't reset on note-on; the
  per-voice filter LFOs have randomized phase so chord voices decorrelate
  anyway. Stays within the existing SF2 and adds only DSP blocks.

Crucially, path B is composable with path A: the post-FX chain sits
downstream of whatever the pad source is. If we later decide path A is
worth the effort, the path-B chain stays exactly as it is and only the
source upstream of `finalMixer` changes. So path B is the "do the cheap
high-leverage thing first, escalate only if needed" move.

---

## Goal

Make WHITESNAKE mode "breathe" without rendering new samples or rewriting the
pad source. Specifically:

1. **Continuous timbre motion** under a held note (slow filter LFO).
2. **Per-voice independence** so simultaneously-struck notes drift through
   that motion at different points (per-voice LFO with randomized phase).
3. **Spatial wash** that doesn't reset on note-on (chorus + reverb).

Constraint: the live performance is **mono out into the FOH board**, so we
do not split L/R or build any stereo width — every block in the chain is
mono in/mono out.

---

## Signal chain (mono, end-to-end)

Per-voice path (repeats NUM_VOICES = 8 times):

```
mainWT -+
        |               LFO[i] (sine, ~0.2 Hz,
subWT --+--> voiceMixer  random phase per voice)
        |                    \
HP   ---+                     v (octave-control mod input)
                         voiceMixer --> LPF[i] --> envelope[i]
```

Voice sum and global post-FX (built once, mono):

```
envelope[0..7] --> mixerA / mixerB --> finalMixer
                                            |
                  +-------------------------+
                  |
                  +--> (dry tap) -----------> wetDryMixer.ch0
                  |
                  +--> AudioEffectFlange A --+
                  |        (rate 0.43 Hz)    |
                  |                          +--> chorusBus --> wetDryMixer.ch1
                  +--> AudioEffectFlange B --+      (Mixer4)              |
                           (rate 0.67 Hz)                                 |
                                                                          v
                                                  AudioEffectFreeverb --> wetDryMixer.ch2
                                                          (fed from chorusBus)
                                                                          |
                                                                          v
                                                                  getOutput()  -->  mixerL6 / mixerR6
```

Two rules across the whole chain:

Two architectural rules that hold across the whole chain:

- **Nothing resets on noteOn except the envelope.** All LFOs and the
  chorus/reverb state are continuously free-running.
- **Per-voice motion is built per-voice, post-mixer motion is built once.**
  The slow filter LFO is per-voice (so chords decorrelate). Chorus and
  reverb sit after the voice sum, because their job is global wash.

---

## Block-by-block design

### B1. Per-voice low-pass filter with free-running LFO

**Position:** between each voice's `voiceMixer` output and its
`AudioEffectEnvelope` input. (User-requested: before the envelope. Means the
modulation amplitude follows the envelope curve — swells in on attack,
fades out on release, instead of being constantly present.)

**Module per voice:**
- `AudioFilterStateVariable lpfVoice[NUM_VOICES]`
  - Use the low-pass output only.
- `AudioSynthWaveform lfoVoice[NUM_VOICES]` (sine, free-running)
  - `frequency(lfoRate)` — global; same rate on all voices.
  - `amplitude(lfoDepth)` — global; same depth on all voices.
  - `phase(random_phase_per_voice)` — **per voice at construction**, so they
    are decorrelated for the life of the synth. Not re-randomized on
    noteOn — the whole point is to be free-running.

**Connections per voice (new patch cords):**
- voiceMixer[i] -> lpfVoice[i].input(0)             (signal)
- lfoVoice[i]   -> lpfVoice[i].input(1)             (frequency modulation, in octaves)
- lpfVoice[i].output(0) -> envelope[i].input        (low-pass tap)

**Existing connection that gets cut:** voiceMixer[i] -> envelope[i] is
replaced by the lpfVoice insertion above. Net: one new AudioStream object
pair per voice, one extra patch cord per voice.

**Filter base cutoff control:** the voice's base cutoff is set on noteOn to
`baseHz * lpfMultiplier`, where `lpfMultiplier` is global and CC-controlled
(see CC table). The LFO then modulates ± `octaveControl()` octaves around
that base. `octaveControl()` sets the mod range in octaves; with the LFO
sine swinging 0..lfoDepth, total swing is `±0.5 * lfoDepth * octaveControl`.

**Defaults:**
- LFO rate: 0.20 Hz (one cycle every 5 sec)
- LFO depth: 0.5 (half amplitude)
- Filter base multiplier: 6.0× note frequency
- octaveControl: 0.6 (±0.18 octaves around base when depth = 0.6)

This gives ~3 dB of brightness motion swinging slowly under the pad. With
random per-voice phase, a 3-note chord has each voice peaking at a
different time.

**Filter key-tracking (added after hardware tuning).** Pure proportional
tracking (`cutoff = noteHz × lpMultiplier`) made notes below middle C
progressively muffled — the absolute cutoff falls with pitch, so a note an
octave down sits at half the cutoff. The fix is a key-track exponent
(`lpKeyTrack`) applied **only below middle C**, pivoting exactly at middle C
so the above-MC sound (which was already right) is untouched:

```
noteHz >= refHz:  cutoff = noteHz * lpMultiplier              # unchanged
noteHz <  refHz:  cutoff = refHz  * lpMultiplier * (noteHz/refHz)^lpKeyTrack
```

`lpKeyTrack = 1.0` reproduces the original full-tracking behavior;
`0.0` makes the cutoff flat below middle C (every bass note gets middle C's
cutoff, ~1988 Hz at the 7.6× default). Locked in at **0.0** on hardware —
the flat floor gave the best low-end body without brightening the top.
It is a build-time constant (`SoundfontPadSynthesizer` constructor default),
not CC-bound; see the note under the CC table.

### B2. Global ensemble chorus (two flanges)

**Why not `AudioEffectChorus`:** the Teensy library's `AudioEffectChorus`
is misnamed. Its source (verified) implements a multi-tap fixed-delay
average with no LFO, no modulation, no time variation. It produces fixed
comb-filter notches, not chorus motion. We use two `AudioEffectFlange`
instances instead — `AudioEffectFlange` is the actual LFO-modulated
delay-line algorithm, and with chorus-range parameters (10–30 ms delay,
2–5 ms depth, 0.3–1.5 Hz rate) it produces real chorus.

**Position:** two parallel flange instances both fed from `finalMixer`,
summed into a `chorusBus` mixer, then routed both to `wetDryMixer.ch1`
(chorus wet tap) and into the reverb's input.

**Modules:**
- `AudioEffectFlange chorusA;`
- `AudioEffectFlange chorusB;`
- `AudioMixer4 chorusBus;` (ch0 = flange A, ch1 = flange B, ch2/ch3 unused)
- Two delay buffers: `static short chorusBufA[CHORUS_BUF_LEN];` and
  `static short chorusBufB[CHORUS_BUF_LEN];`
- Length: 2048 samples each (~46 ms at 44.1 kHz). Comfortable headroom
  above a 30 ms max delay offset. **Memory: ~8 KB main RAM total.**

**Settings (initial defaults; CCs override):**
- `chorusA.begin(chorusBufA, CHORUS_BUF_LEN, 661, 132, 0.43f);`
  (offset ≈ 15 ms, depth ≈ 3 ms, rate 0.43 Hz)
- `chorusB.begin(chorusBufB, CHORUS_BUF_LEN, 882, 176, 0.67f);`
  (offset ≈ 20 ms, depth ≈ 4 ms, rate 0.67 Hz)
- `chorusBus.gain(0, 0.5f); chorusBus.gain(1, 0.5f);` (sum at -6 dB each
  to leave 6 dB headroom for the wet/dry mix; final wet level set by
  `wetDryMixer.ch1`)

Both flange LFOs are free-running with independent phase counters. They
never reset on note-on; they never interact with each other. The slow rate
difference (0.43 vs 0.67 Hz, ratio ≈ 1.56) keeps them from drifting back
into phase too quickly, which preserves "ensemble" feel rather than
"vibrato."

**CC story:**
- CC 45 (chorus mix) drives `wetDryMixer.ch1` gain — true wet/dry of the
  chorus bus into the final output.
- CC 46 (chorus depth) drives `delay_depth` on **both** flanges via their
  `voices()` re-init, scaled around the defaults above. Lower depth = more
  subtle wobble; higher depth = pronounced chorus.

Rates stay fixed (the 0.43/0.67 Hz pair is the ensemble character). A
future CC could expose the rate pair as a global "speed" multiplier if
desired — not in v1.

### B3. Stereo dry/wet mixer (single-channel use)

**Position:** after chorus, replacing the current `finalMixer -> getOutput()`
exit point.

**Module:**
- `AudioMixer4 wetDryMixer;`
  - ch0: dry tap from finalMixer
  - ch1: chorus tap (post-chorus)
  - ch2: reverb tap (post-reverb, which is fed from the chorus tap)
  - ch3: unused

**CC-controlled gains:**
- dry gain: 1.0 (fixed, or CC-controlled if we want a "color amount" knob)
- chorus mix: 0..1 (CC)
- reverb mix: 0..1 (CC)

`getOutput()` is changed to return `&wetDryMixer` instead of `&finalMixer`.

### B4. Reverb

**Position:** fed from the chorus output (so reverb sees the chorused signal,
which is denser and more pleasing than reverberating the dry pad).

**Module choice:** `AudioEffectFreeverb` — Schroeder-Moorer style, classic
"hall/plate" wash. The simpler `AudioEffectReverb` is cheaper but rougher.
For a pad context Freeverb is the right tradeoff.

**Memory:** ~9 KB main RAM internal buffers.

**CPU:** ~25–30% on Teensy 4.1 at default block size. Significant. We
should verify headroom before committing — see budget section.

**Settings:**
- `roomsize(0.7)` — big-ish room, on the wash side of hall.
- `damping(0.5)` — moderate hi-end decay so it doesn't get fizzy.
- Reverb wet level is controlled by ch2 of wetDryMixer above, not by the
  reverb's own internal output level.

---

## CC mapping (bank 4, CCs 41-48 in WHITESNAKE mode)

New table `channel1Bank_41_50_PAD` parallel to the existing
`channel1Bank_41_50` (string-pad controls) and `channel1Bank_21_30_PAD`.
The `WHITESNAKE` row in `modeBankConfigs[]` is updated to point its bank-4
columns at this new table; no per-mode-handler edits are needed (see
"Where each piece of code lives" below).

> **Implementation note (as shipped).** The table below reflects the
> mapping actually wired in `channel1Bank_41_50_PAD[]`
> (`MIDIControlCallbacks.cpp`). It differs from this document's original
> proposal: the final implementation put **cutoff on CC 41 and resonance on
> CC 42** (low CC = baseline/amount), pushing **LFO depth/rate to CC 43/44**
> (high CC = modulation/character), to keep the pair structure consistent
> with the other banks. Defaults below are the shipped values from the
> `SoundfontPadSynthesizer` constructor, not the placeholder defaults in the
> original draft. The musician-facing `docs/keyboard-controls.md` matches
> this table; treat that and the code as the source of truth.

| CC | Callback | Parameter | Range | Default | Notes |
|----|----------|-----------|-------|---------|-------|
| 41 | `CC_WhitesnakeFilterCutoff`    | base filter multiplier      | 1.0–20.0× note (exp)      | 7.6× (CC 41 = 86) | The base cutoff that the LFO modulates around. Clamped to 8 kHz internally for SVF stability. Key-tracked above middle C only; flat below it (see "Filter key-tracking" above). `lpKeyTrack` is a build-time constant (0.0), not a CC. |
| 42 | `CC_WhitesnakeFilterResonance` | filter Q                    | 0.7–4.0 (linear)          | 1.12 (CC 42 = 16) | Watch for self-oscillation above ~3.5. |
| 43 | `CC_WhitesnakeFilterLfoDepth`  | per-voice LFO amplitude     | 0.0–1.0 (linear)          | 0.5     | 0 = static filter, 1 = full ± octaveControl swing. |
| 44 | `CC_WhitesnakeFilterLfoRate`   | per-voice LFO rate          | 0.05–1.5 Hz (exponential) | 0.20 Hz | Global rate, applied to all voice LFOs. Phase stays randomized. |
| 45 | `CC_WhitesnakeChorusMix`       | chorus wet level            | 0.0–1.0 (linear)          | 0.433 (CC 45 = 55) | Wet/dry blend of chorus bus into output (`wetDryMixer.ch1` gain). |
| 46 | `CC_WhitesnakeChorusDepth`     | chorus modulation depth     | 0.0–1.0 fraction (~0–6 ms) | 0.567 (CC 46 = 72) | Maps to `delay_depth` on both flanges via `voices()` re-init. Scales around the defaults (132 / 176 samples per flange at fraction 0.5). |
| 47 | `CC_WhitesnakeReverbMix`       | reverb wet level            | 0.0–1.0 (linear)          | 0.591 (CC 47 = 75) | Wet/dry blend of reverb tap into output. |
| 48 | `CC_WhitesnakeReverbSize`      | room size                   | 0.0–1.0 (linear)          | 0.591 (CC 48 = 75) | Maps to Freeverb's `roomsize()`. |

CC 41-48 retain their non-WHITESNAKE meanings in bank 4 (string-pad controls)
when the active mode is not WHITESNAKE — the bank install switch handles
that, mirroring the existing bank-2 pattern.

---

## Where each piece of code lives

### `SoundfontPadSynthesizer.{h,cpp}` (modified)

- Add per-voice members:
  - `AudioFilterStateVariable lpfVoice[NUM_VOICES];`
  - `AudioSynthWaveform lfoVoice[NUM_VOICES];`
  - `AudioConnection* voiceMixerToLpf[NUM_VOICES];`
  - `AudioConnection* lfoToLpf[NUM_VOICES];`
  - `AudioConnection* lpfToEnvelope[NUM_VOICES];`
- Remove the existing `voiceMixerToEnvelope[NUM_VOICES]` cords (replaced by
  the LPF insertion above).
- Add post-mix members:
  - `AudioEffectFlange chorusA, chorusB;`
  - `AudioMixer4 chorusBus;`
  - `AudioEffectFreeverb reverbPost;`
  - `AudioMixer4 wetDryMixer;`
  - Static delay buffers: `static short chorusBufA[CHORUS_BUF_LEN];` and
    `static short chorusBufB[CHORUS_BUF_LEN];` (2048 samples each).
  - Patch cords: `finalMixer -> wetDryMixer.0` (dry), `finalMixer -> chorusA`,
    `finalMixer -> chorusB`, `chorusA -> chorusBus.0`, `chorusB -> chorusBus.1`,
    `chorusBus -> wetDryMixer.1` (chorus wet), `chorusBus -> reverbPost`,
    `reverbPost -> wetDryMixer.2` (reverb wet).
- Change `getOutput()` to return `&wetDryMixer`.
- Constructor: call `chorusA.begin(...)` and `chorusB.begin(...)` with the
  defaults above, set LFO rates/depths/phases on per-voice LFOs, set filter
  defaults, set reverb defaults.
- New public API:
  - `void setFilterLfoRate(float hz);`
  - `void setFilterLfoDepth(float depth);`
  - `void setFilterCutoffMultiplier(float m);` — applied per voice on noteOn,
    plus re-pushed to sounding voices.
  - `void setFilterResonance(float q);`
  - `void setChorusMix(float m);`
  - `void setReverbMix(float m);`
  - `void setReverbRoomSize(float s);`
- `noteOn`: in addition to existing wavetable setup, set `lpfVoice[i].frequency(noteHz * lpfMultiplier)` and `lpfVoice[i].resonance(q)`.
- Constructor randomizes initial LFO phase per voice: `lfoVoice[i].phase((float)random(360));`. Each voice's phase is fixed for the life of the synth; LFOs run free thereafter.

### `HybridSynthesizer.h` (small change)

- No graph changes — the new wet/dry mixer is internal to
  `SoundfontPadSynthesizer`, so `whitesnakePadPatchCordL/R` are unchanged.
- New forwarder calls in `loadWhitesnakeInstruments()` to set defaults on
  the new parameters.

### `MIDIControlCallbacks.{h,cpp}` (extended)

The per-mode bank install machinery was refactored to a table-driven
`modeBankConfigs[]` + `installBanksForMode()` (see commit `87c1e71`), so
wiring a new PAD-only bank-4 table is purely additive — no edits to any
mode handler, no new install helpers.

- 8 new callback functions matching the CC table above.
- New `channel1Bank_41_50_PAD[]` table (parallel to the existing
  `channel1Bank_41_50` string-pad table).
- In `modeBankConfigs[]`, change the `WHITESNAKE` row's `bank4` and
  `bank4_count` columns from `channel1Bank_41_50` /
  `channel1Bank_41_50_count` to `channel1Bank_41_50_PAD` /
  `channel1Bank_41_50_PAD_count`. Every other row stays on the string-pad
  table. `installBanksForMode()` re-installs bank 4 from the table on every
  mode switch, so leaving WHITESNAKE automatically reverts bank 4 to the
  string-pad table — no per-handler edits needed.

### `MIDIController.cpp` (unchanged)

The bank installer machinery already supports re-installing tables
per-bank. Initial bank registrations in the constructor stay as-is.

---

## Memory and CPU budget

### Memory (main RAM, not PSRAM)

| Item | Size |
|------|------|
| Per-voice SVF objects (8) | ~1 KB total |
| Per-voice LFO objects (8) | ~1 KB total |
| Chorus delay buffers (2 × 2048 samples) | ~8 KB |
| Freeverb internal buffers | ~9 KB |
| New patch cords (~36) | trivial |
| **Total new main-RAM cost** | **~19 KB** |

Teensy 4.1 has 512 KB of OCRAM2 for audio buffers (and 512 KB of DTCM total).
~19 KB is comfortably absorbable. **PSRAM (the 8 MB chip) is untouched by
this path — all sample data is unchanged.**

### CPU

Rough per-block percentages at 44.1 kHz, 128-sample blocks, Teensy 4.1:

| Item | Estimate |
|------|----------|
| Per-voice SVF (8 instances) | ~8% |
| Per-voice LFO sines (8 instances) | ~2% |
| Two flange chorus instances | ~4-5% |
| Freeverb | ~25-30% |
| New mixers / patch cords | ~1% |
| **Total added CPU** | **~40-45%** |

**Measured baseline (chord-heavy play on current WHITESNAKE): peak 16.4%.**
Adding ~40% lands us at ~57% peak — comfortable margin to ~80% safe
ceiling. Freeverb stays in; no need to fall back to AudioEffectReverb or
trim the per-voice filter LFO.

---

## What's intentionally *not* in this design

- **Stereo width / dual chorus per channel.** Excluded by the mono-out
  constraint. If we ever want stereo for headphone monitoring, two chorus
  instances with slightly different rates feeding L and R is the natural
  extension. Not now.
- **Per-voice detune.** Could fatten the chord further. Held off because
  the existing main+sub octave structure already provides some thickness,
  and detune adds tuning instability that interacts oddly with the LFO.
- **Pre-FX EQ.** No high-shelf or low-shelf tilt before the chorus. If the
  result sounds too dark or too bright, we tilt the filter base multiplier
  rather than adding an EQ stage.
- **Reverb pre-delay.** Freeverb doesn't expose one. If a sense of "room
  size" via pre-delay matters, we'd need to add a `AudioEffectDelay` block
  before the reverb. Defer.
- **Modulating the chorus rate from a slow LFO.** Adds movement on movement.
  Tempting but easy to overshoot. `AudioEffectFlange` doesn't expose its
  delay rate as a modulation input either; we'd need to call its `voices()`
  re-init periodically. Defer.
- **CC for the per-voice LFO depth ON FILTER (`octaveControl`)** vs LFO
  amplitude (`amplitude()`). We expose only LFO amplitude (CC 43). The
  octaveControl is a build-time constant (0.6) — keeps the design simple
  and the depth knob musically intuitive. Could add a CC later if needed.

---

## Resolved decisions

1. ~~**Freeverb vs AudioEffectReverb.**~~ Resolved: measured baseline is
   16.4%, so Freeverb fits comfortably.
2. ~~**`AudioEffectChorus` depth control.**~~ Resolved: switched to two
   `AudioEffectFlange` instances because `AudioEffectChorus` does no LFO
   modulation. CC 46 maps to `delay_depth` on both flanges via `voices()`
   re-init. See B2.
3. ~~**Bank 4 install on non-WHITESNAKE modes.**~~ Resolved by the
   table-driven refactor in commit `87c1e71`: `modeBankConfigs[]` now
   centralises the per-mode bank-2 and bank-4 table assignments, and
   `installBanksForMode()` (called from every mode handler) re-installs
   both on every mode switch. Adding a WHITESNAKE-specific bank-4 table
   is now a one-cell edit in that table, with no mode-handler changes.
4. ~~**LFO phase distribution.**~~ Resolved: use `random(360)` at
   construction time for each voice's initial LFO phase. Decorrelated
   forever, no audible difference from golden-angle distribution.
5. ~~**LFO+filter on released voices.**~~ Resolved: continues to run on
   released voices. Release tail breathes with the same modulation as the
   sustained portion. Intentional.

---

## Testing plan

1. Build the chain with all wet/dry mixes at 0 (everything bypassed). Sanity
   check that the sound is identical to current WHITESNAKE.
2. Bring up CC 43/44 (filter LFO depth/rate) only. Verify per-voice
   independence by playing a 3-note chord and listening for the brightness
   to peak at different times on the three voices.
3. Add CC 45 chorus mix. Verify the wash is mono and never resets on
   note-on (start the chorus, hold a note, repeatedly retrigger — the
   chorus modulation should keep moving independently).
4. Add CC 47/48 reverb. Verify the tail makes sense — long enough to wash,
   short enough to not muddy successive chords.
5. CPU budget: with the worst-case 8-voice chord held + reverb tail
   active, verify peak CPU < 80%.

---

## Honest cost estimate

- Design doc (this): done.
- New `SoundfontPadSynthesizer` infrastructure (per-voice LPF/LFO, post-FX
  chain, public setters): ~half a day.
- New CC callbacks and bank-4 install: ~1 hour.
- Tuning the defaults on real hardware: 1–2 evenings.

The tuning evenings are the load-bearing part. The DSP wiring is
mechanical; the musical question of "what does this pad *want* in terms of
LFO rate, chorus mix, reverb size" only gets answered on the keyboard.

---

## Composability with future path A

If we later render the four VS oscillator stems and replace the current
single-SF2 source with a 4-stem vector engine, the only change to this
path B chain is: each voice has 4 wavetable+envelope chains instead of 1
main+1 sub. The post-FX chain (chorus, wet/dry, reverb) is unchanged. The
per-voice LFO either:
- stays on a single LPF per voice (one filter after the 4-stem mix), or
- becomes 4 LPFs per voice (one per stem, all driven by the same LFO).

The first option is simpler and probably indistinguishable. Either way,
nothing in path B precludes path A.
