"""Chop a Prophet-VS render into per-note WAVs for SF2 building.

Matches Workflow A timing with 2-beat rest between notes:
  60 BPM, 4-beat note + 2-beat gap = 6.0 s per slice.
  First note attack at 3.95 s; 9 notes at MIDI 43, 48, 53, 58, 63, 68, 73, 78, 83.

Usage:
  python chop_samples.py <render.wav> [out_dir]

Defaults to "<render-folder>/samples_chopped".

Stdlib only -- works with PlatformIO's bundled Python:
  & "C:\\Users\\tacer\\.platformio\\penv\\Scripts\\python.exe" chop_samples.py "Whitesnake clean.wav"
"""

import sys
import wave
from pathlib import Path

NOTES = [
    (43, "G2"),
    (48, "C3"),
    (53, "F3"),
    (58, "As3"),
    (63, "Ds4"),
    (68, "Gs4"),
    (73, "Cs5"),
    (78, "Fs5"),
    (83, "B5"),
]
FIRST_ATTACK_S = 3.95
WINDOW_S = 6.0


def main(in_path: Path, out_dir: Path) -> None:
    out_dir.mkdir(exist_ok=True)
    with wave.open(str(in_path), "rb") as src:
        params = src.getparams()
        sr = params.framerate
        n_channels = params.nchannels
        sampwidth = params.sampwidth
        total_frames = src.getnframes()
        print(f"input: {in_path.name}  {sr} Hz  {n_channels}ch  {sampwidth*8}-bit  "
              f"{total_frames/sr:.2f} s")

        for i, (midi, name) in enumerate(NOTES):
            start_frame = int(round((FIRST_ATTACK_S + WINDOW_S * i) * sr))
            n_frames = int(round(WINDOW_S * sr))
            if start_frame + n_frames > total_frames:
                print(f"  WARN: slice {i} ({name}) extends past end of file -- truncating")
                n_frames = total_frames - start_frame
            src.setpos(start_frame)
            data = src.readframes(n_frames)
            out_path = out_dir / f"vocal1_{midi:03d}_{name}.wav"
            with wave.open(str(out_path), "wb") as dst:
                dst.setparams(params)
                dst.writeframes(data)
            print(f"  wrote {out_path.name}  midi={midi}  "
                  f"{start_frame/sr:.2f}-{(start_frame+n_frames)/sr:.2f}s  "
                  f"({n_frames} frames)")

    print(f"\nDone. {len(NOTES)} files in {out_dir}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    in_path = Path(sys.argv[1])
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else in_path.parent / "samples_chopped"
    main(in_path, out_dir)
