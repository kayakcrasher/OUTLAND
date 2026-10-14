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
(near and far), the shotgun pump, the sniper bolt, shell casings and the kill confirmation;
each gun's reload (timed to its reload), drawing a weapon, bullets cracking past and
ricocheting near the player, hit markers, taking hits (body and armour plate), picking up,
pulling a grenade pin, the zone warning pulse and the low-health heartbeat; and the siren of
the police cars and ambulances (a seamless 4 s wail loop). For the feel of play: car crashes
(light and heavy, with glass), the wet thwack of a round striking a body, a hard landing, a low
punch under the player's own shots, and the ringing in the ears after a grenade goes off close.

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


def thing(t, at, low, high, freq, decay, seed, level=1.0, ring=0.0):
    """One mechanical event at `at` s: a band of noise (the body of the click or clunk) with a
    sine for its pitch, and optionally a metallic ring."""
    rng = np.random.default_rng(seed)
    age = t - at
    on = age >= 0
    a = np.where(on, age, 0)
    env = np.exp(-a / decay) * (1 - np.exp(-a / .0004))
    body = band(rng.uniform(-1, 1, len(t)), low, high) * 1.6 + np.sin(2 * np.pi * freq * a) * .7
    if ring:
        body += (np.sin(2 * np.pi * freq * 2.76 * a) + .6 * np.sin(2 * np.pi * freq * 5.4 * a)) * ring * np.exp(-a / (decay * 3))
    return np.where(on, body * env, 0) * level


def scrape(t, at, length, low, high, seed, level=1.0):
    rng = np.random.default_rng(seed)
    age = t - at
    env = np.where((age >= 0) & (age < length), np.sin(np.pi * np.clip(age / length, 0, 1)), 0)
    return band(rng.uniform(-1, 1, len(t)), low, high) * env * level


def normalise(x, peak):
    return x / np.max(np.abs(x)) * peak


# Each reload is a sequence of (kind, time) timed inside the gun's reload (WeaponDefinition):
# release (a light click), out (the magazine sliding out), in (seated, a clunk), slap,
# handle/slide (a charging pull and snap), shell (one shell thumbed in), bolt_* (the bolt).
RELOADS = {
    "pistol":  (1.9, [("release", .10), ("out", .18), ("in", .95), ("slide", 1.42)]),
    "rifle":   (2.1, [("release", .12), ("out", .22), ("in", 1.05), ("slap", 1.22), ("handle", 1.62)]),
    "smg":     (2.3, [("release", .14), ("out", .25), ("in", 1.18), ("slap", 1.34), ("handle", 1.85)]),
    "shotgun": (4.2, [("shell", .45), ("shell", 1.05), ("shell", 1.65), ("shell", 2.25), ("shell", 2.85), ("pump", 3.55)]),
    "sniper":  (3.8, [("bolt_up", .18), ("bolt_back", .34), ("release", .70), ("out", .80), ("in", 1.95), ("bolt_fwd", 3.0), ("bolt_down", 3.16)]),
}


