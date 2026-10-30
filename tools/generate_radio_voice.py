#!/usr/bin/env python3
"""Verda's radio voices: spoken station idents and the baseball news, in Esperanto.

Each station has its own voice, and an ident for each part of the day ("Bonan matenon! Vi
aŭskultas ..."), spoken over the station's jingle. Radio Sovaĝejo's announcer is from the forest
country: a slow Southern drawl (an American voice reading the Esperanto as it sounds, unhurried and
low). Between songs the stations read the latest result from La Verda Basballigo: "Sportaj novaĵoj!
Roka venkis, Espera perdis." - one clip for each pairing, in the newsreader's voice (in the drawl
on Radio Sovaĝejo).

Everything fits the radio's ident slot (VOICE_SLOT seconds), so the clock-driven schedule holds.

The voices are neural (Piper, https://github.com/rhasspy/piper): espeak-ng turns the Esperanto into
its sounds and a natural voice speaks them. Esperanto's sounds are close to Italian, Polish and
Czech, so the announcers are Italian, Polish and Czech voices; the drawl is an American voice.
The voice models are downloaded once into ~/.cache/outland/piper (not kept in the repository).

Writes assets/audio/radio/voice/*.ogg and include/outland/game/radio/RadioVoices.hpp.
Needs piper-tts (pip install piper-tts), numpy, scipy and ffmpeg (libvorbis).
"""
import argparse
import subprocess
import tempfile
import urllib.request
import wave
from pathlib import Path

import numpy as np
from scipy import signal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "audio" / "radio" / "voice"
HEADER = ROOT / "include" / "outland" / "game" / "radio" / "RadioVoices.hpp"
RATE = 32000
VOICE_SLOT = 7.5

# The stations (in RadioCatalog's order): how their announcer sounds, and what they say.
# voice: Piper voice; pace: how slowly it speaks (1 = the voice's own pace); drawl: read in the
# Southern voice.
STATIONS = [
    {"key": "popolko", "voice": "it_IT-paola-medium", "pace": 1.0, "drawl": False, "lines": {
        "morning": "Bonan matenon, Verdo! Vi aŭskultas Verda Popolko. Vekiĝu kaj dancu!",
        "day": "Vi aŭskultas Verda Popolko, naŭdek ok komo unu. Polko por la tuta insulo!",
        "evening": "Bonan vesperon! Vi aŭskultas Verda Popolko, naŭdek ok komo unu.",
        "night": "Ankoraŭ maldorma? Vi aŭskultas Verda Popolko, la tutan nokton.",
    }},
    {"key": "kantrio", "voice": "en_US-joe-medium", "pace": 1.15, "drawl": True, "lines": {
        "morning": "Bonan matenon, amikoj! Vi aŭskultas Radion Sovaĝejo.",
        "day": "Vi aŭskultas Radion Sovaĝejo, cent du komo kvin. Kantrio el la arbaro.",
        "evening": "Bonan vesperon el la arbaro. Vi aŭskultas Radion Sovaĝejo.",
        "night": "Estas malfrue, amikoj. Vi aŭskultas Radion Sovaĝejo.",
    }},
    {"key": "roko", "voice": "pl_PL-darkman-medium", "pace": .95, "drawl": False, "grit": True, "lines": {
        "morning": "Vekiĝu, Espera! Vi aŭskultas Espera Roko. Laŭte!",
        "day": "Vi aŭskultas Espera Roko, cent kvin komo naŭ. Laŭte!",
        "evening": "Bonan vesperon, rokuloj! Vi aŭskultas Espera Roko.",
        "night": "Noktaj rokuloj, vi aŭskultas Espera Roko, cent kvin komo naŭ.",
    }},
]
OCCASIONS = ["morning", "day", "evening", "night"]
# La Verda Basballigo's clubs (game::sports::teams, in order), as the announcers say them.
TEAMS = ["Verda Siti", "Espera", "Porto Luma", "Suda Haveno", "Roka"]
NEWSREADER = {"voice": "cs_CZ-jirka-medium", "pace": 1.0, "drawl": False}
DRAWL_NEWS = {"voice": "en_US-joe-medium", "pace": 1.15, "drawl": True}
VOICE_CACHE = Path.home() / ".cache" / "outland" / "piper"
VOICE_URL = "https://huggingface.co/rhasspy/piper-voices/resolve/main"
_voices = {}


