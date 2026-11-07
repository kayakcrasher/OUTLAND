#!/usr/bin/env python3
"""The VT2 taser's sounds (assets/audio/weapons/taser_*.wav, reload_taser.wav): the pop of the
cartridge, then the darts' wires crackling at the pulse rate (~19 Hz bursts of arcing), and a
cartridge swap. Mono 16-bit 32 kHz, like the guns'."""
import numpy as np, wave, pathlib

RATE = 32000
OUT = pathlib.Path(__file__).resolve().parent.parent / "assets/audio/weapons"


def write(name, signal):
    signal = np.clip(signal, -1, 1)
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes((signal * 32000).astype(np.int16).tobytes())


def crackle(seed, seconds=0.75, level=0.8, bright=1.0):
    rng = np.random.default_rng(seed)
    n = int(RATE * seconds)
    t = np.arange(n) / RATE
    # The pop: a short thump and a snap as the gas charge fires the darts.
    pop = np.exp(-t / 0.012) * (np.sin(2 * np.pi * 140 * t) * 0.8 + rng.normal(0, 0.6, n))
    # The arc: pulses at ~19 Hz, each a few ms of sharp noise with a buzzing tone underneath.
    pulse_rate = 19 + rng.uniform(-1.5, 1.5)
    phase = (t * pulse_rate) % 1.0
    gate = np.where(phase < 0.18, np.exp(-phase / 0.05), 0.0)
    arc = rng.normal(0, 1, n)
    arc = np.diff(arc, prepend=0) * bright + arc * 0.3          # bright, spitting
    buzz = np.sign(np.sin(2 * np.pi * 950 * t)) * 0.25
    body = (arc * 0.7 + buzz) * gate
    env = np.clip(t / 0.03, 0, 1) * np.exp(-np.maximum(t - 0.55, 0) / 0.06)
    return (pop * 0.9 + body * env) * level


for v in range(3):
    write(f"taser_{v}.wav", crackle(11 + v))
write("taser_distant.wav", np.convolve(crackle(21, level=0.35, bright=0.4), np.ones(8) / 8, mode="same"))
# The reload: the spent cartridge out, a fresh one clicked in.
rng = np.random.default_rng(5)
n = int(RATE * 1.6); t = np.arange(n) / RATE
reload = np.zeros(n)
for at, level in ((0.15, 0.6), (0.22, 0.35), (1.05, 0.8), (1.12, 0.5)):
    i = int(at * RATE); k = int(0.04 * RATE)
    tt = np.arange(k) / RATE
    reload[i:i + k] += level * np.exp(-tt / 0.006) * (rng.normal(0, 1, k) * 0.6 + np.sin(2 * np.pi * 2300 * tt))
write("reload_taser.wav", reload * 0.8)
print("taser audio written")
