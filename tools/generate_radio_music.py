#!/usr/bin/env python3
"""Verda's radio: fourteen original instrumental songs and three station idents, made from scratch.

Every note is synthesised here - additive oscillators for reeds, brass, bowed and electric
instruments, Karplus-Strong strings for the banjo, guitars, mandolin and bass, noise for the drums -
from scores written in this file. No samples, no third-party recordings, nothing borrowed: the
melodies are composed from each song's chords and its seed (motif, answer, cadence), so they stay
the same every time the tool runs.

Writes assets/audio/radio/<station>_<n>.ogg and <station>_ident.ogg (mono, 32 kHz, Vorbis) and the
catalogue the game reads, include/outland/game/radio/RadioCatalog.hpp.

Needs numpy, scipy and ffmpeg (libvorbis).
"""
import argparse
import functools
import math
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np
from scipy import ndimage, signal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "audio" / "radio"
CATALOG = ROOT / "include" / "outland" / "game" / "radio" / "RadioCatalog.hpp"
RATE = 32000


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


# ------------------------------------------------------------------------------------- filters
def lowpass(x, cutoff, order=2):
    b, a = signal.butter(order, min(cutoff, RATE * .45) / (RATE / 2))
    return signal.lfilter(b, a, x)


def highpass(x, cutoff, order=2):
    b, a = signal.butter(order, cutoff / (RATE / 2), "high")
    return signal.lfilter(b, a, x)


def bandpass(x, lo, hi, order=2):
    b, a = signal.butter(order, [lo / (RATE / 2), min(hi, RATE * .45) / (RATE / 2)], "band")
    return signal.lfilter(b, a, x)


def peak(x, f, gain_db, q=1.0):
    """RBJ peaking EQ."""
    A = 10 ** (gain_db / 40)
    w = 2 * np.pi * f / RATE
    alpha = np.sin(w) / (2 * q)
    b = [1 + alpha * A, -2 * np.cos(w), 1 - alpha * A]
    a = [1 + alpha / A, -2 * np.cos(w), 1 - alpha / A]
    return signal.lfilter(b, a, x)


# ---------------------------------------------------------------------------------- generators
def times(n):
    return np.arange(n) / RATE


def envelope(n, attack=.005, decay=.1, sustain=1.0, release=.05, hold=None):
    """Attack, an exponential decay to the sustain level, held for `hold` seconds, then released."""
    t = times(n)
    hold = n / RATE - release if hold is None else hold
    e = np.where(t < attack, t / max(attack, 1e-5), sustain + (1 - sustain) * np.exp(-(t - attack) / max(decay, 1e-4)))
    return e * (1 - np.clip((t - hold) / max(release, 1e-4), 0, 1))


def phase(freq, n, vibrato=0.0, rate=5.5, delay=.15, slide_from=None, slide=.08):
    t = times(n)
    f = np.full(n, freq)
    if slide_from:
        f = freq * (slide_from / freq) ** np.clip(1 - t / slide, 0, 1)
    if vibrato:
        f = f * (1 + vibrato * np.clip((t - delay) / .25, 0, 1) * np.sin(2 * np.pi * rate * t))
    return 2 * np.pi * np.cumsum(f) / RATE


def additive(ph, freq, amps):
    out = np.zeros_like(ph)
    for k, a in enumerate(amps, 1):
        if k * freq > RATE * .45:
            break
        if a:
            out += a * np.sin(k * ph)
    return out


SAW = [1 / k for k in range(1, 41)]


def noise(n, seed):
    return np.random.default_rng(seed).uniform(-1, 1, n)


def ks(f, n, t60, bright, seed):
    """Karplus-Strong string at any pitch: run at a whole-sample period, then resample."""
    nf = RATE / f
    N = max(2, int(nf - .5))
    ratio = (N + .5) / nf
    m = int(n * ratio) + 2
    exc = noise(N, seed)
    for _ in range(int(round((1 - bright) * 8))):
        exc = .5 * (exc + np.roll(exc, 1))
    exc -= exc.mean()
    loss = 10 ** (-3 / (t60 * f))
    y = np.zeros(m + 1)
    y[1:N + 1] = exc
    k = N
    while k < m:
        e = min(k + N, m)
        y[1 + k:1 + e] = loss * .5 * (y[1 + k - N:1 + e - N] + y[k - N:e - N])
        k = e
    return np.interp(np.arange(n) * ratio, np.arange(m), y[1:m + 1])


def norm(x):
    p = np.max(np.abs(x))
    return x / p if p > 0 else x


# --------------------------------------------------------------------------------- instruments
# Each takes (midi, seconds held, variant) and returns a note at about unit peak. Cached: a song
# plays the same notes many times.
@functools.lru_cache(maxsize=None)
def accordion(m, dur, v):
    f, n = hz(m), int((dur + .08) * RATE)
    out = sum(additive(phase(f * (1 + d), n), f, [1, .7, .55, .4, .33, .25, .2, .16, .12, .1, .08, .06]) for d in (-.0028, 0, .0028))
    out = lowpass(out, 4200) * envelope(n, .018, .3, .85, .07, hold=dur)
    return norm(out * (1 + .04 * np.sin(2 * np.pi * .7 * times(n) + v)))


@functools.lru_cache(maxsize=None)
def clarinet(m, dur, v):
    f, n = hz(m), int((dur + .06) * RATE)
    out = additive(phase(f, n, vibrato=.004, rate=5), f, [1, .08, .45, .05, .3, .04, .18, .03, .1, .02, .06])
    breath = lowpass(noise(n, 70 + v), 2500) * .03
    return norm(lowpass(out + breath, 3500) * envelope(n, .03, .2, .9, .05, hold=dur))


@functools.lru_cache(maxsize=None)
def tuba(m, dur, v):
    f, n = hz(m), int((dur + .05) * RATE)
    out = additive(phase(f, n), f, [1, .6, .35, .2, .12, .07, .04])
    return norm(lowpass(out, 1200) * envelope(n, .025, .12, .55, .05, hold=dur))


@functools.lru_cache(maxsize=None)
def glock(m, dur, v):
    f, n = hz(m), int(1.2 * RATE)
    t = times(n)
    out = (np.sin(2 * np.pi * f * t) * np.exp(-t * 3) + .4 * np.sin(2 * np.pi * f * 2.76 * t) * np.exp(-t * 8)
           + .2 * np.sin(2 * np.pi * f * 5.4 * t) * np.exp(-t * 14))
    return norm(out * np.minimum(1, t / .002))


@functools.lru_cache(maxsize=None)
def pad(chord, dur):
    n = int((dur + .4) * RATE)
    out = sum(additive(phase(hz(m) * (1 + d), n), hz(m), SAW[:16]) for m in chord for d in (-.004, .004))
    return norm(lowpass(out, 1400) * envelope(n, .25, 1, 1, .4, hold=dur))


@functools.lru_cache(maxsize=None)
def banjo(m, dur, v):
    n = int(min(dur + .5, 1.2) * RATE)
    out = peak(highpass(ks(hz(m), n, .9, .92, 100 + v), 300), 1800, 6, 1.2)
    return norm(out * envelope(n, .001, 1, 1, .06))


@functools.lru_cache(maxsize=None)
def guitar(m, dur, v):
    n = int(min(dur + .8, 2.5) * RATE)
    out = highpass(peak(ks(hz(m), n, 2.2, .6, 200 + v), 220, 3, 1), 80)
    return norm(out * envelope(n, .001, 1, 1, .08))


@functools.lru_cache(maxsize=None)
def twang(m, dur, v):
    """A bright single-coil electric, a little overdriven: the country lead."""
    n = int((dur + .3) * RATE)
    out = ks(hz(m), n, 1.6, .95, 300 + v) * envelope(n, .001, 1, 1, .1, hold=dur + .2)
    out = np.tanh(out * 2.2) / np.tanh(2.2)
    return norm(peak(highpass(out, 180), 2500, 5, 1.2))


@functools.lru_cache(maxsize=None)
def mandolin(m, dur, v):
    n = int((dur + .1) * RATE)
    out = ks(hz(m), n, .7, .9, 400 + v) + ks(hz(m) * 1.002, n, .7, .9, 450 + v)
    return norm(highpass(out, 350) * envelope(n, .001, 1, 1, .05, hold=dur))


@functools.lru_cache(maxsize=None)
def chop(chord, v):
    n = int(.14 * RATE)
    out = sum(ks(hz(m), n, .25, .95, 500 + v + i) for i, m in enumerate(chord))
    return norm(bandpass(out, 600, 6000) * envelope(n, .001, .03, .2, .04, hold=.07))


@functools.lru_cache(maxsize=None)
def upright(m, dur, v):
    f, n = hz(m), int((dur + .25) * RATE)
    t = times(n)
    out = ks(f, n, 1.0, .3, 600 + v) * .7 + .6 * np.sin(2 * np.pi * f * t) * np.exp(-t * 2.5) * np.minimum(1, t / .006)
    return norm(lowpass(out, 1500) * envelope(n, .002, 1, 1, .12, hold=dur))


