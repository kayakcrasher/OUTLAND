#!/usr/bin/env python3
"""Original procedural sound-design sketches. No third-party recordings."""
import argparse
import math
from pathlib import Path
import random
import struct
import wave

RATE = 22050


def write_wav(path, samples):
    if not all(math.isfinite(s) and abs(s) <= 1 for s in samples):
        raise ValueError("Invalid or clipped PCM")
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(b"".join(struct.pack("<h",round(s*32767)) for s in samples))


def wind():
    # Periodic noise knots and integer-period gusts keep the loop continuous.
    rng = random.Random(1928)
    knots = [rng.uniform(-1,1) for _ in range(2048)]
    count = RATE*8
    samples = []
    for i in range(count):
        t = i/count
        phase = t*len(knots)
        k = int(phase)
        u = phase-k
        u = u*u*(3-2*u)
        noise = knots[k]*(1-u)+knots[(k+1)%len(knots)]*u
        gust = .62+.18*math.sin(t*2*math.pi)+.1*math.sin(t*6*math.pi)
        samples.append(noise*gust*.22 + .013*math.sin(t*74*math.pi))
    return samples


def footstep(surface, variant):
    # A soft footfall, not a hiss: a short low heel thud and a brief, heavily smoothed scuff
    # (gravel adds a few faint crunches). Quiet, and over in about a tenth of a second.
    rng = random.Random(930+variant+{"grass":0,"dirt":10,"gravel":20}[surface])
    count = round(RATE*(.11+variant*.008))
    smooth = smoother = 0
    samples = []
    crackles = [rng.randrange(round(count*.5)) for _ in range(5)]
    for i in range(count):
        t = i/RATE
        noise = rng.uniform(-1,1)
        smooth = smooth*.93+noise*.07
        smoother = smoother*.9+smooth*.1
        attack = min(1,t/.004)
        fade = (1-i/count)**2
        thump = math.sin(2*math.pi*(72+variant*6)*t)*math.exp(-t*55)
        scuff = smoother*6*math.exp(-t*40)*{"grass":.5,"dirt":.8,"gravel":.6}[surface]
        if surface=="gravel":
            scuff += sum(.12*smooth*math.exp(-(i-c)/40) for c in crackles if 0 <= i-c < 120)
        samples.append((thump+scuff)*attack*fade)
    peak = max(abs(v) for v in samples)
    return [v*.4/max(peak,.01) for v in samples]

def combat_sound(kind):
    rng = random.Random(101+sum(ord(c) for c in kind))
    duration = {"pistol":.28,"rifle":.32,"reload":.55,"target_hit":.12,"dry_fire":.07}[kind]
    count = round(RATE*duration)
    samples=[]
    low=0
    for i in range(count):
        t=i/RATE
        noise=rng.uniform(-1,1)
        low=.86*low+.14*noise
        if kind in ["pistol","rifle"]:
            # Blast transient, low body and a short diffuse outdoor tail.
            decay=35 if kind=="pistol" else 29
            tone=95 if kind=="pistol" else 75
            sample=noise*math.exp(-t*decay)*.5 + math.sin(2*math.pi*tone*t)*math.exp(-t*42)*.42
            sample+=low*math.exp(-t*12)*.22
        elif kind=="reload":
            sample=0
            for click in [.02,.18,.40]:
                age=t-click
                if age>=0: sample+=(noise*.6+math.sin(age*1800)*.2)*math.exp(-age*70)
        else:
            sample=(noise*.3+math.sin(t*(2300 if kind=="dry_fire" else 900))*.65)*math.exp(-t*65)
        sample*=min(1,t/.001)*(1-i/count)**2
        samples.append(sample)
    peak=max(abs(v) for v in samples)
    return [v*.82/peak for v in samples]


def generate(output):
    output.mkdir(parents=True,exist_ok=True)
    write_wav(output/"verda_wind.wav",wind())
    for surface in ["grass","dirt","gravel"]:
        for variant in range(3):
            write_wav(output/f"step_{surface}_{variant}.wav",footstep(surface,variant))
    # Gunshots and blasts are tools/generate_weapon_audio.py's (assets/audio/weapons).
    for kind in ["reload","target_hit","dry_fire"]:
        write_wav(output/f"{kind}.wav",combat_sound(kind))
    print(f"Generated wind, 9 footsteps and 3 combat sounds in {output}")


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,default=Path(__file__).resolve().parents[1]/"assets/audio/environment")
    generate(parser.parse_args().output)
