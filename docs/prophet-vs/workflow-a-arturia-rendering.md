# Workflow A: Render "11 VOCAL 1" from Arturia Prophet-VS V into a chromatic SF2

End-to-end guide for rendering the literal Cuomo patch out of the Arturia plugin demo and packaging it as a chromatic SF2 that drops into `SoundfontSynthesizer`. Assumes you've read `summary.md` and chose this over path 3 (`path-3-sketch.md`).

---

## Why this works

Arturia's "Vintage Factory Bank" is a port of the original Sequential Circuits VS factory bank, including the literal preset Cuomo identified as "Preset 11" in the Gearspace thread. Rendering it gets you:

- The actual 4 ROM waveforms used in VOCAL 1, mixed at the patch's actual vector position
- The patch's actual amp envelope and filter behavior
- All baked into one note recording per pitch — chromatic playback on the Teensy is just resampling

The whole thing is unambiguous license-wise: Arturia's demo exists *to be rendered from* in order to evaluate the plugin. Personal Teensy use is well within evaluation scope. Distributing the resulting SF2 publicly would cross a line; keeping it on your SD card is fine.

---

## 1. Tools to install

| Tool | What for | Cost | Link |
|---|---|---|---|
| **Arturia Prophet-VS V demo** | The instrument plugin itself | Free demo (20-min sessions) | https://www.arturia.com/products/software-instruments/prophet-vs-v/ |
| **Arturia Software Center (ASC)** | Required to authorize the demo | Free | https://www.arturia.com/support/downloads-manuals#asc |
| **Ableton Live 12 Lite** | DAW to host the plugin and sequence MIDI | Already installed on your machine | — |
| **Audacity** | Trimming + loop-point preview | Free | https://www.audacityteam.org/ |
| **Polyphone** | SF2 builder | Free | https://www.polyphone-soundfonts.com/ |

You already have Live Lite installed at `C:\ProgramData\Ableton\Live 12 Lite\Program\Ableton Live 12 Lite.exe` (also reachable from the Start menu). Lite has 8-track and stock-effects limits that don't matter for this — we only need one MIDI track with one plugin.

### Setup

1. **Arturia**: Install ASC. Sign up for a free Arturia account when prompted.
2. In ASC, find **Prophet-VS V** → click "Try" or "Demo". ASC authorizes the demo and installs the plugin into the system VST3 folder (`C:\Program Files\Common Files\VST3` by default).
3. **Live**: Launch Ableton Live 12 Lite from the Start menu. Dismiss any welcome / Help View prompts.
4. Press `Tab` (or use the arrangement icon — the parallel-lines icon in the very top-right of the window) to switch to **Arrangement View**. The default Session View is clip-grid based; Arrangement View is timeline-based and is what we want for sampling.
5. **Make Live find the plugin**: `Options → Preferences` (Ctrl+,) → **Plug-Ins** tab. Make sure **Use VST3 Plug-In System Folders** is **On**. Click **Rescan** if needed. Close Preferences.
6. **Set sample rate**: still in `Options → Preferences`, go to the **Audio** tab. Set **In/Out Sample Rate** to 44100 Hz. (Teensy SD playback expects 44.1 kHz; matching here avoids resampling later.) Close Preferences.

---

## 2. Verify the demo behavior before committing

Demos can have surprises: 20-min session timeouts, periodic noise watermarks, output silencing every N minutes. Find out what you're dealing with before recording 10 minutes of takes that turn out to be unusable.

