#!/usr/bin/env python3
"""Original synthesized weapon audio for OUTLAND (assets/audio/weapons/). No recordings.

Each gunshot is built the way a real report sounds outdoors, in layers:
  crack   - the supersonic N-wave and muzzle pressure spike: a sharp, high, very short burst
  boom    - the propellant blast: a low sine sweep that drops in pitch, with a fast attack
  bark    - the mid-range body that gives a battle-royale shot its punch (band-passed noise)
  echoes  - discrete reflections off nearby buildings and hills, each duller than the last
  tail    - the diffuse outdoor decay, low-passed and long for big rifles
then soft-clipped for density and normalised. Every gun has its own character (a pistol is
short and snappy, a 7.62 sniper a long thunderclap) and three variants so automatic fire never
machine-guns one sample. Distant versions are muffled, crack-light and echo-heavy; the game
picks them for far shooters and delays them by the speed of sound. Also: grenade explosions
(near and far), the shotgun pump, the sniper bolt, shell casings and the kill confirmation.

    python3 tools/generate_weapon_audio.py           # write assets/audio/weapons/*.wav
    python3 tools/generate_weapon_audio.py --check   # fail if the committed files are stale
"""
import argparse
import io
import sys
import wave
from pathlib import Path

import numpy as np

RATE = 32000
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/audio/weapons"

# name: crack (ms, high-pass Hz, level), boom (start Hz, end Hz, decay s, level),
#       bark (low Hz, high Hz, decay s, level), tail (seconds, level), drive
GUNS = {
    "pistol":  dict(crack=(2.2, 2200, 1.0), boom=(170, 75, .040, .75), bark=(900, 3400, .028, .70), tail=(.55, .20), drive=2.2),
    "rifle":   dict(crack=(1.8, 2600, 1.0), boom=(140, 58, .060, .95), bark=(700, 2800, .042, .80), tail=(.95, .26), drive=2.8),
    "smg":     dict(crack=(2.6, 1700, .85), boom=(120, 62, .050, .90), bark=(550, 2000, .038, .75), tail=(.65, .22), drive=2.6),
    "shotgun": dict(crack=(3.5, 1300, .90), boom=(95, 40, .095, 1.0), bark=(420, 1600, .060, .80), tail=(1.10, .30), drive=3.0),
    "sniper":  dict(crack=(1.4, 3200, 1.0), boom=(85, 36, .120, 1.0), bark=(600, 2500, .065, .85), tail=(1.50, .32), drive=3.2),
}


def band(x, low, high):
    """Band-limit by FFT with soft (raised-cosine) edges."""
    spectrum = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / RATE)
    gain = np.ones_like(f)
    if low > 0:
        edge = low * .5
        gain *= np.clip((f - (low - edge)) / edge, 0, 1) ** 2
    if high < RATE / 2:
        edge = high * .5
        gain *= np.clip(((high + edge) - f) / edge, 0, 1) ** 2
    return np.fft.irfft(spectrum * gain, len(x))


def echoes(x, delays, gains, cutoff):
    out = np.zeros_like(x)
    for delay, gain in zip(delays, gains):
        shift = int(delay * RATE)
        if shift < len(x):
            reflected = band(np.concatenate([np.zeros(shift), x[:len(x) - shift]]), 60, cutoff)
            out += reflected * gain
            cutoff *= .7
    return out


def finish(x, drive, peak=.95):
    x = np.tanh(x * drive) / np.tanh(drive)
    fade = min(len(x), int(.02 * RATE))
    x[-fade:] *= np.linspace(1, 0, fade)
    return x / np.max(np.abs(x)) * peak


def gunshot(name, variant, distant=False):
    p = GUNS[name]
    rng = np.random.default_rng(1000 + variant * 17 + sum(map(ord, name)))
    length = p["tail"][0] * (1.6 if distant else 1) + .15
    t = np.arange(int(length * RATE)) / RATE
    noise = rng.uniform(-1, 1, len(t))
    crack_ms, crack_hp, crack_level = p["crack"]
    crack = band(noise, crack_hp, 12000) * np.exp(-t / (crack_ms / 1000))
    nwave = np.zeros_like(t)                                   # the pressure spike
    w = max(2, int(.00035 * RATE))
    nwave[:w], nwave[w:2 * w] = 1, -.75
    f0, f1, decay, boom_level = p["boom"]
    f0, f1 = f0 * (1 + .04 * (variant - 1)), f1 * (1 + .03 * (variant - 1))
    freq = f1 + (f0 - f1) * np.exp(-t / .03)
    boom = np.sin(2 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t / decay) * (1 - np.exp(-t / .0007))
    boom += band(noise, 30, 260) * np.exp(-t / (decay * 1.4)) * .5
    low, high, bark_decay, bark_level = p["bark"]
    bark = band(noise, low, high) * np.exp(-t / bark_decay) * (1 - np.exp(-t / .0005))
    tail_s, tail_level = p["tail"]
    tail_env = (1 - np.exp(-t / .012)) * np.exp(-t / (tail_s / 4.2))
    tail = band(rng.uniform(-1, 1, len(t)), 50, 1100) * tail_env
    blast = boom * boom_level + bark * bark_level
    reflections = echoes(blast, [.065 + .01 * variant, .14, .29, .51], [.34, .22, .15, .08], 2400)
    if not distant:
        x = (crack * crack_level + nwave * .8) + blast + tail * tail_level * 2.2 + reflections
        return finish(x, p["drive"])
    # Far away: the highs are gone, the crack is a dull snap, the echoes and rumble carry.
    x = crack * crack_level * .25 + blast * .7 + tail * tail_level * 3.5 + reflections * 1.6
    x = band(x, 40, 1500)
    x += echoes(band(x, 40, 900), [.21, .47, .83], [.45, .30, .18], 900)
    return finish(x, p["drive"] * .7, .85)


