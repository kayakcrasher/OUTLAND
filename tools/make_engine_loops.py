#!/usr/bin/env python3
"""Cut the engine loops in assets/audio/engines/ from real engine recordings.

The recordings (credited in assets/audio/engines/CREDITS.md) are downloaded by hand into a
folder (--source); each loop is a steady stretch of one of them: held to a steady pitch (the
revs wander a little in a recording), cut to a whole number of firings and crossfaded at the
seam so it loops without a click, then levelled. The game plays them back pitched to the
engine's revs (game::vehicles::EngineSynth) and prints the firing rate the table below finds
for each, for kEngineLoops in src/game/vehicles/EngineSynth.cpp.

    python3 tools/make_engine_loops.py --source ~/engine-recordings
"""
import argparse
import subprocess
from pathlib import Path

import numpy as np

R = 22050
ROOT = Path(__file__).resolve().parents[1]

# name, source file (a prefix of the downloaded name), start s, end s, about the firing rate (Hz), steady it
LOOPS = [
    ("four_0", "loop_0.wav", 0, None, 43, False),        # OpenGameArt racing-car loops (an idle and five revs)
    ("four_1", "loop_1_0.wav", 0, None, 63, False),
    ("four_2", "loop_3_0.wav", 0, None, 69, False),
    ("four_3", "loop_5_0.wav", 0, None, 77, False),
    ("v8_idle", "Lexus_GS_F", 47.55, 49.7, 52, True),     # a smooth V8 idling
    ("v8_lope", "Ford_Bronco", 10.0, 13.1, 42, False),   # an old V8's lumpy idle
    ("v8_high", "Dodge_Durango", 260.0, 262.1, 92, True),   # a V8 held up in the revs
    ("diesel_idle", "Detroit62", 7.0, 9.9, 41, False),   # a 6.2 litre diesel idling
    ("diesel_high", "Detroit62", 3.7, 5.15, 88, True),   # and revved
]


def load(path):
    raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", str(path), "-ac", "1", "-ar", str(R), "-f", "f32le", "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype="<f4").astype(np.float64)


def pitch(seg, hz):
    """The firing rate near `hz` (autocorrelation, within a quarter either way)."""
    seg = seg - seg.mean()
    n = len(seg)
    spectrum = np.fft.rfft(seg * np.hanning(n), 2 * n)
    ac = np.fft.irfft(np.abs(spectrum) ** 2)[:n]
    lo, hi = int(R / (hz * 1.25)), min(n - 2, int(R / (hz * .8)))
    k = lo + int(np.argmax(ac[lo:hi]))
    # Parabolic interpolation for a fractional lag.
    a, b, c = ac[k - 1], ac[k], ac[k + 1]
    shift = .5 * (a - c) / (a - 2 * b + c) if a - 2 * b + c != 0 else 0
    return R / (k + shift)


def steady(x, hz):
    """Resample so the firing rate holds still: track it, then read the recording faster where it sags."""
    hop, win = 256, 2048
    centres, rates = [], []
    for at in range(0, len(x) - win, hop):
        centres.append(at + win / 2)
        rates.append(pitch(x[at:at + win], hz))
    rates = np.convolve(np.pad(rates, 2, mode="edge"), np.ones(5) / 5, mode="valid")
    f = np.interp(np.arange(len(x)), centres, rates)
    target = float(np.median(rates))
    # Each input sample, counted in output samples (firings run at `target`); invert to read.
    position = np.cumsum(f / target) - 1
    reads = np.interp(np.arange(int(position[-1])), position, np.arange(len(x)))
    return np.interp(reads, np.arange(len(x)), x), target


def seamless(x, hz):
    """A whole number of firings, with the seam crossfaded."""
    period = R / hz
    fade = int(.06 * R)
    usable = len(x) - fade - int(period) - 1
    count = int(usable / period)
    length = int(round(count * period))
    # Nudge the cut to where the wave lines up best with the start.
    window = x[:fade]
    best, best_score = length, -1e9
    for cut in range(length - int(period / 2), min(usable + int(period / 2), len(x) - fade)):
        score = float(np.dot(window, x[cut:cut + fade]))
        if score > best_score:
            best, best_score = cut, score
    out = x[:best].copy()
    ramp = np.linspace(0, 1, fade)
    out[:fade] = x[best:best + fade] * np.sqrt(1 - ramp) + x[:fade] * np.sqrt(ramp)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "assets/audio/engines")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, prefix, start, end, hz, hold in LOOPS:
        source = next(p for p in sorted(args.source.rglob("*")) if p.name.startswith(prefix))
        x = load(source)
        x = x[int(start * R):int(end * R) if end else len(x)]
        # No rumble below the engine (around the seam, for a loop that is one already).
        x = x - np.convolve(np.pad(x, 256, mode="wrap" if end is None else "edge"), np.ones(512) / 512, mode="same")[256:-256]
        if hold:
            x, rate = steady(x, hz)
        else:
            rate = pitch(x, hz)
        loop = seamless(x, rate)
        loop *= 10 ** (-18 / 20) / np.sqrt(np.mean(loop ** 2))
        loop /= max(1.0, np.max(np.abs(loop)) / .97)
        target = args.output / f"{name}.wav"   # (Vorbis would trim the end and click at the seam)
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-f", "f32le", "-ar", str(R), "-ac", "1", "-i", "-",
                        "-c:a", "pcm_s16le", str(target)], input=loop.astype("<f4").tobytes(), check=True)
        print(f'{{"{name}", {rate:.2f}F}},  // {len(loop) / R:.2f} s from {source.name}')


if __name__ == "__main__":
    main()