1. **Add a MIDI track**: in Live's Arrangement View, `Create → Insert MIDI Track` (or Ctrl+Shift+T). A new empty MIDI track appears below the existing ones.
2. **Insert the plugin**: in Live's left sidebar, click **Plug-Ins** under the Categories section. You should see "Prophet-VS V" listed (probably under an "Arturia" subgroup). Drag it onto your new MIDI track. The plugin's device strip appears in the bottom panel (Live's "Device View").
3. **Open the plugin GUI**: click the wrench/spanner icon on the device strip. The Arturia plugin's full UI opens in a separate window.
4. **Load the preset**: in the Arturia GUI, open the preset browser (typically a button labeled "PROPHET-VS V" or a list icon at the top). Navigate to **Vintage Factory Bank → 11 VOCAL 1**. The preset name appears in the Arturia title bar — confirm it.
5. **Play test notes**: in Live, press `M` to toggle the **Computer MIDI Keyboard** (your QWERTY row plays MIDI notes — A is C, S is D, etc.). Or use a real MIDI controller if connected. Make sure the MIDI track is **armed for recording** (the small circle button in the track header) so the plugin receives input. Play a few sustained notes; listen for noise bursts, sudden audio cuts, or an obvious "demo" watermark every minute or so.
6. **Idle-noise check**: leave the plugin loaded with no notes playing. Click the master output meter at the right of Live's window for ~30 seconds. Any movement on the meter at all means there's a watermark/noise floor. Note its periodicity.

If the demo is clean, proceed. If it watermarks, you have two options:
- **Live with it**: the bursts are usually short and outside the sample windows you'll trim to.
- **Buy the plugin** — it's on sale frequently for ~$50 and the Vintage Factory Bank is permanent.

Do not try to mute the demo's restrictions through hacks. It's not worth the rabbit hole; just buy it if the demo's annoying.

---

## 3. Pick which pitches to sample

The score uses MIDI 43 (G2) to 81 (A5). Sample density is a tradeoff:

- **Every 6 semitones** (7 samples): G2, C#3, G3, C#4, G4, C#5, G5. Lightest, fastest, may have audible timbre shift between zones.
- **Every 5 semitones** (9 samples): G2, C3, F3, Bb3, Eb4, Ab4, Db5, F#5, B5. Recommended starting point. Covers the whole range with one extra at the top.
- **Every 4 semitones** (10 samples): G2, B2, Eb3, F#3, A#3, D4, F4, Ab4, B4, Eb5. Better for picky ears; samples are closer to their target pitch so re-pitching artifacts shrink.
- **Every 3 semitones** (13 samples): closest to chromatic, biggest SF2.

**Start with every 5.** The score is pop chord voicings, not classical violin lines — listeners won't pick out a 1-cent timbre shift in the middle of an Eb major chord.

The VS Vocal 1 patch has *vector envelope motion* — the timbre evolves over the first ~500 ms of each note as the joystick moves through its programmed positions. Capturing the same total time per note across pitches keeps that evolution consistent.

---

## 4. Set up the MIDI clip in Live

### 4a. Critical Live quirk: octave numbering

Ableton uses different octave naming than the rest of the world. Middle C (MIDI 60) is labeled **C3 in Live** but **C4 in your score** (and in MuseScore, Reaper, Logic, almost everything else). The actual *pitches* are identical — just the labels differ. We'll use unambiguous **MIDI numbers** below; "Live's label" column shows what you'll see on Live's piano roll vertical axis.

| MIDI # | Score (C4=middle) | Live (C3=middle) |
|---|---|---|
| 43 | G2 | G1 |
| 48 | C3 | C2 |
| 53 | F3 | F2 |
| 58 | A♯3/B♭3 | A♯2 |
| 63 | D♯4/E♭4 | D♯3 |
| 68 | G♯4/A♭4 | G♯3 |
| 73 | C♯5/D♭5 | C♯4 |
| 78 | F♯5 | F♯4 |
| 83 | B5 | B4 |

### 4b. Set tempo and the loop region

1. In Live's top bar, click the **BPM display** (currently shows whatever default — usually 120).
2. Type `60` and press Enter. With 60 BPM, 1 beat = 1 second, which makes the timing arithmetic trivial.
3. We'll use bars 1 through 50 of the timeline. Each note is 4 beats (4 sec) long, with 1 beat (1 sec) gap; 9 notes × 5 beats = 45 beats. So we want a clip ~46 bars long (since each bar is 4 beats at 4/4, that's ~12 bars). Wait — quick math correction:
   - 1 bar @ 4/4 = 4 beats = 4 seconds at 60 BPM
   - 1 note window (4 sec note + 1 sec gap = 5 sec) = 1.25 bars
   - 9 notes = 11.25 bars total
   - Round up: aim for a clip 13–15 bars long.

### 4c. Create the MIDI clip

1. **Right-click in the empty area of the MIDI track's lane** (in Arrangement View, in the wide horizontal track strip) at bar 1, and select **Insert MIDI Clip**. A short empty clip appears.
2. **Drag the right edge of the clip** to extend it to bar 15 or so. (You can also click-select the clip and type a length in the Clip View at the bottom.)
3. **Double-click the clip** to open the MIDI Note Editor in the bottom panel.