def reload(name):
    seconds, steps = RELOADS[name]
    t = np.arange(int((seconds + .1) * RATE)) / RATE
    x = np.zeros_like(t)
    for i, (kind, at) in enumerate(steps):
        seed = 300 + i * 11 + sum(map(ord, name))
        if kind == "release":
            x += thing(t, at, 2500, 9000, 2200, .008, seed, .45)
        elif kind == "out":
            x += scrape(t, at, .16, 1500, 6000, seed, .35) + thing(t, at + .16, 300, 2500, 330, .03, seed + 1, .4)
        elif kind == "in":
            x += scrape(t, at - .12, .12, 1800, 6500, seed, .3) + thing(t, at, 250, 3000, 240, .045, seed + 1, 1.0, ring=.25)
        elif kind == "slap":
            x += thing(t, at, 150, 1800, 150, .05, seed, .7)
        elif kind in ("handle", "slide"):
            x += thing(t, at, 1200, 7000, 900, .02, seed, .55) + scrape(t, at + .01, .1, 2000, 7000, seed + 1, .3)
            x += thing(t, at + .13, 600, 7000, 620, .04, seed + 2, 1.0, ring=.35)
        elif kind == "shell":
            x += scrape(t, at - .08, .09, 1500, 5000, seed, .25) + thing(t, at, 300, 3500, 420, .035, seed + 1, .75, ring=.2)
        elif kind == "pump":
            x += thing(t, at, 500, 4500, 900, .035, seed, .9) + scrape(t, at, .14, 1200, 5000, seed + 1, .35)
            x += thing(t, at + .17, 500, 5000, 1250, .03, seed + 2, 1.0, ring=.3)
        elif kind.startswith("bolt_"):
            freq = {"bolt_up": 1500, "bolt_back": 800, "bolt_fwd": 950, "bolt_down": 1700}[kind]
            x += thing(t, at, 900, 8000, freq, .03, seed, .8, ring=.25)
            if kind in ("bolt_back", "bolt_fwd"):
                x += scrape(t, at - .1, .1, 2000, 7000, seed + 1, .3)
    return normalise(band(x, 120, 11000), .8)


def equip():
    t = np.arange(int(.45 * RATE)) / RATE
    x = scrape(t, 0, .22, 400, 3000, 41, .5) + thing(t, .2, 300, 4000, 520, .03, 42, .8, ring=.3)
    x += thing(t, .3, 1500, 8000, 1900, .01, 43, .5)
    return normalise(x, .65)


def whiz(variant):
    # A round going past: the supersonic crack beside the head, then the air tearing as it goes.
    rng = np.random.default_rng(500 + variant)
    t = np.arange(int(.32 * RATE)) / RATE
    crack = band(rng.uniform(-1, 1, len(t)), 2500, 12000) * np.exp(-t / .0025)
    crack[:int(.0003 * RATE)] += 1
    sweep = np.zeros_like(t)
    centre = 3400 - 900 * variant
    for k in range(6):  # a band whose pitch falls (Doppler) as it passes
        start, length = int(k * .03 * RATE), int(.06 * RATE)
        seg = band(rng.uniform(-1, 1, length), centre * (1 - .1 * k) * .7, centre * (1 - .1 * k) * 1.4)
        sweep[start:start + length] += seg * np.hanning(length) * (1 - k / 7)
    x = crack * 1.2 + sweep * .5
    return finish(x, 1.8, .85)


def ricochet(variant):
    rng = np.random.default_rng(600 + variant)
    t = np.arange(int(.6 * RATE)) / RATE
    hit = band(rng.uniform(-1, 1, len(t)), 200, 5000) * np.exp(-t / .012) * (1 - np.exp(-t / .0003))
    f = (2600 + 500 * variant) * np.exp(-t / .5)
    whine = np.sin(2 * np.pi * np.cumsum(f) / RATE) * np.exp(-((t - .12) / .16) ** 2) * .35
    dirt = band(rng.uniform(-1, 1, len(t)), 800, 5000) * np.exp(-((t - .05) / .06) ** 2) * .3
    return finish(hit + whine + dirt, 1.6, .75)


def hurt(variant):
    # Taking a round: a deep body thump and a short wet slap.
    rng = np.random.default_rng(700 + variant)
    t = np.arange(int(.3 * RATE)) / RATE
    f = 95 + 40 * np.exp(-t / .03) + 8 * variant
    thump = np.sin(2 * np.pi * np.cumsum(f) / RATE) * np.exp(-t / .07) * (1 - np.exp(-t / .002))
    slap = band(rng.uniform(-1, 1, len(t)), 300, 2200) * np.exp(-t / .02)
    return finish(thump * 1.2 + slap * .7, 2.0, .85)


