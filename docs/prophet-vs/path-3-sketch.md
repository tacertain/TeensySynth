# Path 3: VS-flavoured pad on top of existing soundfont infra

Concrete plan for getting a chromatically playable VS-Vocal-1-flavoured pad mode running on TeensySynth, using the `SoundfontInstrument` / `SoundfontSynthesizer` you already have. No new audio engine, no custom DSP code — just SF2 file authoring, one small infrastructure addition (per-slot mixer gain), and a new mode wire-up.

---

## The shape of the plan

```
4 single-cycle VS-style waveforms (each in its own tiny SF2)
        │
        ▼
SoundfontSynthesizer's 4 instrument slots
   slot 0: wave A    slot 1: wave B    slot 2: wave C    slot 3: wave D
        │                  │                  │                  │
   per-voice ADSR    per-voice ADSR    per-voice ADSR    per-voice ADSR
   per-voice filter  per-voice filter  per-voice filter  per-voice filter
        │                  │                  │                  │
        └──────────┬───────┴──────────┬───────┴──────────┬──────┘
                   ▼                  ▼                  ▼
                       finalMixer (4 inputs, gains a/b/c/d)
                                    │
                           PROPHET_VS mode output
```

Every note-on triggers all 4 slots simultaneously at the same MIDI pitch. The `finalMixer` per-input gains become the *static* vector mix (manual joystick position). Adding a vector envelope to automate those gains over time is a follow-up — not needed for first sound.

The trick that makes this work cheaply: each "instrument" SF2 contains a single **single-cycle waveform** with `loop_start = 0`, `loop_end = sample length`, mapped to the entire MIDI key range. `sf22aswt` resamples it for any pitch you trigger. The result is a sustained tone whose timbre is that one waveform. Layer 4 of those and you have approximate VS oscillator-stack synthesis built entirely out of sample playback.

---

## Step 1 — Get VS single-cycle waveforms

Free option: **Make Noise's CC0 Prophet VS waves on Freesound** (`https://freesound.org/people/makenoisemusic/sounds/482906/`). 0.79 sec, 48 kHz, stereo. The file concatenates the VS ROM single-cycles end-to-end for use with the Morphagene module, which means you'll need to chop it into individual waves.

Alternative (paid, higher quality): **Starsky Carr Prophet VS waves** (£20, 128 waves at 4096 samples each, ready to use without chopping).

If you have access to a Korg Modwave (or know someone who does), that includes the VS wavetables natively and can render single notes via MIDI/USB.

### Chopping the Make Noise file

1. Open the WAV in Audacity.
2. Convert to mono (`Tracks → Mix → Mix Stereo Down to Mono`).
3. The file is 0.79 sec at 48 kHz = 38,016 samples. The VS has 96 ROM waveforms originally; if all 96 are concatenated, each is ~396 samples (≈121 Hz fundamental, near B2). If 128 waves, each is ~297 samples (≈162 Hz, near E3).
4. Use `Analyze → Find Zero Crossings` and `View → Show Spectrogram` to spot wave boundaries — they're the points where the waveform shape resets.
5. Export each individual cycle as `vs_wave_001.wav`, `vs_wave_002.wav`, etc. Mono, 16-bit PCM, 48 kHz.

Realistically: chopping all 96+ waves by hand is painful. Two shortcuts:
- **Just chop ~12-20 candidates by ear** — pick spots that sound vocal/formant-like. You only need 4 in the final patch.
- **Write a Python one-shot** to slice the file into N equal segments, then audition them. (Each segment is a 1-cycle loop at file_sample_rate / segment_length Hz.)

### Picking 4 waves

Without the actual VS Preset 11 sysex (still not publicly available — see `summary.md`), the exact 4-wave combo is a guess. For "VOCAL 1" character, audition for waves that have:

- **Formant-like spectra** — energy concentrated in 2-3 broad humps, like a vowel
- **Soft attack/decay shape** — not a pure square or sawtooth (those are too aggressive for this patch)
- **Slight buzz on top** — implies a few mid-range harmonics still present

Start with this rough recipe, then iterate by ear:
- **Wave A**: brightest formant — gives the patch its characteristic "ahh"
- **Wave B**: a darker formant — body/warmth
- **Wave C**: a saw or saw-like wave — bite/edge underneath
- **Wave D**: a sine or soft triangle — sub-fundamental warmth

If your audition gives you something that sounds like "voices stacked", you're close.

---

## Step 2 — Build 4 single-wave SF2s in Polyphone

For each of the 4 chosen waves, build a separate SF2 file. Polyphone `File → New`, save as `vs_wave_a.sf2`, `vs_wave_b.sf2`, `vs_wave_c.sf2`, `vs_wave_d.sf2`.

Per file:

1. **Import sample.** `Tools → Import → Sample`, pick the WAV.
2. **Sample properties:**
   - **Original key**: pick whatever the wave's natural pitch is. If you sliced the Make Noise file into 96 equal segments at 48 kHz, each is 396 samples ≈ 121 Hz ≈ MIDI 47 (B2). Set Original key = 47.
   - **Loop start** = 0
   - **Loop end** = (sample length − 1)
   - **Loop playback** = "Loop continuously" (continues looping while the key is held; release plays the natural decay, but for a single-cycle wave there's no decay tail — just silence after release, which is fine because the `AudioEffectEnvelope` in your `SoundfontInstrument` handles the release fade).
3. **Build instrument**: right-click `Instruments → New instrument`, name it `VS Wave A`. Drag the sample in.
   - Key range: 0–127 (the whole MIDI keyboard — sf22aswt will resample).
   - Velocity range: 0–127.
4. **Volume envelope** in the instrument zone: leave SF2 envelope flat (Attack=1ms, Sustain=0dB, Release=0). The `SoundfontInstrument` ADSR will apply on top — don't double-envelope.
5. **Build preset**: right-click `Presets → New preset`, name it `VS Wave A`, bank 0, preset 0. Drag the instrument in.
6. **File → Save as `vs_wave_a.sf2`**.

Each file should be tiny — a few KB.

---

## Step 3 — Add per-slot mixer gain to `SoundfontSynthesizer`

The one piece of code you actually have to change. Currently `SoundfontSynthesizer::updateMixerGains` divides `finalMixer` evenly across loaded slots (`1/N` per input). For path 3 we want explicit per-slot control so we can set the A/B/C/D mix manually (and later automate it with a vector envelope).

**Header (`SoundfontSynthesizer.h`):**

Add to the `private` section:
```cpp
float instrumentGains[MAX_INSTRUMENTS];  // 0.0 - 1.0 per slot
```

Add to the public API:
```cpp
void setInstrumentGain(int slot, float gain);  // 0.0 - 1.0
float getInstrumentGain(int slot) const;
```

**Implementation (`SoundfontSynthesizer.cpp`):**

In the constructor, initialize:
```cpp
for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
    instrumentGains[i] = 1.0f;
}
```

Replace `updateMixerGains` body with:
```cpp
for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
    finalMixer.gain(i, instruments[i].isLoaded() ? instrumentGains[i] : 0.0f);
}
```

Add the new methods:
```cpp
void SoundfontSynthesizer::setInstrumentGain(int slot, float gain) {
    if (slot < 0 || slot >= MAX_INSTRUMENTS) return;
    instrumentGains[slot] = gain;
    updateMixerGains();
}

float SoundfontSynthesizer::getInstrumentGain(int slot) const {
    if (slot < 0 || slot >= MAX_INSTRUMENTS) return 0.0f;
    return instrumentGains[slot];
}
```

That's the only structural change to existing code. Existing callers keep working because the default `1.0f` per slot combined with `isLoaded()` gating reproduces the previous "load → instantly audible" behaviour, just without the auto-divide. You'll want to compensate by lowering each slot's gain when you load 4 of them (e.g., 0.25 each) to avoid clipping — that's exactly the manual mix you wanted anyway.

---

## Step 4 — Add `PROPHET_VS` mode to `HybridSynthesizer`

**Enum:** add `PROPHET_VS` to `SynthMode`. While you're in there, fix the bounds check at lines 200–201 — the `mode < 7` was already wrong for the 8-mode enum, and adding a 9th makes it worse. Probably easiest:

```cpp
const char* modeNames[] = {"PLUCKED_STRINGS", "DRONE", "STRING_PADS",
                           "SOUNDFONT", "SPLIT", "IRAN", "TUSK",
                           "TUSK_CHORD", "PROPHET_VS"};
const int numModes = sizeof(modeNames) / sizeof(modeNames[0]);
const char* currentModeName = (currentMode >= 0 && currentMode < numModes) ? modeNames[currentMode] : "UNKNOWN";
```

**Loader** (alongside `loadTuskInstruments()`):
```cpp
void loadProphetVSInstruments() {
    Serial.println("Loading Prophet VS waves...");
    for (int i = 0; i < 4; i++) soundfont.unloadInstrument(i);

    soundfont.loadInstrument(0, "vs_wave_a.sf2", 0);
    soundfont.loadInstrument(1, "vs_wave_b.sf2", 0);
    soundfont.loadInstrument(2, "vs_wave_c.sf2", 0);
    soundfont.loadInstrument(3, "vs_wave_d.sf2", 0);

    // Initial vector mix — equal blend, headroom for 4-slot sum
    soundfont.setInstrumentGain(0, 0.25f);
    soundfont.setInstrumentGain(1, 0.25f);
    soundfont.setInstrumentGain(2, 0.25f);
    soundfont.setInstrumentGain(3, 0.25f);

    // Pad envelope: fast attack (single-cycle waves have no natural attack),
    // long release. Sustain held until note-off.
    soundfont.setAttack(20.0f);
    soundfont.setDecay(100.0f);
    soundfont.setSustain(0.9f);
    soundfont.setRelease(1500.0f);

    // Filter: moderate cutoff, modest resonance for body
    soundfont.setFilterMultiplier(8.0f);
    soundfont.setFilterResonance(1.2f);

    // Crossfade: short, since we're not crossfading filter heavily here
    soundfont.setCrossfadeDuration(50.0f);
}
```

**`noteOn` switch arm:**
```cpp
case PROPHET_VS:
    // Layer all 4 oscillator slots at the same pitch
    for (int slot = 0; slot < 4; slot++) {
        soundfont.noteOn(slot, key, velocity);
    }
    break;
```

**`noteOff` switch arm:**
```cpp
case PROPHET_VS:
    for (int slot = 0; slot < 4; slot++) {
        soundfont.noteOff(slot, key);
    }
    break;
```

**Channel mapping** in `modeFromChannel()`: pick an unused MIDI channel (e.g., 9) and map it to `PROPHET_VS` for live mode switching.

---

## Step 5 — MIDI CCs for live tweaking

The "manual joystick" lives in CCs. Pick four nearby controllers in `MIDIControlCallbacks.cpp` and map them to `setInstrumentGain(0..3, value/127.0f)`. Suggested:

- **CC 12-15**: oscillator A/B/C/D mix (0-127 → 0.0-1.0)
- **CC 16**: master pad cutoff (already wired? if not, map to `setFilterMultiplier`)
- **CC 17**: pad release time (map to `setRelease`)
- **CC 18**: pad attack time (map to `setAttack`)

This gives you live timbre control while iterating. Once a mix sounds right, hard-code the values in `loadProphetVSInstruments()`.

---

## Step 6 — Iterate

First boot will probably sound *thin* or *thick* in the wrong way. Things to try, in this order:

1. **Mix balance.** All 4 at 0.25 is rarely the right answer. Try 0.4 / 0.3 / 0.2 / 0.1 for "front-loaded" weighting. For VS-Vocal-style: heaviest on the formant waves (A, B), light on saw (C), trace of sine (D).
2. **Filter cutoff.** Multiplier of 8.0 (8× note frequency) is fairly bright. For more "vocal" character, try 3.0–5.0. Drop resonance to ~0.7 if it sounds nasal, raise to 1.5 if it sounds dull.
3. **Wave selection.** If it doesn't sound vocal at all, the 4 waves you picked aren't the right ones. Audition more from the source pack and swap. The "VS Choir" character specifically comes from waves with strong 600-1200 Hz formants.
4. **Slight detune** between slots: not exposed yet. Could be added later by giving each SF2 a slightly different pitch correction (e.g., +5 cents on one, -3 cents on another) for chorus-like fattening. Don't bother on first pass.
5. **External chorus + reverb.** The original record's "size" is largely the chorus pedal and hall reverb. Don't try to add those on the Teensy now — play through pedals or your FX chain.

---

## What's *not* in this plan (deliberately)

- **Vector envelope (XY automation).** Real VS patches modulate the A/B/C/D mix over the note's lifetime — that's why each note seems to evolve. We're starting with a static mix because (a) it's much simpler, and (b) the Whitesnake patch's evolution is subtle compared to, say, *Sledgehammer*. If the static version sounds dead, this is the upgrade — add a `VectorEnvelope` helper that ticks per-block and writes `setInstrumentGain` for each voice slot. Single-page class.
- **Per-voice vector mix.** All 4 voices in `SoundfontSynthesizer` share the same gain settings. That's fine for our use case (everyone hits a chord and they share the same vector position), but a "real" VS has independent vector envelopes per voice. Don't need this for the song.
- **Pitch-correct oscillator stack.** All 4 slots play the same MIDI pitch. No 5ths, no octaves, no per-osc detune. Single-cycle stacking only — which is what VOCAL 1 is.
- **Joystick automation recording.** The original VS lets you record joystick movements as part of the patch. If the static mix gets you 90% there for this song, skip this entirely.

---

## Honest cost estimate

- **SF2 authoring** (steps 1-2): half a day to a day, mostly auditioning waves
- **Per-slot mix code** (step 3): 30 minutes
- **Mode wire-up** (step 4): 30 minutes
- **MIDI CCs** (step 5): 30 minutes
- **Iteration** (step 6): one to several evenings, sound-design dependent

The longest part is the sound-design iteration in step 6, which can't be avoided — the only way to tell whether 4 specific waves at specific gains sound like "VOCAL 1" is to listen.

---

## Quick checklist

- [ ] VS waveform source obtained (Make Noise CC0 file, or alternative)
- [ ] 4 candidate waves chopped and saved as mono 16-bit WAVs
- [ ] 4 single-wave SF2 files built in Polyphone, root keys correct
- [ ] `setInstrumentGain` API added to `SoundfontSynthesizer`
- [ ] `PROPHET_VS` mode added to `SynthMode` enum, bounds check fixed
- [ ] `loadProphetVSInstruments()` and `noteOn`/`noteOff` cases wired
- [ ] CCs mapped to live mix and envelope controls
- [ ] SF2 files copied to SD card
- [ ] First boot — make a sound at all
- [ ] Iterate the mix and waves until it sounds like the record
