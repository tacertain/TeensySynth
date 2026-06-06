# Prophet VS / Whitesnake "Here I Go Again" — Research Summary

What we've learned from the gearspace, Mod Wiggler, Synth Magic, and Korg Modwave-port sources collected in this folder, plus the related web searches we did along the way.

---

## 1. The patch we want

**The intro pad on the 1987 *Here I Go Again* is Prophet VS factory preset 11**, named **"VOCAL 1"** in the Vintage Factory Bank that Arturia ported into Prophet-VS V.

Two corroborating sources:

- **Bill Cuomo (the keyboardist who actually played it)** — quoted in the Gearspace thread (`Gearspace.htm`, post 15449345 by user "kakaroto", May 12 2021):
  > "I asked Bill Cuomo on his fb page and he said it was a mix of the Sequential Prophet VS (vocal pad), the Roland JX-3P (for the bell sounds) and a Kurzweil K250 (for the piano bits). … Preset 11 gets your 90% there..."
- **Arturia user community** — the [Arturia forum "Looking for Preset – Here I Go Again" thread](https://forum.arturia.com/t/looking-for-preset-here-i-go-again/4149) recommends starting from **"11 VOCAL 1"** in the Vintage Factory Bank, then nudging the Brightness Macro up, lowering the Time Macro slightly, and adding a Hall reverb at ~50% wet/dry. Same number, plausible name.

Other instruments on the track per Cuomo:
- **Roland JX-3P** — the bell sounds (this is the misattribution most people get wrong; it's *not* a DX7).
- **Kurzweil K250** — the piano bits.
- Don Airey is also credited on the album, so other synths likely appear elsewhere in the song.

The same Prophet VS patch (and same vector-synth aesthetic) also shows up on Rush's *Force Ten*, The Outfield's *No Surrender*, and Bette Midler's *Wind Beneath My Wings* — per "Vectorman" on Gearspace, who owned a VS in early '87.

**Authority signal worth noting:** On Mod Wiggler's "Definitive Sequential Prophet VS resource thread" (page 6), the user **Don Solaris** lists his sound-design portfolio as "Arturia, Novation, Roland Corporation, SAE Institute, Waldorf Music GmbH, **Whitesnake (band)**." Don Solaris is a working sound designer who has built factory banks for Arturia. He doesn't discuss the *Here I Go Again* patch directly in this thread — his posts here are about hardware refurbishment (OLED, VCF trimmers, OS 1.0 vs 1.2 LFO behaviour) — but his portfolio tying VS work to Whitesnake is a strong corroborating signal that the VS-Whitesnake connection is established in the pro community.

---

## 2. The Prophet VS itself, in brief

From the README of the Korg Modwave port and Synth Magic's product page:

- 1986 Sequential synth. Vector synthesis: **4 digital oscillators (A/B/C/D)**, each playing one of **127 single-cycle wavetables** from ROM, blended in real time via a **joystick** (manual or recorded).
- Signal path: 4 osc → vector mixer → **Curtis analogue low-pass filter (CEM)** → amp envelope. The "warm digital" flavour is exactly that combination: digital sources through analogue filters.
- Lineage: Sequential was acquired by Yamaha in 1989. The team scattered to Yamaha and Korg, and vector synthesis lived on in the Yamaha SY-22 and the Korg Wavestation. The current **Korg Modwave** explicitly bundles the original VS wavetables.

**OS gotcha for sound recreation:** The VS shipped with multiple OS versions, and they affect the *sound*:
- v1.0: free-running LFOs.
- v1.2 (the most common "final" Sequential OS): LFOs and arpeggiator sync to key depressions.
- Don Solaris on Mod Wiggler downgraded from 1.2 → 1.0 specifically because "key resetting LFOs" was unmusical for his work.
- This means an authentic 1987-tracked sound *might* have been recorded on v1.0 with free-running LFOs — but we don't actually know which OS Cuomo's unit ran. If "VOCAL 1" doesn't use an LFO at all, this is moot.

---

## 3. Where the original patch data lives (and doesn't)

**`prophetvs.com` is essentially gone.** The Wayback snapshot just says: *"If you're looking for the VS editor, it's here: http://prophetvs.com/editor"*. The `VS_Factory_Patches.zip` and `VS_ROM_Patches.zip` files that older threads pointed to are not retrievable from the archive. The site survives only as the **Vector Surgeon** editor by Jason Proctor at `prophetvs.com/editor/`, which runs locally and edits `.syx` files but does not itself host the factory bank.

**The factory sysex is therefore not currently easy to obtain via public links.** Realistic ways someone could still get it: someone who already owns the bank file emails a copy (e.g., the offer from `majkemi` on Vintage Synth Explorer in 2011), buying a ROM cartridge from one of the third-party makers (Synthtaste / Michael Krause / Waverex), or extracting it from an Arturia install.

**Available patch resources that are reachable today:**
- **PresetPatch** (`https://www.presetpatch.com/synth/sequential-prophet-vs`) hosts a free user upload titled `Factory_Patches__patch_data.syx` labelled "**Choir Vox**", plus `rom-cart-patches.syx`, `rom-cart-waves.syx`, and `prophet-vs-waves-single-cycles.wav`. Free account required. *Note:* "Choir Vox" might or might not be the same as "VOCAL 1" — labels on community uploads aren't reliable. These are sysex (instructions for a VS), not audio.
- **Make Noise** has a 0.79 sec `Prophet VS waves formatted for Morphagene.wav` on Freesound, **CC0** (free for any use). Useful for the engine-build path; too small to be all 96/127 ROM waves.
- **Starsky Carr** sells the full set of 128 VS waves as 4096-sample WAVs for £20.
- The previously-attached "VS factory bank" file that user `passat88` posted to the Arturia forum is **no longer attached** to the thread (user confirmed).

**Tools and emulators:**
- **Vector Surgeon** — the Jason Proctor editor. Still downloadable from `prophetvs.com/editor/`. Edits and transfers VS sysex.
- **Vector Sector** by General Vibe — historically the most accurate emulator (written by **Joshua Jeffe**, who wrote the original VS firmware at Sequential and coined the term "vector synthesis"). The company is defunct and the plugin is abandoned. Only available now via piracy; not recommended.
- **Arturia Prophet-VS V** — paid plugin, contains the Vintage Factory Bank with "11 VOCAL 1". Has a free demo with time-limited sessions.
- **UVI Vector Pro** — vector-synthesis sample library inspired by the VS and Yamaha SY22.
- **Korg Modwave** — hardware/plugin that includes the original VS wavetables natively. The community port in `docs/prophet VS/Prophet VS Collection for the Korg Modwave/VS.mwbundle` includes 50+ performances; some replicate the original presets, some are inspired-by.

---

## 4. Multisampled versions of the VS Choir / Vocal sound

**The closest available product is Synth Magic Vector X MKII.** From their product page (`Synth Magic Vector X.htm`), it is:

- A custom Kontakt instrument built from samples of Synth Magic's own Prophet VS hardware.
- 6 GB of NI lossless (.ncw) samples across 4 volumes, ~400 snapshot presets.
- Explicit pitch: *"Vector X includes the infamous VS Choir and Filmscore sounds."*
- Includes "Many classic VS factory sounds, raw oscillator waves, Haunting pads, Vector Choirs, Keys, leads, glassy pads and strings."
- £30. **Requires the full retail Kontakt 5.8.1+** (not the free Kontakt Player).

The product page does *not* claim to include "VOCAL 1" by name, but "VS Choir" and "Vector Choirs" sit in the same family. Worth emailing Synth Magic to confirm before buying if exact-patch fidelity matters.

**No free SF2 of "VOCAL 1" was found.** Searched sample-pack sites, SFZ collections, soundfont indexes, KVR forums. Closest free thing is the PresetPatch "Choir Vox" sysex, which is patch instructions, not audio.

---

## 5. Honest paths to a working WHITESNAKE mode in TeensySynth

Ranked by effort vs fidelity, given everything above:

1. **Sample the song itself** *(free, fastest, surprisingly faithful)*. The first ~10 seconds of the 1987 mix expose the pad nearly cleanly. Pull a clean source, isolate 2–3 root notes per octave, build an SF2 in Polyphone, drop it into the existing `SoundfontInstrument` infrastructure. The ADSR/filter/crossfade machinery in `SoundfontInstrument.h` is already exactly what's needed. Captures the chorus + reverb + tape character that's actually what listeners remember.
2. **Buy Synth Magic Vector X MKII** (£30, Kontakt-only). Render the closest snapshot to multisamples per chromatic step, convert to SF2. Highest commercial fidelity available. Licensing for embedding into a personal Teensy synth is fine; redistribution would need permission from Synth Magic.
3. **Arturia Prophet-VS V** (paid, but free demo). Load "11 VOCAL 1" from the Vintage Factory Bank, render to multisamples, convert to SF2. Most authentic to "what Cuomo programmed" because it's the literal ported factory preset.
4. **Build the vector-synth engine from scratch** and load the original sysex parameters. Requires obtaining `VS_Factory_Patches.zip` (which is no longer on the public web — would need a community contact, or to extract from an Arturia install), decoding preset 11, and supplying the 127 ROM single-cycle waves. The free Make Noise CC0 file is too short to be the full set; Starsky Carr's £20 set has all 128. Highest engineering effort, and the result still sounds like a Teensy approximation of a VS, not the recording.

**Recommendation: Path 1.** The user's existing `SoundfontInstrument` already handles ADSR + filter + crossfade. A targeted SF2 sampled from the record reuses that infrastructure and gets to a record-faithful sound in an evening. Vector synthesis as a generic engine is a great project on its own merits but should not be the path to *this specific patch*.

---

## 6. Sources

Original web pages this summary draws from. Local HTML scrapes were
abandoned during cleanup; URLs preserved here so any of these can be
re-fetched if needed.

- **Gearspace thread** — the original thread where Bill Cuomo's quote appears. Definitive source for "Preset 11" and the JX-3P/K250 split. `https://gearspace.com/board/electronic-music-instruments-and-electronic-music-production/1351753-whitesnakes-quot-here-i-go-again-quot-1987-version-synths.html` (Cuomo quote: post `?p=15449345`).
- **Mod Wiggler "Definitive Sequential Prophet VS resource thread", page 1** — canonical list of programmer hardware (Stereoping ProVessorS / Programmer), the Vector Surgeon link, Synthtaste/Krause/Waverex RAM cartridges, and the full OS version history (1.0 / 1.1 / 1.2 / 1.3 / 1.4 / 1.45). No patch-name list. `https://www.modwiggler.com/forum/viewtopic.php?t=233720`
- **Mod Wiggler thread, page 6** — Don Solaris's "Whitesnake (band)" portfolio signature, active hardware refurb conversations, and `shalebridge`'s ongoing work on a v1.45 OS update. `https://www.modwiggler.com/forum/viewtopic.php?t=233720&start=150`
- **Synth Magic Vector X MKII product page** — the explicit "infamous VS Choir and Filmscore sounds" pitch. `https://synthmagic.co.uk/vector-x/`
- **Korg Modwave Prophet VS Collection bundle** — community port (`VS.mwbundle`) with 50+ performances using the VS wavetables; some replicate the original presets, some are inspired-by. A local copy lives in the workbench directory (`docs/prophet VS/Prophet VS Collection for the Korg Modwave/`); the bundle is not directly usable for the Teensy but is a useful cross-check on wavetable provenance.