def armour_hit():
    rng = np.random.default_rng(800)
    t = np.arange(int(.45 * RATE)) / RATE
    thump = band(rng.uniform(-1, 1, len(t)), 80, 900) * np.exp(-t / .03)
    ring = (np.sin(2 * np.pi * 1850 * t) + .7 * np.sin(2 * np.pi * 2930 * t) + .4 * np.sin(2 * np.pi * 4410 * t)) * np.exp(-t / .09)
    return finish(thump + ring * .45, 1.6, .75)


def hitmarker():
    rng = np.random.default_rng(900)
    t = np.arange(int(.09 * RATE)) / RATE
    x = np.sin(2 * np.pi * 2650 * t) * np.exp(-t / .018) + band(rng.uniform(-1, 1, len(t)), 3000, 9000) * np.exp(-t / .004) * .6
    return normalise(x, .6)


def pickup():
    t = np.arange(int(.35 * RATE)) / RATE
    x = scrape(t, 0, .15, 600, 4000, 51, .5) + thing(t, .14, 400, 5000, 700, .03, 52, .7, ring=.2)
    x += np.where(t > .18, np.sin(2 * np.pi * 1560 * (t - .18)) * np.exp(-(t - .18) / .05), 0) * .25
    return normalise(x, .55)


def grenade_pin():
    t = np.arange(int(.5 * RATE)) / RATE
    x = thing(t, 0, 2000, 9000, 3100, .03, 61, .9, ring=.5)          # the pin's ring
    x += thing(t, .2, 800, 6000, 1400, .02, 62, .7, ring=.3)         # the spoon flies
    x += scrape(t, .26, .2, 300, 1600, 63, .4)                       # the throw
    return normalise(x, .65)


def zone_warning():
    # A low electric pulse, once a second while outside the zone.
    t = np.arange(int(.5 * RATE)) / RATE
    env = np.sin(np.pi * np.clip(t / .45, 0, 1)) ** 2
    x = (np.sin(2 * np.pi * 110 * t) + .5 * np.sin(2 * np.pi * 220 * t) + .3 * np.sign(np.sin(2 * np.pi * 55 * t))) * env
    x *= 1 + .4 * np.sin(2 * np.pi * 18 * t)
    return normalise(band(x, 40, 2500), .6)


def heartbeat():
    t = np.arange(int(.8 * RATE)) / RATE
    x = np.zeros_like(t)
    for at, level in ((0, 1), (.28, .7)):
        a = np.where(t >= at, t - at, 0)
        f = 48 + 30 * np.exp(-a / .02)
        x += np.where(t >= at, np.sin(2 * np.pi * np.cumsum(f) / RATE * (t >= at)) * np.exp(-a / .06) * (1 - np.exp(-a / .004)), 0) * level
    return normalise(band(x, 25, 400), .8)


def siren():
    # A wail rising and falling over 4 s, a square-ish horn tone softened by a low-pass; the
    # carrier is tuned so the loop closes on a whole cycle (no click when it repeats).
    t = np.arange(int(4.0 * RATE)) / RATE
    f = 900 + 420 * np.sin(2 * np.pi * t / 4 - np.pi / 2)
    phase = np.cumsum(f) / RATE * 2 * np.pi
    phase *= round(phase[-1] / (2 * np.pi)) * 2 * np.pi / phase[-1]
    x = np.sin(phase) + .33 * np.sin(3 * phase) + .2 * np.sin(5 * phase) + .12 * np.sin(7 * phase)
    x = band(x, 300, 4200)
    return normalise(np.tanh(x * 1.4), .7)