def drawl(text):
    """Esperanto as an American reads it off the page, slowly: the stressed vowel drawn out."""
    pairs = {"aŭ": "ow", "eŭ": "eh-oo", "aj": "eye", "ej": "ay", "oj": "oy", "uj": "ooey"}
    single = {"a": "ah", "e": "eh", "i": "ee", "o": "oh", "u": "oo", "c": "ts", "ĉ": "ch", "ĝ": "j", "ĥ": "h",
              "ĵ": "zh", "ŝ": "sh", "j": "y", "ŭ": "w"}
    stretched = {"a": "aah", "e": "ehh", "i": "eee", "o": "ohh", "u": "ooo"}
    out = []
    for word in text.split(" "):
        tail = ""
        while word and not word[-1].isalpha():
            tail = word[-1] + tail
            word = word[:-1]
        low = word.lower()
        # The stress: the second-last vowel (a diphthong counts as one).
        vowels = [i for i, ch in enumerate(low) if ch in "aeiou"]
        stress = vowels[-2] if len(vowels) >= 2 else (vowels[0] if vowels else -1)
        i, spoken = 0, ""
        while i < len(low):
            two = low[i:i + 2]
            # (A diphthong only when the j or ŭ doesn't start the next syllable: "ĝe-jo", not "ĝej-o".)
            if two in pairs and (i + 2 >= len(low) or low[i + 2] not in "aeiou"):
                spoken += pairs[two] + ("h" if i == stress else "")
                i += 2
                continue
            ch = low[i]
            if i == stress and ch in stretched:
                spoken += stretched[ch]
            else:
                spoken += single.get(ch, ch)
            i += 1
        out.append(spoken + tail)
    return " ".join(out)


def piper_voice(name):
    """A Piper voice, downloaded the first time."""
    if name not in _voices:
        from piper import PiperVoice
        lang, rest = name.split("-", 1)
        speaker, quality = rest.rsplit("-", 1)
        VOICE_CACHE.mkdir(parents=True, exist_ok=True)
        for ext in ("onnx", "onnx.json"):
            path = VOICE_CACHE / f"{name}.{ext}"
            if not path.exists():
                url = f"{VOICE_URL}/{lang.split('_')[0]}/{lang}/{speaker}/{quality}/{name}.{ext}"
                print("downloading", url)
                urllib.request.urlretrieve(url, path)
        _voices[name] = PiperVoice.load(str(VOICE_CACHE / f"{name}.onnx"))
    return _voices[name]


def speak(text, voice, pace, is_drawl):
    """The line in a neural voice: Esperanto's sounds (or, for the drawl, the American reading)."""
    from piper import SynthesisConfig
    v = piper_voice(voice)
    if is_drawl:
        text = drawl(text)
    v.config.espeak_voice = "en-us" if is_drawl else "eo"
    pieces = []
    for chunk in v.synthesize(text, SynthesisConfig(length_scale=pace, noise_scale=.6, noise_w_scale=.7)):
        x = chunk.audio_float_array.astype(np.float64)
        loud = np.nonzero(np.abs(x) > .01)[0]
        if len(loud):
            pieces += [x[loud[0]:loud[-1] + 1], np.zeros(int(v.config.sample_rate * .28))]
    x = np.concatenate(pieces[:-1]) if pieces else np.zeros(1)
    return signal.resample_poly(x, RATE, v.config.sample_rate)


def on_air(x, grit=False):
    """A broadcast voice: the rumble cut, a little presence, gently compressed (a rock jock: a touch
    of grit). Kept clean enough to understand every word."""
    sos = signal.butter(2, [90, 9000], btype="bandpass", fs=RATE, output="sos")
    x = signal.sosfilt(sos, x)
    presence = signal.sosfilt(signal.butter(2, [2000, 5000], btype="bandpass", fs=RATE, output="sos"), x)
    x = x + .35 * presence
    x = x / (np.max(np.abs(x)) + 1e-9)
    drive = 1.8 if grit else 1.2
    return np.tanh(drive * x) / np.tanh(drive)


def decode(path):
    raw = subprocess.run(["ffmpeg", "-loglevel", "error", "-i", str(path), "-f", "f32le", "-ac", "1", "-ar", str(RATE), "-"],
                         check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype="<f4").astype(np.float64)


def chime():
    t = np.arange(int(RATE * .7)) / RATE
    bell = lambda f, at: np.where(t >= at, np.sin(2 * np.pi * f * (t - at)) * np.exp(-(t - at) * 6), 0)
    return .35 * (bell(880, 0) + bell(1318.5, .18) + .5 * bell(1760, .18))