@functools.lru_cache(maxsize=None)
def fiddle(m, dur, v):
    f, n = hz(m), int((dur + .08) * RATE)
    slide = hz(m - 1) if v == 2 and dur > .35 else None
    out = additive(phase(f, n, vibrato=.006, rate=5.8, delay=.18, slide_from=slide), f, SAW[:30])
    out = out + lowpass(noise(n, 700 + v), 4000) * .04
    out = peak(peak(peak(out, 450, 5, 1.4), 1100, 4, 1.6), 2900, 7, 1.6)
    return norm(lowpass(highpass(out, 190), 6000) * envelope(n, .04, .2, .85, .06, hold=dur))


@functools.lru_cache(maxsize=None)
def steel(m, dur, v):
    f, n = hz(m), int((dur + .3) * RATE)
    slide = hz(m - 2) if v == 0 else None
    out = additive(phase(f, n, vibrato=.003, rate=4.5, delay=.25, slide_from=slide, slide=.18), f, [1, .5, .33, .2, .15, .1, .07, .05])
    return norm(lowpass(out, 3000) * envelope(n, .06, .8, .75, .25, hold=dur))


@functools.lru_cache(maxsize=None)
def power(m, dur, palm, v):
    """A distorted power chord (root, fifth, octave), open or palm-muted."""
    n = int((dur + (.05 if palm else .3)) * RATE)
    out = sum(additive(phase(hz(x) * (1 + d), n), hz(x), SAW[:25]) for x in (m, m + 7, m + 12) for d in (-.003, .003)) / 6
    out = highpass(out * envelope(n, .003, .12 if palm else .9, .25 if palm else .7, .04, hold=dur), 100)
    out = np.tanh(out * 9)
    out = lowpass(out, 2000 if palm else 4200, 4)
    out = peak(out, 800, -3, .8)
    if palm:
        out = peak(out, 130, 4, 1) * envelope(n, .001, .08, .2, .03, hold=dur)
    return norm(out)


@functools.lru_cache(maxsize=None)
def lead(m, dur, v, bend=False, drive=6.0):
    f, n = hz(m), int((dur + .15) * RATE)
    ph = phase(f, n, vibrato=.007 if dur > .4 else 0, rate=6, delay=.2, slide_from=hz(m - 2) if bend else None, slide=.12)
    out = additive(ph, f, SAW[:30]) * envelope(n, .004, .6, .6, .08, hold=dur)
    out = lowpass(np.tanh(highpass(out, 200) * drive), 3800, 4)
    return norm(peak(out, 1500, 4, 1))


@functools.lru_cache(maxsize=None)
def bass(m, dur, v):
    f, n = hz(m), int((dur + .05) * RATE)
    ph = phase(f, n)
    out = (lowpass(additive(ph, f, SAW[:20]), 900 + 300 * v) * .6 + np.sin(ph) * .6) * envelope(n, .004, .25, .7, .04, hold=dur)
    return norm(np.tanh(out * 1.5))


@functools.lru_cache(maxsize=None)
def synth_lead(m, dur, v):
    """An eighties synth lead: a detuned saw and a square, a little vibrato."""
    f, n = hz(m), int((dur + .12) * RATE)
    out = (additive(phase(f * 1.003, n, vibrato=.005, rate=5.5, delay=.25), f, SAW[:24]) * .6 +
           additive(phase(f * .997, n, vibrato=.005, rate=5.5, delay=.25), f, [1, 0, 1 / 3, 0, 1 / 5, 0, 1 / 7, 0, 1 / 9]) * .6)
    return norm(lowpass(out, 3200) * envelope(n, .01, .3, .75, .1, hold=dur))


@functools.lru_cache(maxsize=None)
def synth_bass(m, dur, v):
    f, n = hz(m), int((dur + .04) * RATE)
    ph = phase(f, n)
    out = lowpass(additive(ph, f, SAW[:20]), 700, 2) * .7 + np.sin(ph) * .5
    return norm(out * envelope(n, .003, .15, .6, .03, hold=dur))


@functools.lru_cache(maxsize=None)
def clean(m, dur, v):
    """A clean electric guitar through a chorus: two strings a hair apart."""
    n = int((dur + .6) * RATE)
    f = hz(m)
    out = ks(f, n, 1.4, .75, 1600 + v) + ks(f * 1.004, n, 1.4, .75, 1650 + v) * .7
    return norm(peak(highpass(out, 150), 2200, 3, 1) * envelope(n, .001, 1, 1, .1, hold=dur + .4))


@functools.lru_cache(maxsize=None)
def nylon(m, dur, v):
    n = int(min(dur + .9, 2.6) * RATE)
    out = ks(hz(m), n, 1.8, .42, 1700 + v)
    return norm(peak(highpass(out, 90), 260, 4, .9) * envelope(n, .002, 1, 1, .12))


@functools.lru_cache(maxsize=None)
def harmonica(m, dur, v):
    """A reed harp: a bright reed with a bend into some notes and a breath under it."""
    f, n = hz(m), int((dur + .08) * RATE)
    slide = hz(m - 1) if v == 1 and dur > .4 else None
    out = additive(phase(f, n, vibrato=.008, rate=5, delay=.2, slide_from=slide, slide=.12), f, [1, .6, .5, .35, .3, .2, .15, .1])
    out = out + lowpass(noise(n, 1800 + v), 3000) * .05
    return norm(bandpass(out, 350, 4200) * envelope(n, .03, .2, .85, .06, hold=dur))


@functools.lru_cache(maxsize=None)
def heavy(m, dur, palm, v):
    """A metal power chord: tighter and much more distorted than power()."""
    n = int((dur + (.04 if palm else .25)) * RATE)
    spread = .004 + .002 * (v % 3)
    out = sum(additive(phase(hz(x) * (1 + d), n), hz(x), SAW[:25]) for x in (m, m + 7, m + 12) for d in (-spread, spread)) / 6
    out = highpass(out * envelope(n, .002, .1 if palm else .8, .2 if palm else .75, .03, hold=dur), 90)
    out = np.tanh(out * 18)
    out = lowpass(out, 1500 if palm else 3800, 4)
    out = peak(peak(out, 700, -5, .8), 110, 5 if palm else 2, 1)
    if palm:
        out = out * envelope(n, .001, .06, .15, .02, hold=dur)
    return norm(out)


@functools.lru_cache(maxsize=None)
def gated_snare(v):
    """The eighties snare: a crack into a big room, cut off hard by a gate."""
    n = int(.42 * RATE)
    t = times(n)
    crack = snare(v)
    body = np.zeros(n)
    body[:len(crack)] += crack
    room = lowpass(bandpass(noise(n, 1900 + v), 600, 7000), 5000) * np.exp(-t * 3.5) * .7
    gate = np.clip((.30 - t) / .02, 0, 1)
    return norm((body + room) * gate)


@functools.lru_cache(maxsize=None)
def piano(m, dur, v):
    """An upright piano a little out of tune - two strings a few cents apart, a quick decay."""
    f, n = hz(m), int(min(dur + .7, 2.2) * RATE)
    t = times(n)
    out = np.zeros(n)
    for d in (-.0035, .0035):
        for k, a in enumerate([1, .55, .35, .22, .15, .1, .07], 1):
            fk = f * k * (1 + .0004 * k * k) * (1 + d)
            if fk > RATE * .45:
                break
            out += a * np.sin(2 * np.pi * fk * t) * np.exp(-t * (2.2 + k * .9))
    hammer = bandpass(noise(n, 2000 + v), 1500, 6000) * np.exp(-t * 80) * .15
    return norm((out + hammer) * np.minimum(1, t / .002) * envelope(n, .001, 1, 1, .08, hold=dur + .4))


# Drums (variant picks one of a few hits so a beat is not a loop of one sample).
@functools.lru_cache(maxsize=None)
def kick(v):
    n = int(.4 * RATE)
    t = times(n)
    f = 45 + 90 * np.exp(-t * 30)
    out = np.sin(2 * np.pi * np.cumsum(f) / RATE) * np.exp(-t * 7) + noise(n, 800 + v) * np.exp(-t * 300) * .3
    return norm(out)


@functools.lru_cache(maxsize=None)
def snare(v, tight=False):
    n = int(.3 * RATE)
    t = times(n)
    tone = np.sin(2 * np.pi * (220 if tight else 190) * t) * np.exp(-t * 20) * .5
    rattle = bandpass(noise(n, 900 + v), 1500, 9000) * np.exp(-t * (25 if tight else 14))
    return norm(tone + rattle)


@functools.lru_cache(maxsize=None)
def rim(v):
    n = int(.08 * RATE)
    t = times(n)
    return norm(np.sin(2 * np.pi * 1700 * t) * np.exp(-t * 90) + bandpass(noise(n, 1000 + v), 2000, 8000) * np.exp(-t * 120))


@functools.lru_cache(maxsize=None)
def brush(v):
    n = int(.35 * RATE)
    t = times(n)
    return norm(bandpass(noise(n, 1100 + v), 2000, 8000) * np.minimum(1, t / .02) * np.exp(-t * 9))


@functools.lru_cache(maxsize=None)
def hat(v, open_=False):
    n = int((.5 if open_ else .08) * RATE)
    t = times(n)
    return norm(highpass(noise(n, 1200 + v), 7000) * np.exp(-t * (8 if open_ else 60)))


