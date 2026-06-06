# Path B tuning guide — WHITESNAKE pad FX exploration

Practical sequence for dialing in the path-B FX chain (per-voice LP filter +
LFO, chorus, reverb) on the WHITESNAKE pad. Bank 4 CCs 41–48.

These knobs interact, so the order you tweak matters: timbre first, then
motion, then ensemble, then space.

---

## CC map (bank 4 in WHITESNAKE mode)

Pair structure: low CC = baseline/amount, high CC = modulation/character.

| CC | Parameter            | Range                       | Default     |
|----|----------------------|-----------------------------|-------------|
| 41 | LP base cutoff       | 1×–20× note Hz (exp), clamped to 8 kHz max | 7.6× (CC 86) |
| 42 | LP resonance (Q)     | 0.7–4.0 (linear)            | 1.12 (CC 16) |
| 43 | LP LFO depth         | 0.0–1.0 (linear)            | 0.5         |
| 44 | LP LFO rate          | 0.05–1.5 Hz (exp)           | 0.20 Hz     |
| 45 | Chorus wet mix       | 0.0–1.0 (linear)            | 0.433 (CC 55) |
| 46 | Chorus depth         | 0.0–1.0 fraction (linear)   | 0.567 (CC 72) |
| 47 | Reverb wet mix       | 0.0–1.0 (linear)            | 0.591 (CC 75) |
| 48 | Reverb roomsize      | 0.0–1.0 (linear)            | 0.591 (CC 75) |

---

## What you can and can't bypass

- The **LP filter is always inline** between voiceMixer and envelope. CC 41
  changes its cutoff; CC 43 controls whether the cutoff moves. Even with
  CC 43 = 0, the filter is doing its low-pass thing at whatever static cutoff
  CC 41 has set. There is no "filter off" position.
- Cutoff is clamped to **8 kHz** internally to keep the Chamberlin SVF stable
  (above ~12 kHz it rings up into noise). At CC 41 high settings, low/mid
  notes hit this clamp; the filter still attenuates above 8 kHz with a gentle
  12 dB/octave slope.
- **Chorus and reverb can be muted** via CC 45 = 0 and CC 47 = 0. Their
  underlying blocks are still running — only their wet level is zero.

So the closest thing to a "no path B" baseline is:

| CC | Setting                                                    |
|----|------------------------------------------------------------|
| 41 | 127 (cutoff as open as it can go — 8 kHz clamp applies)    |
| 42 | 0 (lowest Q, ~0.7)                                         |
| 43 | 0 (filter static, LFO inaudible)                           |
| 45 | 0 (chorus off)                                             |
| 47 | 0 (reverb off)                                             |

That gets you to "the existing WHITESNAKE pad with a gentle 8 kHz LP on top."

---

## Step 1 — Static LP filter character (CC 41, CC 42)

Set the baseline above (everything else off; LFO depth 0). Then sweep CC 41
to find the brightness that feels right musically:

- Low values (1×–4×) — darker, more vocal/woody, closer to a real "vocal pad"
- Default ~7.6× — balanced
- High (10×+) — brighter, up to the 8 kHz clamp

Then add a touch of CC 42 (resonance):

- 0.7 — flat, no emphasis
- 0.9–1.5 — body without nasal honk (default 1.12 sits here)
- 2.0–3.0 — adds character but starts to color hard
- Above 3.5 — risks self-oscillation; pulls the SVF closer to instability

**Goal:** a sustained chord that's musically right *before* you add any
movement.

---

## Step 2 — Per-voice LFO motion (CC 43, CC 44)

Bring up CC 43 (depth) to ~64 (≈0.5). The critical test:

- Hold a 3-note chord for ~15 seconds
- Listen for the brightness to peak at **different times** on the three voices

If they all sound locked in step, the per-voice random phase isn't doing its
job and we have a bug to chase. If they decorrelate, the architecture's
working.

Then adjust CC 44 (rate, exp 0.05–1.5 Hz):

- ~20 (≈0.1 Hz, 10s cycle) — "breath"
- Default ~50 (≈0.2 Hz, 5s cycle) — classic slow swell
- ~100 (≈0.7 Hz) — noticeable wobble
- 127 (1.5 Hz) — full vibrato territory

**Goal:** a held chord that breathes without sounding obviously modulated.

---

## Step 3 — Chorus (CC 45, CC 46)

Bring up CC 45 (wet mix) to ~50 (≈0.4). Listen for the chord to feel like
more than one player — that's the ensemble character.

CC 46 is the sound-design knob. **It clicks on every change** (the underlying
`AudioEffectFlange::voices()` re-init resets the LFO phase and the circular
delay buffer). Don't sweep — pick discrete values, play through a full chord
for 10–15 seconds, then change:

- 32 (~0.25) — subtle thickening
- 64 (~0.5) — default
- 96 (~0.75) — pronounced
- 127 (~1.0) — over the top, into vibrato

The right answer is "stack of voices," not "one voice wobbling." If you hear
pitch wobble on a single sustained note, depth is too high.

---

## Step 4 — Reverb (CC 47, CC 48)

Bring up CC 47 (wet) to ~32 (≈0.25) — just enough to feel "space" behind the
dry pad.

CC 48 (size): start at default ~90 (0.7). Smaller values (0.3–0.5) feel like
a room; larger (0.8–0.9) feel like a hall.

**Goal:** the tail washes between chord changes without smearing the next
chord.

---

## Step 5 — The acid test

Play the *Here I Go Again* intro chord progression against the record:

- Record on speakers, synth on its own monitor
- A/B by muting and unmuting the record

Things you'll iterate hardest on:

- **CC 41 (cutoff)** — brightness vs. the record's mix
- **CC 46 (chorus depth)** — how "thick" the ensemble sits
- **CC 47 (reverb mix)** — how "deep" vs. how "present"

---

## What to listen for

| Symptom                                  | Likely cause                       |
|------------------------------------------|------------------------------------|
| Static / noise on high notes             | Cutoff exceeding stability ceiling (should be fixed by 8 kHz clamp) |
| Chord sounds dead, no evolution          | LFO depth too low or rate too slow |
| All voices peaking together              | Per-voice random phase not working (bug) |
| Single note wobbles in pitch             | Chorus depth too high              |
| Reverb tail smears next chord            | Reverb mix too high or size too large |
| Patch sounds nasal                       | LP resonance too high (drop CC 42) |
| Patch sounds dull                        | LP cutoff too low (raise CC 41)    |

---

## Locking it down

When a setting feels right, **write down the CC value**. Once the whole
patch is dialed in:

1. Hand the values back. I'll bake them into the constructor defaults and
   `loadWhitesnakeInstruments()` so a stray CC doesn't blow them away.
2. CC 46 is the leading candidate to retire entirely (clicks on every
   change; "set once" by nature). Once locked in, we can drop the CC and
   hard-code the chorus depth in the constructor.