### 4d. Place the 9 notes

Live's MIDI editor: vertical axis is pitch (with a piano keyboard on the left for reference), horizontal axis is time, the timeline ruler on top shows bars.beats.

1. Press **B** to enable **Draw Mode** (or click the pencil icon at the top right of the editor). In Draw Mode, click-and-drag creates new notes.
2. **Set the grid** by right-clicking on the time ruler → choose **Fixed Grid: 1/4** so notes snap to quarter-note (= 1-second) boundaries.
3. Scroll the piano keyboard until you can see Live label "G1" (= MIDI 43). Click-and-drag at bar 1 from beat 1 across 4 beats at the G1 row. You now have a 4-beat note at the lowest pitch.
4. **Verify pitch**: select the note (press B again to leave Draw Mode, then click the note). The right-side **Notes** panel shows "Pitch: G1" and you should also see "Note: 43" — that's the MIDI number. If Live shows G2 (or G0), you're at the wrong pitch — drag the note up or down a row.
5. Repeat for the other 8 notes. The starting bars and pitches:
   - Bar 1.1 → 4 beats, MIDI 43 (Live: G1)
   - Bar 2.2 → 4 beats, MIDI 48 (Live: C2)
   - Bar 3.3 → 4 beats, MIDI 53 (Live: F2)
   - Bar 4.4 → 4 beats, MIDI 58 (Live: A♯2)
   - Bar 6.1 → 4 beats, MIDI 63 (Live: D♯3)
   - Bar 7.2 → 4 beats, MIDI 68 (Live: G♯3)
   - Bar 8.3 → 4 beats, MIDI 73 (Live: C♯4)
   - Bar 9.4 → 4 beats, MIDI 78 (Live: F♯4)
   - Bar 11.1 → 4 beats, MIDI 83 (Live: B4)

   (Each note starts 5 beats after the previous one. Bars are 4 beats, so positions cycle 1.1 → 2.2 → 3.3 → 4.4 → 6.1, etc.)

6. **Set velocity**: select all 9 notes (Ctrl+A inside the MIDI editor). The bottom of the editor shows velocity bars (small bars under each note). Drag any one bar to ~100 — all selected notes follow. Or use the Notes panel's **Velocity** field on the right, type 100.
7. Why velocity 100, not 127: VOCAL 1 has velocity-sensitive filter cutoff (most pads do). 100 is "musically used", closer to what you'd actually play. 127 sounds harsh on most VS patches.

Sanity check: zoom out (press Z to fit, or use the magnifier). You should see 9 evenly spaced notes climbing diagonally up the keyboard.

### 4e. (Optional) Skip manual entry with a MIDI file

If clicking 9 notes by hand sounds tedious, you can drop a pre-built `.mid` file directly onto the MIDI track and Live places the notes for you. I can hand you a one-line Python script that emits the right `.mid` if you want to go that route — ask and I'll add it.

### Optional: render multiple velocity layers

If you want the SF2 to respond dynamically to your playing, render the same 9 pitches at 3 velocities (e.g., 60 / 90 / 120). 27 samples total instead of 9. Polyphone supports velocity-ranged zones cleanly. Skip this on first pass — you can always add it later.

---

## 5. Render the audio from Live

### 5a. Set the render range

Live exports either a fixed time range you specify, or whatever's currently looped. Easiest: set the playback loop to cover the whole MIDI clip plus a couple seconds of trailing silence so the last note's natural release tail makes it into the render.

1. In Arrangement View, look at the timeline ruler at the top. There are two small triangle markers — drag the **left brace** to **bar 1** and the **right brace** to **bar 15** (or wherever your last note's natural release finishes — give it ~3 seconds of buffer past the last note's end).
2. Press the **Loop switch** in the top bar (the circular-arrow icon next to the BPM display) so the loop region is active. Loop region appears highlighted on the timeline.

### 5b. Export