@functools.lru_cache(maxsize=None)
def crash(v):
    n = int(2.4 * RATE)
    t = times(n)
    x = noise(n, 1300 + v)
    return norm((highpass(x, 4500) + .5 * bandpass(x, 3000, 9000)) * np.exp(-t * 1.6) * np.minimum(1, t / .002))


@functools.lru_cache(maxsize=None)
def clap(v):
    n = int(.25 * RATE)
    t = times(n)
    bursts = sum(np.where((t >= s) & (t < s + .006), 1.0, 0.0) for s in (0, .01, .02))
    return norm(bandpass(noise(n, 1400 + v), 1000, 5000) * (bursts + np.where(t >= .02, np.exp(-(t - .02) * 18), 0)))


@functools.lru_cache(maxsize=None)
def tom(f, v):
    n = int(.5 * RATE)
    t = times(n)
    sweep = f * (1 + .5 * np.exp(-t * 25))
    return norm(np.sin(2 * np.pi * np.cumsum(sweep) / RATE) * np.exp(-t * 7) + bandpass(noise(n, 1500 + v), 200, 3000) * np.exp(-t * 30) * .2)


# ----------------------------------------------------------------------------------- harmony
ROMAN = {"I": 0, "II": 2, "III": 4, "IV": 5, "V": 7, "VI": 9, "VII": 11}
SCALES = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
}


def parse_chord(sym):
    """'IV' -> (5, [0, 4, 7]); 'vi' minor; 'V7' dominant seventh; 'bVII' a semitone down."""
    flat = sym.startswith("b")
    s = sym[1:] if flat else sym
    seventh = s.endswith("7")
    s = s.rstrip("7")
    root = ROMAN[s.upper()] - (1 if flat else 0)
    minor = s != s.upper()
    return root, [0, 3 if minor else 4, 7] + ([10] if seventh else [])