def mix(voice, bed=None, start=.9):
    n = int(RATE * VOICE_SLOT)
    out = np.zeros(n)
    end = start + len(voice) / RATE
    if bed is not None:
        bed = bed[:n]
        t = np.arange(len(bed)) / RATE
        # The jingle under the voice, ducked while it talks.
        duck = np.where((t > start - .15) & (t < end + .1), .25, 1.0)
        duck = np.convolve(duck, np.ones(1600) / 1600, mode="same")
        out[:len(bed)] += bed * duck * .9
    a = int(start * RATE)
    out[a:a + len(voice)] += voice[:max(0, n - a)] * .85
    fade = int(RATE * .3)
    out[-fade:] *= np.linspace(1, 0, fade)
    return out / max(1.0, np.max(np.abs(out)) / .92)


def fit(text, spec, start, grit=False):
    """The voice, a little quicker if it would run past the slot."""
    pace = spec["pace"]
    for _ in range(8):
        voice = on_air(speak(text, spec["voice"], pace, spec["drawl"]), grit)
        if start + len(voice) / RATE <= VOICE_SLOT - .45:
            return voice
        pace *= .92
    raise SystemExit(f"too long for the slot: {text}")


def encode(samples, path):
    with tempfile.TemporaryDirectory() as tmp:
        wav = Path(tmp) / "x.wav"
        with wave.open(str(wav), "wb") as f:
            f.setnchannels(1)
            f.setsampwidth(2)
            f.setframerate(RATE)
            f.writeframes((np.clip(samples, -1, 1) * 32767).astype("<i2").tobytes())
        path.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(wav), "-ar", "22050", "-c:a", "libvorbis", "-q:a", "1",
                        "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", str(path)], check=True)


def header(idents, news):
    def files(names):
        return ", ".join(f'"{n}"' if n else '""' for n in names)
    lines = [
        "#pragma once",
        "// Generated by tools/generate_radio_voice.py - do not edit.",
        "#include <array>",
        "namespace outland::game::radio {",
        "",
        "// The ident slot: every spoken ident and news clip lasts this long (the schedule's gap between songs).",
        f"inline constexpr float voice_slot = {VOICE_SLOT:.2f}F;",
        "// When the announcer says it: morning, day, evening, night.",
        "inline constexpr int occasions = 4;",
        "// Each station's spoken idents, by occasion (stations in RadioCatalog's order).",
        f"inline constexpr std::array<std::array<const char*, occasions>, {len(idents)}> voice_idents{{{{",
    ]
    for row in idents:
        lines.append(f"    {{{{{files(row)}}}}},")
    lines += [
        "}};",
        "// The baseball news: \"<winner> venkis, <loser> perdis\", at [winner * 5 + loser] (game::sports::teams);",
        "// the newsreader's, and the Southern drawl's (Radio Sovaĝejo's).",
        f"inline constexpr std::array<const char*, {len(news[0])}> news_clips{{{{{files(news[0])}}}}};",
        f"inline constexpr std::array<const char*, {len(news[1])}> news_clips_drawl{{{{{files(news[1])}}}}};",
        f"inline constexpr std::array<bool, {len(STATIONS)}> drawl{{{{{', '.join('true' if s['drawl'] else 'false' for s in STATIONS)}}}}};",
        "",
        "} // namespace outland::game::radio",
        "",
    ]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--header-only", action="store_true", help="only rewrite RadioVoices.hpp")
    args = parser.parse_args()
    idents = []
    for st in STATIONS:
        row = []
        bed = None if args.header_only else decode(ROOT / "assets" / "audio" / "radio" / f"{st['key']}_ident.ogg")
        for occasion in OCCASIONS:
            name = f"assets/audio/radio/voice/{st['key']}_{occasion}.ogg"
            row.append(name)
            if not args.header_only:
                voice = fit(st["lines"][occasion], st, .9, st.get("grit", False))
                encode(mix(voice, bed, .9), ROOT / name)
                print("wrote", name)
        idents.append(row)
    news = [[], []]
    for k, spec in enumerate([NEWSREADER, DRAWL_NEWS]):
        for w in range(len(TEAMS)):
            for l in range(len(TEAMS)):
                if w == l:
                    news[k].append("")
                    continue
                name = f"assets/audio/radio/voice/news_{'drawl_' if k else ''}{w}{l}.ogg"
                news[k].append(name)
                if not args.header_only:
                    voice = fit(f"Sportaj novaĵoj! {TEAMS[w]} venkis, {TEAMS[l]} perdis.", spec, .75)
                    clip = mix(voice, None, .75)
                    c = chime()
                    clip[:len(c)] += c
                    encode(clip / max(1.0, np.max(np.abs(clip)) / .92), ROOT / name)
                    print("wrote", name)
    HEADER.write_text(header(idents, news))
    print("wrote", HEADER.relative_to(ROOT))


if __name__ == "__main__":
    main()
