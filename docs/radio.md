# Radio

Verda has three radio stations. Each one broadcasts from its own building in its own town, and
you hear them in any car you drive.

| Station | Frequency | Town | Music |
| --- | --- | --- | --- |
| **Verda Popolko** | 98.1 | Verda City | Pop polka: accordion, tuba oom-pah, clarinet, glockenspiel, a pop beat |
| **Radio Sovaĝejo** | 102.5 | Sovaĝejo | Country and bluegrass: fiddle, banjo rolls, mandolin, upright bass, steel guitar |
| **Espera Roko** | 105.9 | Espera | Rock: distorted guitar riffs, bass, drums, guitar solos |

Each station has three instrumental songs, with the station's ident (a few seconds of its sound)
between them:

- Verda Popolko: *Dancu sur la Placo*, *Somera Akordiono*, *Verda Koro*
- Radio Sovaĝejo: *Ferarbara Kuro* (a bluegrass breakdown), *Polvo sur la Vojo* (a country
  two-step), *Monta Violono*
- Espera Roko: *Espera Nokto*, *Fajro sur la Ringo*, *Ŝtorma Bordo*

## In the car

- The radio plays whenever you are at the wheel. It plays the last station you chose (Verda
  Popolko the first time).
- **O** (or the **RADIO** button at the wheel, left of HAND) turns the dial: the next station, then
  **RADIO OFF** after the last one, then the first station again. Turning it gives a burst of
  static. Your station is saved with the settings.
- The stations are live. Each one plays its ident and songs round and round, and what is on
  depends only on the clock. Tuning in drops you into whatever is playing, part-way through a
  song, and each session starts at a different point.
- A note under the compass names the station and the song. It shows for a few seconds when you get
  in, when you turn the dial and when a new song starts.
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
- The mix is mastered to a common loudness and written as mono 32 kHz Ogg Vorbis files
  (`assets/audio/radio/`, about 9 MB). The tool also writes the catalogue the game reads,
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