def crash(heavy, variant):
    """A car hitting something: the thump of the impact, crumpling metal (resonant clunks), and
    for a heavy crash a scatter of breaking glass and a scrape."""
    rng = np.random.default_rng(300 + heavy * 10 + variant)
    t = np.arange(int((1.4 if heavy else .7) * RATE)) / RATE
    freq = 38 + 40 * np.exp(-t / .05)
    thump = np.sin(2 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t / (.16 if heavy else .09)) * (1 - np.exp(-t / .0015))
    x = thump * 1.3 + band(rng.uniform(-1, 1, len(t)), 70, 1100) * np.exp(-t / (.07 if heavy else .04))
    for k in range(9 if heavy else 4):
        at = rng.uniform(0, .28 if heavy else .12)
        x += thing(t, at, 300, 2600, rng.uniform(180, 520), rng.uniform(.03, .08), 400 + k + variant * 20,
                   rng.uniform(.25, .6), ring=.5)
    if heavy:
        for k in range(26):
            at = .03 + rng.exponential(.12)
            x += thing(t, at, 3500, 11000, rng.uniform(2800, 5200), rng.uniform(.006, .02), 500 + k + variant * 40,
                       rng.uniform(.08, .25), ring=.7)
        x += scrape(t, .25, .7, 900, 5000, 600 + variant, .18)
    return finish(x, 2.4 if heavy else 2.0, .95 if heavy else .8)


def body_hit(variant):
    """A round striking a body: a wet, low thwack under a short slap."""
    rng = np.random.default_rng(700 + variant)
    t = np.arange(int(.22 * RATE)) / RATE
    x = band(rng.uniform(-1, 1, len(t)), 140, 1400) * np.exp(-t / .028) * (1 - np.exp(-t / .0008))
    x += np.sin(2 * np.pi * (95 + 12 * variant) * t) * np.exp(-t / .05) * .9
    x += band(rng.uniform(-1, 1, len(t)), 2000, 6000) * np.exp(-t / .005) * .5
    return normalise(np.tanh(x * 2), .75)


def landing():
    """Feet hitting the ground after a fall: a low thud and a little grit."""
    t = np.arange(int(.35 * RATE)) / RATE
    freq = 45 + 35 * np.exp(-t / .03)
    x = np.sin(2 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t / .08) * (1 - np.exp(-t / .002))
    x += scrape(t, .005, .14, 900, 4500, 801, .35) + thing(t, 0, 200, 1500, 120, .03, 802, .5)
    return normalise(x, .7)


def punch():
    """The low punch felt in the chest under the player's own shots (layered with the report)."""
    t = np.arange(int(.2 * RATE)) / RATE
    freq = 40 + 45 * np.exp(-t / .02)
    x = np.sin(2 * np.pi * np.cumsum(freq) / RATE) * np.exp(-t / .06) * (1 - np.exp(-t / .001))
    x += np.sin(2 * np.pi * 118 * t) * np.exp(-t / .025) * .35
    return normalise(np.tanh(x * 1.8), .9)


def tinnitus():
    """The ringing after a blast close by: a thin high tone, beating slightly, fading away."""
    t = np.arange(int(2.6 * RATE)) / RATE
    x = (np.sin(2 * np.pi * 3700 * t) + .6 * np.sin(2 * np.pi * 3711 * t)) * (1 - np.exp(-t / .06)) * np.exp(-t / .9)
    fade = int(.05 * RATE)
    x[-fade:] *= np.linspace(1, 0, fade)
    return normalise(x, .3)


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
    for name in RELOADS:
        out[f"reload_{name}.wav"] = reload(name)
    out["equip.wav"] = equip()
    for i in range(3):
        out[f"whiz_{i}.wav"] = whiz(i)
    for i in range(2):
        out[f"ricochet_{i}.wav"] = ricochet(i)
    for i in range(3):
        out[f"hurt_{i}.wav"] = hurt(i)
    out["armour_hit.wav"] = armour_hit()
    out["hitmarker.wav"] = hitmarker()
    out["pickup.wav"] = pickup()
    out["grenade_pin.wav"] = grenade_pin()
    out["zone_warning.wav"] = zone_warning()
    out["heartbeat.wav"] = heartbeat()
    out["siren.wav"] = siren()
    for i in range(2):
        out[f"crash_light_{i}.wav"] = crash(False, i)
        out[f"crash_heavy_{i}.wav"] = crash(True, i)
    for i in range(3):
        out[f"body_hit_{i}.wav"] = body_hit(i)
    out["land.wav"] = landing()
    out["punch.wav"] = punch()
    out["tinnitus.wav"] = tinnitus()
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