1. `File → Export Audio/Video` (Ctrl+Shift+R).
2. In the Export dialog:
   - **Rendered Track**: Master
   - **Render Start**: 1.1.1
   - **Render Length**: should auto-fill from your loop region. Verify it covers at least 14 bars at 60 BPM = 56 sec.
   - **File Type**: WAV
   - **Sample Rate**: 44100 Hz
   - **Bit Depth**: 24
   - **Render as Loop**: Off
   - **Convert to Mono**: Off (we'll keep stereo so we can choose later)
   - **Normalize**: Off (preserve actual levels for accurate sample building)
   - **Create Analysis File**: Off (not needed)
3. Click **Export**.
4. Save as `vocal1_render.wav` somewhere convenient (e.g. `C:\Users\tacer\WhitesnakeSampling\`).

Live shows a progress dialog. Render takes a few seconds for ~1 min of audio.

**Watermark check**: open `vocal1_render.wav` in Audacity, zoom into the gaps between notes, look for non-zero content. If clean, proceed. If watermarked, either buy the plugin or carefully trim around the watermark spans.

**A note on session timing**: Arturia's demo sessions expire after ~20 minutes. Render the WAV well within that window. If Live becomes silent mid-render, the session expired — close and reopen Live (the Arturia plugin re-authorizes through ASC on next launch) and re-render.

---

## 6. Trim each note into its own WAV

Open `vocal1_render.wav` in Audacity. Even though Live exported stereo, the Teensy's `SoundfontInstrument` outputs mono and duplicates to L/R downstream — so the SF2 wants mono samples. Collapse the render now: `Tracks → Mix → Mix Stereo Down to Mono`. Audacity sums L+R with -6 dB per-channel compensation, so the resulting mono peak matches the original stereo peak (no clipping risk).

For each note in the render:

1. Set selection format to "hh:mm:ss" so you can use the timing you sequenced.
2. Select from ~50 ms before the note's attack to ~50 ms before the *next* note's attack begins (zoom in and you'll see the next attack as a clear amplitude jump). With 1-sec gaps between notes, you'll capture the 4-sec held portion plus ~0.8 sec of natural release tail — enough that the Teensy's envelope release (set to ~800 ms in `loadWhitesnakeInstruments()`) extends it smoothly. If you want a longer natural tail in the sample, re-render in Live with the notes spaced 8 beats apart instead of 5.
3. `File → Export → Export Selected Audio`:
   - Format: WAV (Microsoft) signed 16-bit PCM
   - 44100 Hz, mono
   - Filename: `vocal1_G2.wav`, `vocal1_C3.wav`, etc.
4. Optional: tiny 10 ms fade-in on the very first samples to kill any click. Don't fade the tail.

You should end up with 9 (or 10, or whatever) WAV files, each ~3-4 sec.

### Verify pitch

Important sanity check before SF2 building: pick one WAV, run `Analyze → Plot Spectrum...` on a 1-second selection in the steady portion of the note. The dominant peak should be at the expected frequency (G2 = 98 Hz, C3 = 130.8 Hz, F3 = 174.6 Hz, etc.). If it's off by ~50 cents or more, you placed a note at the wrong pitch in Live's MIDI editor (easy to do with the C3=middle-C labeling) — re-check against the MIDI-number table in section 4a.

---

## 7. Decide on loop points

Two strategies, pick one based on how long the score holds notes:

**Strategy A: No loops, long sample.** If your sample is 4 sec long and the score holds notes for at most ~4 sec (it does — the longest sustained note in the intro is 4 quarter notes at probably ~80 BPM = 3 sec), don't loop at all. The natural decay/release of the recording carries the note. Simpler, more authentic.

**Strategy B: Loop the sustain section.** If you want notes that can sustain *indefinitely* (organ-style), pick a stable section in the middle of each sample after the vector envelope motion has settled (~700 ms in) and set loop points there. Then notes hold forever. Loses the second half of the patch's natural evolution, but who cares for held notes longer than they'd ever exist on the record.

**Recommendation: Strategy A** for the WHITESNAKE-targeted use case. If you find yourself sustaining notes longer than the sample, switch to B later.

Strategy A means the SF2 sample's loop start = loop end (no loop), which Polyphone treats as "play once and stop." The Teensy's `AudioEffectEnvelope` running on top will fade out cleanly via the release setting.

---

## 8. Build the SF2 in Polyphone

### 8a. Create the file

`File → New`, save as `whitesnake_vs_pad.sf2`.

### 8b. Import samples

For each WAV:

1. `Tools → Import → Sample`, select all your WAVs at once (Polyphone batches them).
2. For each imported sample:
   - **Original key**: the MIDI note you sampled it at (43 for G2, 48 for C3, etc.). **Critical** — this is what tells the resampler how to pitch-shift to other keys.
   - **Pitch correction**: 0 cents, unless your verify step found a tuning offset.
   - **Loop start / Loop end** = both 0 (Strategy A: no loop).
   - **Sample rate**: 44100 Hz.

### 8c. Build the instrument

1. Right-click `Instruments → New instrument`, name it `VS Vocal 1`.
2. Drag all 9 samples in.
3. For each sample row, set the **Key range**:
   - Sample at root 43 (G2): keys 0-45
   - Sample at root 48 (C3): keys 46-50
   - Sample at root 53 (F3): keys 51-55
   - Sample at root 58 (Bb3): keys 56-60
   - … and so on, each sample covers ±2 semitones around its root, with the lowest sample extending down to 0 and the highest extending up to 127.
4. **Velocity range** for each: 0-127 (or the layered ranges if you did multi-velocity).
5. **Volume envelope** (per-zone or instrument-global):
   - Attack: 1 ms (your sample already has the patch's natural attack)
   - Hold: 0
   - Decay: 0
   - Sustain: 0 dB
   - Release: **800 ms** (long enough to let the captured natural release tail play out — see note below)

   **Why the long release**: the audio chain on the Teensy is `AudioSynthWavetable → AudioEffectEnvelope → filter`. The SF2 release time gates the wavetable's output before it ever reaches the Teensy envelope. If you leave SF2 release at the default (~1 ms) or set it to something short like 100 ms, the wavetable output dies before the Teensy's `AudioEffectEnvelope` (which `loadWhitesnakeInstruments` sets to 800 ms) has anything to shape. You'll hear a click on key-up. Setting SF2 release to ~800 ms lets the captured release tail in your sample play through, and the Teensy envelope multiplies on top to give the final smooth fade.
6. **Filter**: leave wide open. Your `SoundfontInstrument` runs its own filter on top with CC control.

### 8d. Build the preset

1. Right-click `Presets → New preset`, name it `VS Vocal 1`, bank 0, preset 0.
2. Drag the `VS Vocal 1` instrument into it.

### 8e. Save

`File → Save as` → `whitesnake_vs_pad.sf2`. Should be a few MB.

---

## 9. Test in Polyphone before flashing

Polyphone has a built-in keyboard at the bottom. Click around the entire G2-A5 range (and outside it) and listen for:

- **Pitch correctness across the whole range** — any zone where the timbre suddenly leaps is a key-range gap or overlap.
- **Smooth zone transitions** — adjacent samples shouldn't have radically different brightness.
- **Sustain length** — verify a held key plays the full sample including the natural fade.
- **Release** — verify note-off triggers a smooth tail.

If a zone sounds wrong, double-check its sample's root key in the sample properties.

---

## 10. Drop into TeensySynth

1. Copy `whitesnake_vs_pad.sf2` to the Teensy SD card.
2. In `HybridSynthesizer.h`, add a loader:
   ```cpp
   void loadWhitesnakeInstruments() {
       Serial.println("Loading Whitesnake VS Pad...");
       for (int i = 0; i < 4; i++) soundfont.unloadInstrument(i);
       soundfont.loadInstrument(0, "whitesnake_vs_pad.sf2", 0);

       // Pad envelope on top of sample's natural envelope
       soundfont.setAttack(5.0f);     // very fast — natural attack is in the sample
       soundfont.setDecay(0.0f);
       soundfont.setSustain(1.0f);
       soundfont.setRelease(800.0f);  // medium release tail
       soundfont.setFilterMultiplier(15.0f);  // wide open; tweak via CC
       soundfont.setFilterResonance(0.7f);
       soundfont.setCrossfadeDuration(50.0f);
   }
   ```
3. Add `WHITESNAKE` to the `SynthMode` enum.
   - While there: fix the `mode < 7` bounds bug in lines 200–201 (was already wrong for the existing 8-mode enum).
4. Add `noteOn`/`noteOff` switch arms:
   ```cpp
   case WHITESNAKE:
       soundfont.noteOn(0, key, velocity);
       break;
   // and the analogous noteOff
   ```
5. Optional: channel mapping in `modeFromChannel()` (e.g., channel 9 → WHITESNAKE).
6. Optional: call `loadWhitesnakeInstruments()` from your mode-switch handler so that switching to WHITESNAKE auto-loads the SF2.

---

## 11. Iterate

- **Aliasing on high notes (above MIDI 76 or so)?** Add more sample points in the upper octaves — render extra notes at every 3 semitones from C5 up.
- **Bottom notes feel weak?** The lowest score note is G2. If your lowest sample is also G2, lowest-pitched key range works fine. If you sampled C3 and below and rely on downward resampling, low notes lose top end. Sample G2 explicitly.
- **Too dry?** The Arturia render sounds pretty bare without effects — that's accurate to the dry VS preset. The 1987 record's character is the chorus/reverb chain on top. Either:
  - Plug the Teensy output into a chorus + reverb pedal
  - Use the existing cross-fade filter routing in `SoundfontInstrument` to add a touch of brightness modulation
  - Add reverb in the Teensy graph (not currently in the project; could be a future addition)
- **Wrong "mood"?** Re-verify you have "11 VOCAL 1" loaded, not a different vocal preset.

---

## 12. Honest limitations

- **Vector envelope motion is per-render-position.** Each sampled note captured the vector envelope starting from time 0. When you play a note on the Teensy, the SF2 plays the sample from time 0, so the envelope motion re-triggers each time — which is what the original VS does. *However*, if you held the same note across multiple measures in the score, the original VS would only run the envelope once (the note holds). Your SF2 plays the envelope once per note-on, which is identical behavior. Fine.
- **Polyphony budget.** `SoundfontSynthesizer`'s 4 voices per instrument means up to 4 simultaneous notes. The intro's voicings have up to 5 simultaneous notes (top G+B, inner D, bass G + top of bass D+A). You'll need to either drop the bottom-octave doubling, or expand `SoundfontInstrument`'s `VOICES_PER_INSTRUMENT` from 4 to 6+. The latter is a one-line change but increases CPU load — measure before committing.
- **Demo session timeout.** Once 20 min elapses, audio cuts. If you need to re-render after a restart, ASC takes 30 sec to re-authorize. Trivial in practice.
- **No way to verify Arturia got VOCAL 1 exactly right.** Their port of the factory bank is by ear and from sysex; it's been argued on KVR forums whether some presets are "the same" as the hardware. For *this* preset specifically, the gearspace consensus is that Arturia's "11 VOCAL 1" sounds correct. If you ever get hands-on-time with a real VS and a clean recording, you can A/B it then.

---

## Quick checklist

- [ ] Arturia Software Center installed and Prophet-VS V demo authorized
- [ ] Live launched, Arrangement View, plugin found in Plug-Ins category, sample rate set to 44.1 kHz
- [ ] Demo behavior verified (watermarks, timing)
- [ ] "11 VOCAL 1" loaded from Vintage Factory Bank
- [ ] MIDI track sequenced with 9 notes spanning G2-B5
- [ ] Render settings: 44.1 kHz, 24-bit, stereo or mono
- [ ] Render produced clean audio, no demo bursts in note windows
- [ ] Each note trimmed to its own WAV in Audacity
- [ ] Pitches verified via spectrum on at least 2-3 samples
- [ ] SF2 built in Polyphone with correct root keys and key ranges
- [ ] Tested in Polyphone keyboard across the range
- [ ] SF2 copied to SD card
- [ ] `loadWhitesnakeInstruments()` and `WHITESNAKE` mode wired in `HybridSynthesizer.h`
- [ ] First boot — held notes sustain at correct pitch across the keyboard
- [ ] Played the intro from the score on the Teensy

---

## Why this approach over sampling the record

An earlier abandoned approach was to sample *the recording* itself (intro pad in the 1987 mix). It didn't work: the record's chord voicings have internal voice movement that can't be captured per-pitch, and stem separation introduces artefacts under the pad. Workflow A samples *the dry plugin preset* one note at a time, sidestepping that problem entirely:

- Sources are clean single notes (no chord soup, no production smear)
- Vector envelope motion is captured intact within each note
- Chromatic playback works because each note is at a known pitch
- No stem separation step needed
- No loop-point rabbit hole (Strategy A handles it)

You're trading the production character (chorus, reverb, room) for cleanly-pitched isolated notes. Add the production back via pedals or Teensy effects later (see `path-b-postfx.md` for the design that does this in code).
