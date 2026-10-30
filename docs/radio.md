# Radio

Verda has three radio stations. Each one broadcasts from its own building in its own town, and
you hear them in any car you drive.

| Station | Frequency | Town | Music |
| --- | --- | --- | --- |
| **Verda Popolko** | 98.1 | Verda City | Pop polka: accordion, tuba oom-pah, clarinet, glockenspiel, a pop beat |
| **Radio Sovaĝejo** | 102.5 | Sovaĝejo | Country and bluegrass: fiddle, banjo rolls, mandolin, upright bass, steel guitar |
| **Espera Roko** | 105.9 | Espera | Rock: distorted guitar riffs, bass, drums, guitar solos |

Each station plays four or five instrumental songs, with the station's ident (a few seconds of
its sound) between them. Every song is an original composition:

- Verda Popolko: *Dancu sur la Placo*, *Somera Akordiono*, *Verda Koro*, and the hit *Blua Floro*
  (upbeat pop: four on the floor, synth bass, accordion verses, a synth and glockenspiel chorus).
- Radio Sovaĝejo: *Ferarbara Kuro* (a bluegrass breakdown), *Polvo sur la Vojo* (a country
  two-step), *Monta Violono*, and two hits: *Drinkejo ĉe la Vojo* (a honky-tonk shuffle with
  walking bass, twang guitar and steel) and *Malrapida Vespero* (a laid-back ballad on
  fingerpicked nylon guitar and harmonica).
- Espera Roko: *Espera Nokto*, *Fajro sur la Ringo*, *Ŝtorma Bordo*, and two hits: *Vitraj Turoj*
  (eighties synth-rock with a shuffle, chorused guitar and a synth lead) and *Nigra Minejo* (heavy
  metal: chugging palm-muted riffs, double kick, a guitar solo).

## The voices

Between the songs the stations talk (`tools/generate_radio_voice.py`). The voices are neural
([Piper](https://github.com/rhasspy/piper)), not the old robot voice: espeak-ng works out how the
Esperanto sounds and a natural voice speaks it. Esperanto sounds much like Italian, Polish and Czech,
so the announcers are Italian, Polish and Czech voices: Verda Popolko's is Italian (Paola), Espera
Roko's a deep Polish voice (Darkman) with a touch of grit, the newsreader Czech (Jirka). Radio
Sovaĝejo's is American (Joe). The broadcast processing is light, so every word stays clear. (Checked
with an Esperanto speech recogniser: it now gets most of the words, against well under half before.)
The voice models download once into `~/.cache/outland/piper` when the tool runs; only the finished
clips are kept in the repository.

- **Spoken idents**, in Esperanto, over the station's jingle - a different one for the morning
  (5-11), the day, the evening (17-22) and the night: "Bonan matenon, Verdo! Vi aŭskultas Verda
  Popolko. Vekiĝu kaj dancu!", "Noktaj rokuloj, vi aŭskultas Espera Roko...". Each station has its
  own announcer: a bright woman on Verda Popolko, a gravelly rock jock on Espera Roko.
- **Radio Sovaĝejo drawls.** Its announcer is from the forest country: an American voice reading
  the Esperanto as it sounds, slow and low, the stressed vowels drawn out.
- **The sports news.** Every other break, once La Verda Basballigo has a result, the station reads
  it: "Sportaj novaĵoj! Roka venkis, Espera perdis." There is a clip for every pairing of the five
  clubs, in the newsreader's voice (drawled on Radio Sovaĝejo), after a two-note chime.

Every clip is exactly as long as the break between songs (7.5 s), so the clock-driven schedule
holds. What a break says is chosen as it starts: the hour turning half-way through doesn't swap
it. The note beside the speedometer reads "Sportaj novaĵoj" during the news.

## In the car

- The radio plays whenever you are at the wheel. It plays the last station you chose (Verda
  Popolko the first time).
- **O** (or the **RADIO** button at the wheel, left of HAND) turns the dial: the next station, then
  **RADIO OFF** after the last one, then the first station again. Turning it gives a burst of
  static. Your station is saved with the settings.
- The stations are live. Each one plays its ident and songs round and round, and what is on
  depends only on the clock. Tuning in drops you into whatever is playing, part-way through a
  song, and each session starts at a different point.
- A small note beside the speedometer names the station and the song, out of the way of the
  road. It shows for a few seconds when you get in, when you turn the dial and when a new song
  starts.
- The radio pauses with the game (the pause menu, the bag) and stops when you get out.
- SETTINGS > **Car radio** sets its volume, on top of the main Volume. Mute (N) silences it too.

## The stations

The studios are modular buildings from the city kit (`civic/radio_station_<station>.glb`). Each
is a one-storey studio you can walk into, with a glass door, a long studio window, a red ON AIR
lamp over the door, a mixing desk facing the studio wall, and a sign band in the station's
colour. Beside it stands a red-and-white lattice transmitter mast with a warning lamp on top.
The modules differ by station:

- **Verda Popolko:** cream walls and green trim, a 30 m mast and a satellite dish. It is on a
  street on the west side of Verda City, by Okcidenta Verda.
- **Radio Sovaĝejo:** timber walls with a porch on posts and a 24 m mast. It is on Sovaĝejo's
  dirt road, among the homesteads.
- **Espera Roko:** black walls and red trim, a 36 m mast and two dishes. It is on the edge of
  Espera.

Each studio is an Office place, so staff work there. Its sign is in Esperanto:

- VERDA POPOLKO 98.1 · RADIO · POPMUZIKO KAJ POLKO
- RADIO SOVAĜEJO 102.5 · KANTRIO · BLUGRASO
- ESPERA ROKO 105.9 · RADIO · ROKMUZIKO

## Making the music

Every note is synthesised from scratch by `tools/generate_radio_music.py`. There are no samples
and no recordings:

- Additive oscillators make the reeds, brass, fiddle, steel guitar and electric guitars.
- Karplus-Strong strings make the banjo, acoustic guitar, mandolin and bass.
- Noise makes the drums.
- Each song's melodies are composed from its chords and a seed: a two-bar motif, the motif again
  on the next chords, an answer, and a cadence home. The same seed gives the same song every
  time.
- The hits get era and genre production on top: a gated-reverb snare and a bright pad layer on
  *Vitraj Turoj*, double-tracked guitars on *Nigra Minejo*, a honky-tonk piano on *Drinkejo ĉe la
  Vojo*, a pumping pop mix on *Blua Floro*, and a warm, roomy mix on *Malrapida Vespero*. The
  melodies, chords and riffs are all original.
- The mix is mastered to a common loudness and written as mono 32 kHz Ogg Vorbis files
  (`assets/audio/radio/`, about 14 MB). The tool also writes the catalogue the game reads,
  `include/outland/game/radio/RadioCatalog.hpp`.

To regenerate the music (needs numpy, scipy and ffmpeg with libvorbis):

```
python3 tools/generate_radio_music.py            # all stations
python3 tools/generate_radio_music.py --only roko --wav /tmp/radio   # one station, plus WAVs to listen to
```

## Code

- `game/radio/Radio`: the stations, what is on air at any time, and the car radio (its dial and
  the now-playing note). Tested by `tests/unit/radio/test_radio.cpp`.
- `game/radio/RadioAudio`: streams the song on air from the right point, keeps it in step with
  the station's clock, and plays the static.
- The world kit (`radio_stations` in `src/dev/VerdaWorldKit.cpp`) places the three studios. The
  signs are in `game/culture/Signage`.