class Song:
    """A song's clock, chords and the sections it plays, and the mix it is written into."""

    def __init__(self, spec):
        self.spec = spec
        self.key = spec["key"]
        self.scale = SCALES[spec.get("scale", "major")]
        self.spb = 60 / spec["tempo"]
        self.swing = spec.get("swing", .5)
        self.rng = np.random.default_rng(spec["seed"])
        self.sections = []
        bar = 0
        for kind, label, bars in spec["form"]:
            self.sections.append((kind, label, bar, bars))
            bar += bars
        self.bars = bar
        self.seconds = bar * 4 * self.spb + spec.get("tail", 3.0)
        self.n = int(self.seconds * RATE)
        self.buses = {}

    # time
    def time(self, beat):
        whole = math.floor(beat)
        frac = beat - whole
        frac = frac / .5 * self.swing if frac < .5 else self.swing + (frac - .5) / .5 * (1 - self.swing)
        return (whole + frac) * self.spb

    def add(self, bus, beat, x, gain, humanise=True):
        t = self.time(beat)
        if humanise and beat != math.floor(beat):
            t += self.rng.normal(0, .004)
        b = self.buses.setdefault(bus, np.zeros(self.n))
        i = max(0, int(round(t * RATE)))
        if i >= self.n:
            return
        j = min(self.n, i + len(x))
        b[i:j] += gain * x[:j - i]

    def secs(self, beats):
        return round(beats * self.spb, 3)

    # harmony
    def chords(self, label):
        return self.spec["progs"][label]

    def chord_at(self, label, bar, beat):
        sym = self.chords(label)[bar % len(self.chords(label))]
        parts = sym.split("|")
        return parse_chord(parts[min(len(parts) - 1, int(beat // (4 / len(parts))))])

    def pcs(self, chord):
        root, iv = chord
        return {(self.key + root + i) % 12 for i in iv}

    def voice(self, chord, low, high=None):
        """The chord's notes from `low` up, one of each (root position or the nearest inversion)."""
        root, iv = chord
        out = []
        for i in iv:
            m = self.key + root + i
            while m < low:
                m += 12
            while m >= low + 12:
                m -= 12
            out.append(m)
        return tuple(sorted(out))

    def root_note(self, chord, low):
        m = self.key + chord[0]
        while m < low:
            m += 12
        while m >= low + 12:
            m -= 12
        return m

    def degree(self, d):
        return self.key + 12 * (d // 7) + self.scale[d % 7]

    def nearest_degree(self, m):
        best = min(range(-30, 60), key=lambda d: abs(self.degree(d) - m))
        return best


# ---------------------------------------------------------------------------------- melodies
VOCAB = {
    # one-bar rhythms in beats; a negative length is a rest
    "polka": [[.5, .5, .5, .5, 1, 1], [1, .5, .5, 1, 1], [.25, .25, .5, .5, .5, 1, 1], [.5, .5, 1, .5, .5, 1], [1.5, .5, 1, 1]],
    "pop": [[1, 1, 2], [1.5, .5, 2], [.5, .5, 1, 2], [1, .5, .5, 1.5, -.5], [2, 1, 1]],
    "fiddle": [[.5] * 8, [.5, .5, .5, .5, 1, 1], [1, .5, .5, .5, .5, 1], [.5, .5, 1, .5, .5, 1], [.25, .25, .5, .5, .5, .5, .5, 1]],
    "country": [[1, .5, .5, 2], [1.5, .5, 1, 1], [2, 1, 1], [.5, .5, 1, 1, 1], [1, 1, 1.5, -.5]],
    "rock": [[.5, .5, 1, 1, 1], [1, .5, .5, 2], [.75, .75, .5, 2], [2, .5, .5, 1], [1.5, .5, 1.5, -.5]],
    "anthem": [[2, 1, 1], [1, 1, 2], [1.5, .5, 2], [3, 1], [.5, .5, 1, 2]],
    "ballad": [[2, 2], [1, 1, 2], [3, 1], [1.5, .5, 1, 1], [2, 1, -1]],
    "metal": [[1, 1, 1, 1], [.5, .5, 1, 2], [2, 1, 1], [1, .5, .5, 2]],
}
CADENCE = [[2, -2], [3, -1], [1, 1, 2], [4]]


def compose(song, label, vocab, low, high):
    """An eight-bar tune over a progression: a two-bar motif, the motif again on the new chords,
    an answer, the motif's first bar and a cadence home. Returns (beat, beats, midi) from bar 0."""
    rng = np.random.default_rng(song.spec["seed"] * 31 + sum(map(ord, label)))
    bars = len(song.chords(label))
    pick = lambda: [list(vocab[i]) for i in rng.choice(len(vocab), 2)]
    a, b = pick(), pick()
    cad = list(CADENCE[rng.integers(len(CADENCE))])
    plan = [a[0], a[1], a[0], a[1], b[0], b[1], a[0], cad]
    while len(plan) < bars:
        plan += plan[:bars - len(plan)]
    steps = {}

    def contour(key, count):
        if key not in steps:
            steps[key] = list(rng.choice([-2, -1, -1, 1, 1, 2, 0, -3, 3], count, p=[.1, .2, .1, .2, .15, .1, .05, .05, .05]))
        return steps[key]

    notes = []
    d = song.nearest_degree((low + high) // 2)
    for bar, rhythm in enumerate(plan[:bars]):
        key = ("a" if rhythm in a else "b" if rhythm in b else "c", bar % 2, len(rhythm))
        moves = contour(key, len(rhythm))
        beat = 0.0
        for i, length in enumerate(rhythm):
            if length < 0:
                beat += -length
                continue
            chord = song.chord_at(label, bar, beat)
            pcs = song.pcs(chord)
            step = int(moves[i])
            target = d + step
            # Turn back at the edges of the range rather than jump an octave.
            if song.degree(target) > high or song.degree(target) < low:
                target = d - step
            last = bar == bars - 1 and i == len([x for x in rhythm if x > 0]) - 1
            if beat == int(beat) or length >= 1 or last:
                wanted = {(song.key + chord[0]) % 12} if last else pcs
                options = [target + o for o in (0, -1, 1, -2, 2, -3, 3)]
                hits = [o for o in options if song.degree(o) % 12 in wanted]
                if hits:
                    target = hits[0]
            while song.degree(target) > high:
                target -= 1
            while song.degree(target) < low:
                target += 1
            notes.append((bar * 4 + beat, length, song.degree(target)))
            d = target
            beat += length
    return notes


def solo(song, label, low, high, minor=True):
    """A pentatonic guitar break over a progression: two-bar phrases of eighths and sixteenths,
    a bend into each phrase's long note, home to the chord root."""
    rng = np.random.default_rng(song.spec["seed"] * 7 + 3)
    pent = [0, 3, 5, 7, 10] if minor else [0, 2, 4, 7, 9]
    ladder = sorted({song.key + o + 12 * k for k in range(-2, 4) for o in pent if low <= song.key + o + 12 * k <= high})
    notes = []
    i = len(ladder) // 2
    bars = len(song.chords(label))
    for phrase in range(bars // 2):
        beat = phrase * 8.0
        end = beat + 7.0
        while beat < end:
            fast = rng.random() < .35
            run = 4 if fast else int(rng.integers(2, 5))
            length = .25 if fast else .5
            for _ in range(run):
                if beat >= end:
                    break
                i = int(np.clip(i + rng.choice([-2, -1, -1, 1, 1, 2]), 0, len(ladder) - 1))
                notes.append((beat, length, ladder[i], False))
                beat += length
            if rng.random() < .4 and beat < end:
                notes.append((beat, 1.0, ladder[i], True))
                beat += 1.0
        root = song.root_note(song.chord_at(label, phrase * 2 + 1, 3), low + 12)
        notes.append((phrase * 8.0 + 7, 1.0, root, rng.random() < .5))
    return notes


# -------------------------------------------------------------------------------- arrangement
def each_bar(song):
    """(section kind, label, bar index in the section, bars in the section, bar start beat, section number)."""
    for number, (kind, label, start, bars) in enumerate(song.sections):
        for i in range(bars):
            yield kind, label, i, bars, (start + i) * 4.0, number


def play(song, bus, inst, notes, start, gain, octave=0):
    for beat, length, m in notes:
        accent = 1.0 if beat % 2 == 0 else .88
        song.add(bus, start + beat, inst(m + 12 * octave, song.secs(length), int(song.rng.integers(3))), gain * accent * (1 + song.rng.uniform(-.08, .08)))


def arrange_polka(song):
    tunes = {label: compose(song, label, VOCAB["pop" if label == "B" else "polka"], song.key + 5, song.key + 24) for label in song.spec["progs"]}
    for kind, label, i, bars, start, number in each_bar(song):
        last_chorus = kind == "chorus" and number == len(song.sections) - 2
        if kind == "outro":
            five, one = parse_chord("V7"), parse_chord("I")
            if i == 0:
                for beat, chord in ((0, five), (1, five), (2, one)):
                    song.add("bass", start + beat, tuba(song.root_note(chord, song.key - 24), song.secs(.6), 0), .9)
                    for m in song.voice(chord, song.key - 5):
                        song.add("chords", start + beat, accordion(m, song.secs(.6 if beat < 2 else 2.5), 0), .3)
                    song.add("drums", start + beat, kick(0), .9)
                song.add("drums", start + 2, crash(0), .5)
            continue
        for beat in range(4):
            chord = song.chord_at(label, i, beat)
            low = song.key - 24
            root = song.root_note(chord, low)
            m = root if beat % 2 == 0 else (root + 7 if root + 7 < low + 12 else root - 5)
            song.add("bass", start + beat, tuba(m, song.secs(.4), beat % 3), .55 if beat % 2 == 0 else .45)
            for m in song.voice(chord, song.key - 5):
                song.add("chords", start + beat + .5, accordion(m, song.secs(.22), beat % 3), .3)
            song.add("drums", start + beat + .5, snare(beat % 3, True), .28)
            if kind != "intro":
                song.add("drums", start + beat, kick(beat % 3), .75)
                song.add("drums", start + beat + .25, hat(beat % 3), .08)
                song.add("drums", start + beat + .75, hat(beat % 3), .08)
            if kind == "chorus" and beat % 2 == 1:
                song.add("drums", start + beat, clap(beat % 3), .45)
        if kind == "chorus":
            chord = song.chord_at(label, i, 0)
            song.add("pad", start, pad(song.voice(chord, song.key), song.secs(4)), .22)
        if i == 0 and kind in ("chorus", "trio"):
            song.add("drums", start, crash(number % 3), .35)
        if kind == "intro":
            continue
        tune = [n for n in tunes[label] if i * 4 <= n[0] < i * 4 + 4]
        tune = [(b - i * 4, l, m) for b, l, m in tune]
        if kind == "trio":
            play(song, "lead", clarinet, tune, start, .55)
        else:
            play(song, "lead", accordion, tune, start, .5)
            if kind == "chorus":
                play(song, "lead", glock, tune, start, .2, octave=1)
            if last_chorus:
                play(song, "lead", clarinet, tune, start, .3, octave=-1)


def roll_notes(song, chord, melody=None):
    """A bar of banjo rolls (sixteenths): low root, fifth, high root or third, the drone; the tune's
    notes take their sixteenth when it plays one."""
    root, iv = chord
    low = song.root_note(chord, 50)
    drone = song.root_note((0, [0]), 62)
    strings = [low, low + 7, low + 12, low + 12 + iv[1], drone]
    patterns = ([0, 2, 4, 0, 2, 4, 0, 3], [1, 3, 4, 3, 0, 2, 4, 2], [0, 3, 4, 1, 2, 4, 0, 4])
    pattern = patterns[int(song.rng.integers(len(patterns)))]
    by_slot = {int(round(b * 4)): m for b, l, m in (melody or [])}
    out = []
    for slot in range(16):
        if slot in by_slot:
            out.append((slot / 4, .5, by_slot[slot], True))
        else:
            out.append((slot / 4, .25, strings[pattern[slot % 8]], False))
    return out


def arrange_bluegrass(song):
    tunes = {label: compose(song, label, VOCAB["fiddle"], song.key + 7, song.key + 26) for label in song.spec["progs"]}
    for kind, label, i, bars, start, number in each_bar(song):
        if kind == "outro":
            if i == 0:
                tag = [(0, 1, 12), (1, .5, 7), (1.5, .5, 7), (2, 1, 9), (3, 1, 7), (5, 1, 11), (6, 2, 12)]
                for b, l, o in tag:
                    song.add("lead", start + b, fiddle(song.key + o + 12, song.secs(l), 1), .55)
                    song.add("banjo", start + b, banjo(song.key + o + 12, song.secs(l), 1), .4)
                for b, sym in ((5, "V"), (6, "I")):
                    chord = parse_chord(sym)
                    song.add("bass", start + b, upright(song.root_note(chord, song.key - 24), song.secs(1.5 if b == 6 else .8), 0), .9)
                    for k, m in enumerate(song.voice(chord, song.key + 4)):
                        song.add("rhythm", start + b + k * .012 / song.spb, guitar(m, song.secs(2.5 if b == 6 else .5), k), .3)
            continue
        tune = [(b - i * 4, l, m) for b, l, m in tunes[label] if i * 4 <= b < i * 4 + 4]
        for beat in range(4):
            chord = song.chord_at(label, i, beat)
            low = song.key - 24 if song.key - 24 >= 36 else song.key - 12
            root = song.root_note(chord, low)
            if beat % 2 == 0:
                m = root if beat == 0 else (root + 7 if root + 7 < low + 12 else root - 5)
                nxt = song.chord_at(label, i + 1, 0)
                if beat == 2 and nxt != chord and song.rng.random() < .5:
                    target = song.root_note(nxt, low)
                    song.add("bass", start + 2, upright(m, song.secs(.9), 1), .45)
                    song.add("bass", start + 3, upright(target - 1 if song.rng.random() < .5 else target + 2, song.secs(.9), 2), .42)
                    continue
                song.add("bass", start + beat, upright(m, song.secs(1.8), beat), .5)
            else:
                voicing = song.voice(chord, song.key + 4)
                for k, m in enumerate(voicing):
                    song.add("rhythm", start + beat + k * .012 / song.spb, guitar(m, song.secs(.5), k), .34)
                song.add("rhythm", start + beat, chop(song.voice(chord, song.key + 14), beat), .3)
        chord = song.chord_at(label, i, 0)
        banjo_lead = kind == "banjo"
        rolls = roll_notes(song, chord, tune if banjo_lead else None)
        for b, l, m, hit in rolls:
            g = (.6 if hit else .34) if banjo_lead else .26
            song.add("banjo", start + b, banjo(m, song.secs(l), int(b * 4) % 3), g * (1.0 if b % 1 == 0 else .85))
        if kind == "intro":
            continue
        if kind == "mandolin":
            for b, l, m in tune:
                count = max(1, int(l / .25)) if l >= .75 else 1
                for k in range(count):
                    song.add("lead", start + b + k * .25, mandolin(m, song.secs(.25 if count > 1 else l), k % 3), .42 if k == 0 else .3)
        elif banjo_lead:
            song.add("pad", start, fiddle(song.voice(chord, song.key + 4)[1], song.secs(3.8), 1), .14)
        else:
            play(song, "lead", fiddle, tune, start, .5, octave=1 if kind == "fiddle_high" else 0)


def arrange_country(song):
    tunes = {label: compose(song, label, VOCAB["country"], song.key + 7, song.key + 24) for label in song.spec["progs"]}
    for kind, label, i, bars, start, number in each_bar(song):
        if kind == "outro":
            if i == 0:
                tag = [(0, 1, 12), (1, .5, 7), (1.5, .5, 7), (2, 1, 9), (3, 1, 7), (5, 1, 11), (6, 2, 12)]
                for b, l, o in tag:
                    song.add("lead", start + b, twang(song.key + o + 12, song.secs(l), 1), .5)
                for b, sym in ((5, "V"), (6, "I")):
                    chord = parse_chord(sym)
                    song.add("bass", start + b, bass(song.root_note(chord, song.key - 24), song.secs(1.5 if b == 6 else .8), 0), .8)
                    song.add("drums", start + b, kick(0), .8)
                    for k, m in enumerate(song.voice(chord, song.key + 4)):
                        song.add("rhythm", start + b + k * .012 / song.spb, guitar(m, song.secs(2.5 if b == 6 else .5), k), .3)
                song.add("drums", start + 6, crash(1), .35)
                song.add("pad", start + 6, steel(song.key + 16, song.secs(3), 0), .3)
            continue
        tune = [(b - i * 4, l, m) for b, l, m in tunes[label] if i * 4 <= b < i * 4 + 4]
        for beat in range(4):
            chord = song.chord_at(label, i, beat)
            low = song.key - 24 if song.key - 24 >= 36 else song.key - 12
            root = song.root_note(chord, low)
            if beat % 2 == 0:
                m = root if beat == 0 else (root + 7 if root + 7 < low + 12 else root - 5)
                song.add("bass", start + beat, bass(m, song.secs(.9), 0), .27)
                song.add("drums", start + beat, kick(beat), .8)
                for k, x in enumerate(song.voice(chord, song.key + 4)):
                    song.add("rhythm", start + beat + k * .015 / song.spb, guitar(x, song.secs(.9), k), .32)
            else:
                nxt = song.chord_at(label, i + 1, 0)
                if beat == 3 and nxt != chord:
                    target = song.root_note(nxt, low)
                    song.add("bass", start + 3, bass(target - 1 if target - 1 >= low else target + 2, song.secs(.9), 1), .24)
                hit = brush if kind != "chorus" else snare
                song.add("drums", start + beat, hit(beat) if hit is brush else snare(beat), .45 if hit is brush else .5)
                for k, x in enumerate(reversed(song.voice(chord, song.key + 4))):
                    song.add("rhythm", start + beat + .5 + k * .01 / song.spb, guitar(x, song.secs(.4), k), .18)
            song.add("drums", start + beat + .5, hat(beat), .1)
        if kind == "intro":
            song.add("pad", start, steel(song.voice(song.chord_at(label, i, 0), song.key + 4)[1], song.secs(3.6), i % 2), .3)
            continue
        if kind == "verse":
            play(song, "lead", twang, tune, start, .6)
            third = song.voice(song.chord_at(label, i, 0), song.key + 9)[1]
            song.add("pad", start, steel(third, song.secs(3.6), 1), .14)
        elif kind == "chorus":
            play(song, "lead", fiddle, tune, start, .6)
        else:
            play(song, "lead", steel, tune, start, .65)


def parse_riff(text):
    """'0m 0m 3 5 - . 7' - one token per eighth: a power chord's semitones over the key, m palm-muted,
    - holds the last one, . rests."""
    notes = []
    for k, tok in enumerate(text.replace("|", " ").split()):
        if tok == "-" and notes:
            notes[-1][1] += .5
        elif tok != ".":
            notes.append([k * .5, .5, int(tok.rstrip("m")), tok.endswith("m")])
    return notes


def drum_fill(song, start):
    for k in range(4):
        f = (180, 150, 120, 95)[k]
        song.add("drums", start + 3 + k * .25, tom(f, k) if k else snare(k), .6)


def arrange_rock(song):
    spec = song.spec
    chord_fn = heavy if spec.get("heavy") else power
    style = "anthem" if spec.get("anthem") else "rock"
    tunes = {label: compose(song, label, VOCAB[style], song.key + 7, song.key + 26) for label in spec["progs"]}
    riff = parse_riff(spec["riff"])
    lick = solo(song, "A", song.key + 7, song.key + 31, minor=not spec.get("anthem"))
    for kind, label, i, bars, start, number in each_bar(song):
        low = song.key - 24 if song.key - 24 >= 28 else song.key - 12
        if kind == "outro":
            if i == 0:
                for b, l, o, palm in riff:
                    if b < 4:
                        song.add("guitar", start + b, chord_fn(song.key - 12 + o, song.secs(l), palm, 0), .5)
                        song.add("bass", start + b, bass(low + o % 12, song.secs(l), 0), .6)
                for beat in range(4):
                    song.add("drums", start + beat, kick(beat), .8)
                    song.add("drums", start + beat + .5, hat(beat), .2)
                    if beat % 2:
                        song.add("drums", start + beat, snare(beat), .7)
            elif i == 1:
                song.add("guitar", start, chord_fn(song.key - 12, song.secs(4), False, 1), .6)
                song.add("bass", start, bass(low, song.secs(4), 0), .7)
                song.add("drums", start, crash(0), .55)
                song.add("drums", start, kick(0), .9)
                for k in range(16):
                    song.add("drums", start + k * .25, tom((180, 150, 120, 95)[k // 4], k) if k % 2 else snare(k), .3 + .03 * k)
            else:
                song.add("guitar", start, chord_fn(song.key - 12, song.secs(3), False, 2), .65)
                song.add("bass", start, bass(low, song.secs(3), 1), .75)
                song.add("drums", start, crash(1), .6)
                song.add("drums", start, kick(1), 1.0)
            continue
        riffing = kind in ("intro", "riff")
        if riffing:
            for b, l, o, palm in riff:
                if i % 2 * 4 <= b < i % 2 * 4 + 4:
                    song.add("guitar", start + b - i % 2 * 4, chord_fn(song.key - 12 + o, song.secs(l), palm, int(b) % 3), .5)
                    song.add("bass", start + b - i % 2 * 4, bass(low + o % 12, song.secs(l), int(b) % 2), .33)
        else:
            chorus = kind in ("chorus", "bridge")
            for b in [x * .5 for x in range(8)]:
                chord = song.chord_at(label, i, b)
                r = song.root_note(chord, low)
                song.add("bass", start + b, bass(r, song.secs(.45), int(b) % 2), .3 if b % 1 else .34)
            if chorus:
                for b, l in ((0, 1.5), (1.5, .5), (2, 1), (3, .5), (3.5, .5)):
                    chord = song.chord_at(label, i, b)
                    song.add("guitar", start + b, chord_fn(song.root_note(chord, max(38, song.key - 12)), song.secs(l), False, int(b * 2) % 3), .5)
            else:
                for b in [x * .5 for x in range(8)]:
                    chord = song.chord_at(label, i, b)
                    song.add("guitar", start + b, chord_fn(song.root_note(chord, max(38, song.key - 12)), song.secs(.45), True, int(b * 2) % 3), .55 if b % 1 == 0 else .45)
        if not (kind == "intro" and i < 2):
            for beat in range(4):
                if beat in (0, 2):
                    song.add("drums", start + beat, kick(beat), 1.0)
                if beat == 2:
                    song.add("drums", start + 2.5, kick(1), .75)
                if spec.get("heavy") and kind in ("chorus", "solo"):
                    for q in (.25, .5, .75):
                        song.add("drums", start + beat + q, kick(int(q * 4)), .5)
                if beat % 2:
                    song.add("drums", start + beat, snare(beat), .95)
                open_ = kind in ("chorus", "solo", "bridge")
                for half in (0, .5):
                    song.add("drums", start + beat + half, hat(beat, open_ and half == .5), .14 if open_ else .18)
            if i == 0 and kind != "intro":
                song.add("drums", start, crash(number % 3), .5)
            if i == bars - 1:
                drum_fill(song, start)
        if kind == "solo":
            notes = [(b - i * 4, l, m, bend) for b, l, m, bend in lick if i * 4 <= b < i * 4 + 4]
            for b, l, m, bend in notes:
                song.add("lead", start + b, lead(m, song.secs(l), int(b * 4) % 3, bend), .5)
        elif kind in ("verse", "chorus", "bridge"):
            tune = [(b - i * 4, l, m) for b, l, m in tunes[label] if i * 4 <= b < i * 4 + 4]
            octave = 0 if kind != "verse" else -1
            for b, l, m in tune:
                song.add("lead", start + b, lead(m + 12 * octave, song.secs(l), int(b * 2) % 3, l >= 1 and song.rng.random() < .3), .5 if kind != "verse" else .38)


def drums_rock(song, start, kind, i, bars, number, shuffle=False, double=False, gated=False):
    """A rock beat: kick on one and three, snare on two and four, eighths on the hat (triplets in a
    shuffle), a crash into each section and a fill out of it; double kick on demand."""
    for beat in range(4):
        if beat in (0, 2):
            song.add("drums", start + beat, kick(beat), 1.0)
        if beat % 2:
            song.add("drums", start + beat, gated_snare(beat) if gated else snare(beat), .9)
        steps = (0, 2 / 3) if shuffle else (0, .5)
        for k, half in enumerate(steps):
            song.add("drums", start + beat + half, hat(beat + k, kind == "chorus" and k == 1), .13)
        if double:
            for q in (.25, .5, .75):
                song.add("drums", start + beat + q, kick(int(q * 4)), .55)
    if i == 0 and kind not in ("intro",):
        song.add("drums", start, crash(number % 3), .5)
    if i == bars - 1:
        drum_fill(song, start)


def arrange_synthrock(song):
    """Eighties synth-rock with a shuffle: a pad, a chorused clean guitar picking the chords,
    pumping synth bass, a synth lead on the tune and a guitar break."""
    tunes = {label: compose(song, label, VOCAB["anthem"], song.key + 7, song.key + 24) for label in song.spec["progs"]}
    lick = solo(song, "A", song.key + 7, song.key + 26, minor=False)
    for kind, label, i, bars, start, number in each_bar(song):
        if kind == "outro":
            if i == 0:
                chord = song.chord_at("I", 0, 0)
                song.add("pad", start, pad(song.voice(chord, song.key), song.secs(6)), .3)
                song.add("bass", start, synth_bass(song.root_note(chord, song.key - 24), song.secs(4), 0), .4)
                song.add("drums", start, crash(0), .5)
                song.add("drums", start, kick(0), .9)
                for k, m in enumerate(song.voice(chord, song.key + 7)):
                    song.add("guitar", start + k * .5, clean(m, song.secs(3), k), .3)
            continue
        for half in (0, 2):
            chord = song.chord_at(label, i, half)
            song.add("pad", start + half, pad(song.voice(chord, song.key), song.secs(2)), .16)
            song.add("pad", start + half, pad(song.voice(chord, song.key + 12), song.secs(2)), .1)   # the bright upper layer
            notes = song.voice(chord, song.key + 7)
            order = [0, 1, 2, 1] if (number + i) % 2 else [0, 2, 1, 2]
            for k in range(4):
                song.add("guitar", start + half + k * .5, clean(notes[order[k]] + (12 if k == 3 else 0), song.secs(.5), k), .5)
            root = song.root_note(chord, song.key - 24 if song.key - 24 >= 33 else song.key - 12)
            for k in range(4):
                song.add("bass", start + half + k * .5, synth_bass(root + (12 if k % 2 else 0), song.secs(.4), k % 2), .36)
        if not (kind == "intro" and i < 2):
            drums_rock(song, start, kind, i, bars, number, shuffle=True, gated=True)
        if kind == "solo":
            for b, l, m, bend in [(b - i * 4, l, m, bend) for b, l, m, bend in lick if i * 4 <= b < i * 4 + 4]:
                song.add("lead", start + b, lead(m, song.secs(l), int(b * 4) % 3, bend, 4.0), .42)
        elif kind in ("verse", "chorus", "bridge"):
            tune = [(b - i * 4, l, m) for b, l, m in tunes[label] if i * 4 <= b < i * 4 + 4]
            play(song, "lead", synth_lead, tune, start, .45 if kind != "verse" else .38)
            if kind == "chorus":
                play(song, "lead", clean, tune, start, .18, octave=-1)


def arrange_pop(song):
    """Pop on the polka station: four on the floor, claps, an octave-pumping synth bass, the
    accordion on the verse tune and a synth lead with the glockenspiel on the chorus."""
    tunes = {label: compose(song, label, VOCAB["pop"], song.key + 5, song.key + 22) for label in song.spec["progs"]}
    for kind, label, i, bars, start, number in each_bar(song):
        if kind == "outro":
            if i == 0:
                chord = song.chord_at("I", 0, 0)
                song.add("pad", start, pad(song.voice(chord, song.key), song.secs(5)), .3)
                song.add("lead", start, glock(song.key + 24, 1.0, 0), .3)
                song.add("drums", start, crash(0), .45)
                song.add("drums", start, kick(0), .9)
                song.add("bass", start, synth_bass(song.root_note(chord, song.key - 24), song.secs(3), 0), .4)
            continue
        for beat in range(4):
            chord = song.chord_at(label, i, beat)
            root = song.root_note(chord, song.key - 24 if song.key - 24 >= 33 else song.key - 12)
            if kind != "intro" or i >= 2:
                song.add("drums", start + beat, kick(beat), .9)
                song.add("drums", start + beat + .5, hat(beat, beat % 2 == 1), .12)
                if beat % 2:
                    song.add("drums", start + beat, clap(beat), .55)
            for half in (0, .5):
                song.add("bass", start + beat + half, synth_bass(root + (12 if half else 0), song.secs(.4), int(half * 2)), .34)
            song.add("chords", start + beat + .5, accordion(song.voice(chord, song.key + 3)[0], song.secs(.2), beat % 3), .14)
        chord = song.chord_at(label, i, 0)
        song.add("pad", start, pad(song.voice(chord, song.key), song.secs(4)), .2 if kind == "chorus" else .12)
        if i == 0 and kind in ("chorus", "bridge"):
            song.add("drums", start, crash(number % 3), .4)
        if kind == "intro":
            continue
        tune = [(b - i * 4, l, m) for b, l, m in tunes[label] if i * 4 <= b < i * 4 + 4]
        if kind == "chorus":
            play(song, "lead", synth_lead, tune, start, .42)
            play(song, "lead", glock, tune, start, .2, octave=1)
        elif kind == "bridge":
            play(song, "lead", clarinet, tune, start, .5)
        else:
            play(song, "lead", accordion, tune, start, .48)


def arrange_honkytonk(song):
    """A honky-tonk shuffle: walking bass, a boom-chuck acoustic, snare on the backbeat, the twang
    guitar on the verses, the steel on the choruses and the fiddle on the bridge."""
    tunes = {label: compose(song, label, VOCAB["country"], song.key + 5, song.key + 22) for label in song.spec["progs"]}
    for kind, label, i, bars, start, number in each_bar(song):
        if kind == "outro":
            if i == 0:
                for b, l, o in [(0, .5, 12), (.5, .5, 9), (1, .5, 7), (1.5, .5, 4), (2, 2, 0)]:
                    song.add("lead", start + b, twang(song.key + o + 12, song.secs(l), 1), .5)
                chord = parse_chord("I")
                song.add("bass", start + 2, bass(song.root_note(chord, song.key - 24), song.secs(2), 0), .3)
                for k, m in enumerate(song.voice(chord, song.key + 4)):
                    song.add("rhythm", start + 2 + k * .012 / song.spb, guitar(m, song.secs(2.5), k), .3)
                song.add("pad", start + 2, steel(song.key + 16, song.secs(3), 0), .3)
                song.add("drums", start + 2, crash(1), .35)
            continue
        low = song.key - 24 if song.key - 24 >= 36 else song.key - 12
        for beat in range(4):
            chord = song.chord_at(label, i, beat)
            root = song.root_note(chord, low)
            walk = [0, chord[1][1], 7, 9][beat]
            song.add("bass", start + beat, bass(root + walk, song.secs(.8), beat % 2), .26)
            if beat % 2 == 0:
                song.add("drums", start + beat, kick(beat), .7)
            else:
                song.add("drums", start + beat, snare(beat, True), .5)
                for k, x in enumerate(song.voice(chord, song.key + 4)):
                    song.add("rhythm", start + beat + k * .012 / song.spb, guitar(x, song.secs(.4), k), .26)
            song.add("drums", start + beat + .5, hat(beat), .1)
            # The honky-tonk piano: chords on the offbeat, a high fill at the end of each bar.
            for m in song.voice(chord, song.key + 7):
                song.add("chords", start + beat + .5, piano(m, song.secs(.4), beat % 3), .17)
            if beat == 3:
                for k, o in enumerate((12, 16, 19, 24)):
                    song.add("chords", start + 3 + k * .125, piano(song.root_note(chord, song.key + 12) + o - 12, song.secs(.2), k % 3), .12)
        if kind == "intro":
            if i >= 2:
                play(song, "lead", twang, [(0, .5, song.key + 19), (.5, .5, song.key + 16), (1, 1, song.key + 12), (2, 2, song.key + 14)], start, .4)
            continue
        tune = [(b - i * 4, l, m) for b, l, m in tunes[label] if i * 4 <= b < i * 4 + 4]
        inst = {"verse": twang, "chorus": steel}.get(kind, fiddle)
        play(song, "lead", inst, tune, start, .6)


def arrange_ballad(song):
    """A laid-back country ballad: a fingerpicked nylon guitar, upright bass, brushes, the harp
    on the verses and a nylon guitar break."""
    tunes = {label: compose(song, label, VOCAB["ballad"], song.key + 7, song.key + 24) for label in song.spec["progs"]}
    for kind, label, i, bars, start, number in each_bar(song):
        if kind == "outro":
            if i == 0:
                chord = parse_chord("I")
                for k, m in enumerate(song.voice(chord, song.key + 4) + (song.key + 24,)):
                    song.add("rhythm", start + k * .25, nylon(m, song.secs(4), k), .32)
                song.add("bass", start, upright(song.root_note(chord, song.key - 24 if song.key - 24 >= 36 else song.key - 12), song.secs(3), 0), .5)
                song.add("lead", start + 1, harmonica(song.key + 24, song.secs(3), 1), .4)
            continue
        low = song.key - 24 if song.key - 24 >= 36 else song.key - 12
        for half in (0, 2):
            chord = song.chord_at(label, i, half)
            root = song.root_note(chord, low)
            song.add("bass", start + half, upright(root if half == 0 else root + 7 if root + 7 < low + 12 else root - 5, song.secs(1.8), half), .45)
            notes = song.voice(chord, song.key + 7)
            picks = [(0, root + 12), (.5, notes[1]), (1, notes[2]), (1.5, notes[1])]
            for b, m in picks:
                song.add("rhythm", start + half + b, nylon(m, song.secs(.9), int(b * 2)), .28 if b == 0 else .22)
            if kind != "intro":
                song.add("drums", start + half + 1, brush(half), .35)
        if kind == "intro":
            continue
        tune = [(b - i * 4, l, m) for b, l, m in tunes[label] if i * 4 <= b < i * 4 + 4]
        if kind == "solo":
            play(song, "lead", nylon, tune, start, .55, octave=1)
        elif kind == "chorus":
            play(song, "lead", harmonica, tune, start, .5)
            play(song, "lead", nylon, tune, start, .22)
        else:
            play(song, "lead", harmonica, tune, start, .45)


# ----------------------------------------------------------------------------------- mastering
BUS = {
    # gain, reverb send
    "lead": (1.0, .30), "chords": (.9, .2), "bass": (1.0, .02), "drums": (.9, .12), "pad": (.8, .4),
    "banjo": (1.0, .15), "rhythm": (1.0, .15), "guitar": (.9, .12),
}


def reverb_ir(seconds):
    n = int(seconds * RATE)
    t = times(n)
    ir = lowpass(noise(n, 4242), 5000) * np.exp(-t * 6.9 / seconds)
    ir[:int(.015 * RATE)] = 0
    return ir / np.sqrt(np.sum(ir ** 2))


def produce(song):
    """Era and genre production on the finished buses (the music itself is untouched)."""
    spec = song.spec
    if spec.get("heavy") and "guitar" in song.buses:
        # Double-tracked rhythm guitar: a second, darker pass a few milliseconds late.
        g = song.buses["guitar"]
        lag = int(.014 * RATE)
        second = np.zeros_like(g)
        second[lag:] = lowpass(g, 3000, 2)[:-lag]
        song.buses["guitar"] = g * .8 + second * .7
    if spec.get("pump"):
        # The pop pump: pad, chords and bass duck on every beat and swell back.
        beats = np.arange(song.n) / RATE / song.spb
        frac = beats - np.floor(beats)
        duck = 1 - .55 * np.exp(-frac * 7)
        for bus in ("pad", "bass", "chords"):
            if bus in song.buses:
                song.buses[bus] = song.buses[bus] * duck


def master(song, room):
    produce(song)
    dry = np.zeros(song.n)
    send = np.zeros(song.n)
    for name, x in song.buses.items():
        gain, wet = BUS[name]
        dry += gain * x
        send += gain * wet * x
    out = dry + .5 * signal.fftconvolve(send, reverb_ir(room))[:song.n]
    out = highpass(out, 35)
    if song.spec.get("warm"):
        out = lowpass(out, 6500, 2)   # a warm, old-tape top end
    # gentle compression on a 60 ms RMS level, then the level and a look-ahead peak limiter
    level = np.sqrt(np.maximum(lowpass(out ** 2, 8, 1), 1e-12))
    threshold = np.percentile(level, 70)
    out *= np.where(level > threshold, (threshold / level) ** .35, 1)
    out *= 10 ** (-16 / 20) / np.sqrt(np.mean(out ** 2))
    ceiling = 10 ** (-1 / 20)
    peaks = ndimage.maximum_filter1d(np.abs(out), int(.006 * RATE))
    gain = np.minimum(1, ceiling / np.maximum(peaks, 1e-9))
    gain = ndimage.minimum_filter1d(gain, int(.006 * RATE))
    b, a = signal.butter(1, 40 / (RATE / 2))
    gain = np.minimum(signal.filtfilt(b, a, gain), gain)
    out = np.clip(out * gain, -ceiling, ceiling)
    fade = int(.02 * RATE)
    out[:fade] *= np.linspace(0, 1, fade)
    out[-int(RATE * .5):] *= np.linspace(1, 0, int(RATE * .5))
    return out


# --------------------------------------------------------------------------------------- songs
def form(*parts):
    return list(parts)


STATIONS = [
    {
        "key": "popolko", "name": "Verda Popolko", "frequency": "98.1", "genre": "Pop polka", "town": "Verda City",
        "arrange": arrange_polka, "room": 1.1,
        "songs": [
            {"title": "Dancu sur la Placo", "key": 65, "tempo": 132, "seed": 11,
             "progs": {"I": ["I", "IV", "V7", "I"], "A": ["I", "IV", "V7", "I", "I", "IV", "V7", "I"],
                       "B": ["vi", "IV", "I", "V", "vi", "IV", "V7", "I"], "C": ["ii", "V7", "I", "vi", "ii", "V7", "I", "I"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("verse", "A", 8), ("chorus", "B", 8), ("chorus", "B", 8),
                          ("trio", "C", 8), ("verse", "A", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 2))},
            {"title": "Somera Akordiono", "key": 58, "tempo": 120, "seed": 23,
             "progs": {"I": ["I", "V", "IV", "I"], "A": ["I", "V", "vi", "IV", "I", "V", "IV", "I"],
                       "B": ["IV", "V", "iii", "vi", "ii", "V", "I", "I"], "C": ["vi", "vi", "IV", "IV", "I", "I", "V", "V7"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("verse", "A", 8), ("chorus", "B", 8),
                          ("trio", "C", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 2))},
            {"title": "Verda Koro", "key": 67, "tempo": 138, "seed": 37,
             "progs": {"I": ["vi", "III7", "vi", "vi"], "A": ["vi", "vi", "III7", "III7", "III7", "III7", "vi", "vi"],
                       "B": ["IV", "I", "V7", "I", "IV", "I", "V7", "I"], "C": ["I", "I", "IV", "IV", "V7", "V7", "I", "I"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("verse", "A", 8), ("chorus", "B", 8), ("chorus", "B", 8),
                          ("trio", "C", 8), ("verse", "A", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 2))},
            {"title": "Blua Floro", "key": 64, "tempo": 118, "seed": 113, "arrange": arrange_pop, "pump": True,
             "progs": {"I": ["I", "iii", "IV", "V"], "A": ["I", "iii", "IV", "V", "I", "iii", "ii", "V"],
                       "B": ["IV", "I", "V", "vi", "IV", "I", "ii", "V"], "C": ["vi", "iii", "IV", "I", "ii", "V", "I", "I"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("verse", "A", 8), ("chorus", "B", 8),
                          ("bridge", "C", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 2))},
        ],
    },
    {
        "key": "kantrio", "name": "Radio Sovaĝejo", "frequency": "102.5", "genre": "Country & bluegrass", "town": "Sovaĝejo",
        "arrange": arrange_bluegrass, "room": .7,
        "songs": [
            {"title": "Ferarbara Kuro", "key": 55, "tempo": 128, "seed": 41,
             "progs": {"I": ["I", "I", "V", "I"], "A": ["I", "I", "vi", "vi", "I", "I", "V", "I"],
                       "B": ["I", "I", "IV", "IV", "I", "I", "V", "I"]},
             "form": form(("intro", "I", 4), ("fiddle", "A", 8), ("fiddle", "A", 8), ("fiddle", "B", 8), ("mandolin", "B", 8),
                          ("banjo", "A", 8), ("mandolin", "B", 8), ("fiddle", "A", 8), ("fiddle_high", "A", 8), ("outro", "I", 2))},
            {"title": "Polvo sur la Vojo", "key": 62, "tempo": 104, "seed": 53, "swing": .58, "arrange": arrange_country,
             "progs": {"I": ["I", "IV", "V", "I"], "A": ["I", "I", "IV", "I", "I", "I", "V", "V"],
                       "B": ["IV", "IV", "I", "I", "V", "V", "I", "I"], "C": ["vi", "IV", "I", "V", "vi", "IV", "V", "V"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("verse", "A", 8), ("chorus", "B", 8), ("verse", "A", 8),
                          ("bridge", "C", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 2))},
            {"title": "Monta Violono", "key": 57, "tempo": 132, "seed": 67, "scale": "mixolydian",
             "progs": {"I": ["I", "bVII", "I", "I"], "A": ["I", "I", "bVII", "I", "I", "I", "V", "I"],
                       "B": ["IV", "IV", "I", "I", "bVII", "bVII", "I", "V"]},
             "form": form(("intro", "I", 4), ("fiddle", "A", 8), ("fiddle", "A", 8), ("mandolin", "B", 8), ("banjo", "A", 8),
                          ("fiddle", "B", 8), ("banjo", "B", 8), ("fiddle", "A", 8), ("fiddle_high", "A", 8), ("outro", "I", 2))},
            {"title": "Drinkejo ĉe la Vojo", "key": 64, "tempo": 132, "seed": 127, "swing": .62, "arrange": arrange_honkytonk,
             "progs": {"I": ["I", "I", "V", "I"], "A": ["I", "I", "IV", "I", "I", "I", "V", "V"],
                       "B": ["IV", "IV", "I", "I", "V", "IV", "I", "I"], "C": ["vi", "vi", "IV", "IV", "I", "I", "V", "V"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("verse", "A", 8), ("chorus", "B", 8), ("verse", "A", 8),
                          ("bridge", "C", 8), ("chorus", "B", 8), ("verse", "A", 8), ("chorus", "B", 8), ("outro", "I", 2))},
            {"title": "Malrapida Vespero", "key": 55, "tempo": 84, "seed": 131, "swing": .6, "arrange": arrange_ballad, "warm": True, "room": 1.2,
             "progs": {"I": ["I", "IV", "I", "V"], "A": ["I", "I", "IV", "I", "I", "vi", "ii", "V"],
                       "B": ["IV", "V", "I", "vi", "IV", "V", "I", "I"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("verse", "A", 8), ("solo", "A", 8),
                          ("chorus", "B", 8), ("outro", "I", 2))},
        ],
    },
    {
        "key": "roko", "name": "Espera Roko", "frequency": "105.9", "genre": "Rock", "town": "Espera",
        "arrange": arrange_rock, "room": .8,
        "songs": [
            {"title": "Espera Nokto", "key": 52, "tempo": 132, "seed": 71, "scale": "minor",
             "riff": "0m 0m 0m 3 5 - 0m 0m | 0m 0m 10 - 8 - 7 -",
             "progs": {"I": ["i", "i", "i", "i"], "A": ["i", "i", "bVI", "bVII", "i", "i", "bVI", "bVII"],
                       "B": ["bIII", "bVII", "iv", "bVI", "bIII", "bVII", "iv", "V"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("riff", "I", 4), ("verse", "A", 8),
                          ("chorus", "B", 8), ("solo", "A", 8), ("verse", "A", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 3))},
            {"title": "Fajro sur la Ringo", "key": 57, "tempo": 120, "seed": 83, "anthem": True,
             "riff": "0 - . 0 - 0 7 5 | 0 - . 0 - 0 10 -",
             "progs": {"I": ["I", "I", "I", "I"], "A": ["I", "V", "vi", "IV", "I", "V", "vi", "IV"],
                       "B": ["IV", "I", "V", "vi", "IV", "I", "V", "V"], "C": ["vi", "IV", "I", "V", "vi", "IV", "V", "V"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("verse", "A", 8), ("chorus", "B", 8),
                          ("bridge", "C", 8), ("solo", "A", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 3))},
            {"title": "Ŝtorma Bordo", "key": 50, "tempo": 144, "seed": 97, "scale": "mixolydian",
             "riff": "0m 0m 3 5 0m 0m 10 - | 0m 0m 3 5 7 - 5 -",
             "progs": {"I": ["I", "I", "I", "I"], "A": ["I", "I", "bVII", "IV", "I", "I", "bVII", "IV"],
                       "B": ["IV", "IV", "I", "I", "bVII", "bVII", "V", "V"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("riff", "I", 4), ("verse", "A", 8),
                          ("chorus", "B", 8), ("solo", "A", 8), ("riff", "I", 4), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 3))},
            {"title": "Vitraj Turoj", "key": 62, "tempo": 110, "seed": 139, "swing": .66, "arrange": arrange_synthrock,
             "progs": {"I": ["I", "IV", "I", "IV"], "A": ["I", "I", "IV", "vi", "I", "I", "IV", "V"],
                       "B": ["vi", "IV", "V", "I", "vi", "IV", "ii", "V"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("verse", "A", 8), ("chorus", "B", 8),
                          ("solo", "A", 8), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 2))},
            {"title": "Nigra Minejo", "key": 52, "tempo": 116, "seed": 149, "scale": "minor", "heavy": True,
             "riff": "0m 0m 0m 0m 1m 0m 0m 0m | 0m 0m 3 - 5 - 0m 1m",
             "progs": {"I": ["i", "i", "i", "i"], "A": ["i", "i", "bVI", "bVII", "i", "i", "iv", "v"],
                       "B": ["bVI", "bVII", "i", "i", "bVI", "bVII", "iv", "V"]},
             "form": form(("intro", "I", 4), ("verse", "A", 8), ("chorus", "B", 8), ("riff", "I", 4), ("verse", "A", 8),
                          ("chorus", "B", 8), ("solo", "A", 8), ("riff", "I", 4), ("chorus", "B", 8), ("chorus", "B", 8), ("outro", "I", 3))},
        ],
    },
]


def ident(station):
    """A few seconds of the station's sound between songs."""
    key = station["key"]
    if key == "popolko":
        song = Song({"key": 65, "tempo": 132, "seed": 5, "form": [("x", "I", 2)], "progs": {"I": ["I", "I"]}, "tail": 1.5})
        for k, o in enumerate((0, 4, 7, 12, 16)):
            song.add("lead", k * .5, accordion(65 + o, song.secs(.45 if k < 4 else 2), 0), .5)
        song.add("lead", 2, glock(65 + 24, 1.0, 0), .35)
        for beat in (0, 1, 2):
            song.add("bass", beat, tuba(41 + (0, 7, 0)[beat], song.secs(.4), 0), .8)
        song.add("drums", 2, crash(0), .3)
    elif key == "kantrio":
        song = Song({"key": 55, "tempo": 128, "seed": 6, "form": [("x", "I", 2)], "progs": {"I": ["I", "I"]}, "tail": 1.5})
        for b, l, m, hit in roll_notes(song, parse_chord("I")):
            if b < 3:
                song.add("banjo", b, banjo(m, song.secs(l), 0), .4)
        song.add("lead", 2, fiddle(67 + 12, song.secs(3), 2), .55)
        song.add("bass", 0, upright(43, song.secs(2), 0), .8)
        song.add("bass", 2, upright(43, song.secs(2), 1), .8)
    else:
        song = Song({"key": 52, "tempo": 132, "seed": 7, "form": [("x", "I", 2)], "progs": {"I": ["i", "i"]}, "tail": 1.5})
        for k in range(8):
            song.add("drums", k * .25, snare(k) if k % 2 == 0 else tom((180, 150, 120, 95)[k // 2], k), .4 + .05 * k)
        song.add("guitar", 2, power(40, song.secs(4), False, 0), .6)
        song.add("bass", 2, bass(28, song.secs(4), 0), .7)
        song.add("drums", 2, crash(0), .55)
        song.add("drums", 2, kick(0), .9)
        song.add("lead", 2.5, lead(64 + 12, song.secs(2.5), 0, True), .35)
    return master(song, station["room"])


# ------------------------------------------------------------------------------------- output
def encode(samples, path):
    with tempfile.TemporaryDirectory() as tmp:
        wav = Path(tmp) / "x.wav"
        with wave.open(str(wav), "wb") as f:
            f.setnchannels(1)
            f.setsampwidth(2)
            f.setframerate(RATE)
            f.writeframes((np.clip(samples, -1, 1) * 32767).astype("<i2").tobytes())
        path.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav), "-c:a", "libvorbis", "-q:a", "2",
                        "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", str(path)], check=True)


def cpp(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def catalog(entries):
    lines = [
        "#pragma once",
        "// Generated by tools/generate_radio_music.py - do not edit.",
        "#include <array>",
        "#include <span>",
        "namespace outland::game::radio {",
        "",
        "struct SongEntry { const char* title; const char* file; float seconds; };",
        "struct StationEntry {",
        "    const char* key; const char* name; const char* frequency; const char* genre; const char* town;",
        "    const char* ident; float ident_seconds;",
        "    std::span<const SongEntry> songs;   // the playlist, in order",
        "};",
        "",
    ]
    for st, ident_seconds, songs in entries:
        lines.append(f"inline constexpr std::array<SongEntry, {len(songs)}> {st['key']}_songs{{{{")
        for title, file, seconds in songs:
            lines.append(f'    {{"{cpp(title)}", "{file}", {seconds:.3f}F}},')
        lines.append("}};")
    lines += ["", "inline constexpr std::array<StationEntry, 3> stations{{"]
    for st, ident_seconds, songs in entries:
        lines.append(f'    {{"{st["key"]}", "{cpp(st["name"])}", "{st["frequency"]}", "{cpp(st["genre"])}", "{cpp(st["town"])}",')
        lines.append(f'     "assets/audio/radio/{st["key"]}_ident.ogg", {ident_seconds:.3f}F, {st["key"]}_songs}},')
    lines += ["}};", "", "} // namespace outland::game::radio", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--only", help="render just this station key")
    parser.add_argument("--wav", help="also write WAVs to this folder (to listen)")
    args = parser.parse_args()
    entries = []
    for st in STATIONS:
        songs = []
        for n, spec in enumerate(st["songs"], 1):
            file = f"assets/audio/radio/{st['key']}_{n}.ogg"
            song = Song(spec)
            if args.only and args.only != st["key"]:
                songs.append((spec["title"], file, song.seconds))
                continue
            (spec.get("arrange") or st["arrange"])(song)
            out = master(song, spec.get("room", st["room"]))
            encode(out, ROOT / file)
            if args.wav:
                Path(args.wav).mkdir(parents=True, exist_ok=True)
                with wave.open(str(Path(args.wav) / f"{st['key']}_{n}.wav"), "wb") as f:
                    f.setnchannels(1)
                    f.setsampwidth(2)
                    f.setframerate(RATE)
                    f.writeframes((np.clip(out, -1, 1) * 32767).astype("<i2").tobytes())
            songs.append((spec["title"], file, len(out) / RATE))
            print(f"{file}: {spec['title']} ({len(out) / RATE:.0f} s)")
        jingle = ident(st)
        if not args.only or args.only == st["key"]:
            encode(jingle, ROOT / f"assets/audio/radio/{st['key']}_ident.ogg")
        entries.append((st, len(jingle) / RATE, songs))
    CATALOG.parent.mkdir(parents=True, exist_ok=True)
    CATALOG.write_text(catalog(entries))
    print(f"wrote {CATALOG.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