def explosion(distant=False):
    rng = np.random.default_rng(77 + distant)
    t = np.arange(int((3.2 if distant else 2.6) * RATE)) / RATE
    noise = rng.uniform(-1, 1, len(t))
    freq = 26 + 60 * np.exp(-t / .08)
    boom = np.sin(2 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t / .38) * (1 - np.exp(-t / .002))
    crack = band(noise, 900, 9000) * np.exp(-t / .012)
    roar = band(noise, 40, 700) * (1 - np.exp(-t / .02)) * np.exp(-t / .55)
    debris = band(rng.uniform(-1, 1, len(t)), 1500, 6000) * np.exp(-((t - .35) / .25) ** 2) * .25
    rumble = band(rng.uniform(-1, 1, len(t)), 30, 260) * np.exp(-t / .9) * .7
    x = boom * 1.2 + crack * .8 + roar * .9 + debris + rumble
    x += echoes(band(x, 30, 1200), [.12, .31, .62, 1.0], [.4, .28, .18, .1], 1200)
    if distant:
        x = band(x, 25, 600) + echoes(band(x, 25, 400), [.3, .7], [.4, .25], 400)
    return finish(x, 2.6 if not distant else 1.8, .95 if not distant else .85)


def clicks(times, pitches, decays, length, seed, level=1.0):
    rng = np.random.default_rng(seed)
    t = np.arange(int(length * RATE)) / RATE
    x = np.zeros_like(t)
    for at, pitch, decay in zip(times, pitches, decays):
        age = t - at
        on = age >= 0
        a = np.where(on, age, 0)
        tick = (rng.uniform(-1, 1, len(t)) * .6 + np.sin(2 * np.pi * pitch * a) * .5 + np.sin(2 * np.pi * pitch * 2.7 * a) * .25)
        x += np.where(on, tick * np.exp(-a / decay), 0)
    x = band(x, 300, 9000)
    return x / np.max(np.abs(x)) * .8 * level


def casing(seed):
    # A spent case: three metallic bounces, quickly dying, on hard ground.
    t = np.arange(int(.45 * RATE)) / RATE
    rng = np.random.default_rng(seed)
    x = np.zeros_like(t)
    for at, gain in ((0, 1), (.11 + .02 * (seed % 3), .55), (.2 + .03 * (seed % 2), .3)):
        age = t - at
        a = np.where(age >= 0, age, 0)
        ping = sum(np.sin(2 * np.pi * f * a) * g for f, g in ((3150 + 90 * (seed % 4), 1), (4870, .6), (6900, .35)))
        ping += rng.uniform(-1, 1, len(t)) * .25
        x += np.where(age >= 0, ping * np.exp(-a / .03) * gain, 0)
    return x / np.max(np.abs(x)) * .5


def kill_ding():
    t = np.arange(int(.32 * RATE)) / RATE
    x = (np.sin(2 * np.pi * 1320 * t) * np.exp(-t / .09) +
         np.where(t > .06, np.sin(2 * np.pi * 1980 * (t - .06)) * np.exp(-(t - .06) / .12), 0) * .9)
    return x / np.max(np.abs(x)) * .7


def sounds():
    out = {}
    for name in GUNS:
        for variant in range(3):
            out[f"{name}_{variant}.wav"] = gunshot(name, variant)
        out[f"{name}_distant.wav"] = gunshot(name, 1, distant=True)
    out["explosion.wav"] = explosion()
    out["explosion_distant.wav"] = explosion(True)
    out["shotgun_pump.wav"] = clicks([0, .17], [900, 1250], [.035, .03], .32, 5)
    out["sniper_bolt.wav"] = clicks([0, .12, .3, .42], [1500, 800, 950, 1700], [.02, .04, .035, .02], .55, 6)
    for i in range(3):
        out[f"casing_{i}.wav"] = casing(i)
    out["kill.wav"] = kill_ding()
    return out


def encode(samples):
    pcm = np.clip(np.round(samples * 32767), -32767, 32767).astype("<i2")
    buffer = io.BytesIO()
    with wave.open(buffer, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(pcm.tobytes())
    return buffer.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    stale = []
    total = 0
    for name, samples in sounds().items():
        if not np.all(np.isfinite(samples)) or np.max(np.abs(samples)) > 1:
            raise SystemExit(f"invalid PCM in {name}")
        data = encode(samples)
        total += len(data)
        path = OUT / name
        if args.check:
            if not path.exists() or path.read_bytes() != data:
                stale.append(name)
        else:
            path.write_bytes(data)
    if stale:
        raise SystemExit("Stale weapon audio: " + ", ".join(stale) + f" (run {Path(__file__).name})")
    print(f"weapon audio: {len(sounds())} sounds, {total // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
