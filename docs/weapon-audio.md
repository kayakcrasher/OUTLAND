# Weapon audio

All gunfire in OUTLAND is original synthesized audio: there are no recordings.
`tools/generate_weapon_audio.py` writes `assets/audio/weapons/` (28 mono 32 kHz WAVs, about
2 MB). CTest's `outland_generated_weapon_audio` fails if the committed files are stale.

## A report, in layers
Each gun's shot is mixed from:
- **crack:** the supersonic snap and muzzle pressure spike, a very short high burst;
- **boom:** the propellant blast, a low sine that drops in pitch with a sharp attack;
- **bark:** band-passed mid-range noise, the punch you feel in PUBG and Fortnite;
- **echoes:** reflections off buildings and hills at about 65, 140, 290 and 510 ms, each duller
  than the last;
- **tail:** the diffuse outdoor decay.

The mix is then soft-clipped for density and normalised.

Every gun has its own character:

| Gun | Sound |
|---|---|
| V9 pistol | Snappy, 0.55 s tail |
| VR30 rifle | Big crack and bark, 0.95 s |
| VK45 SMG | Thuddier, lower |
| VS12 shotgun | A deep boom, 1.1 s |
| VP7 sniper | A thunderclap, 1.5 s |

Each gun has three variants plus ±3% pitch, so automatic fire never repeats one sample.

## Other sounds
- **Distant versions:** muffled, with little crack and heavy echoes.
- **Grenade blast:** near and far versions.
- **Mechanisms:** the shotgun pump after each shot, the sniper bolt, and spent cases tinkling on
  the ground.
- **Kill confirmation:** a short ding.

## Playback (`engine::audio::GunAudio`)
- **Your shots** play at full weight, followed by the pump or bolt and the spent case.
- **Everyone else's shots** come from the sound bus: Battle Royale bots, police and anyone else
  who fires. How far a shot carries identifies the gun. `mix_shot` places each one:
  - volume falls off with distance;
  - beyond 35 m the distant mix is used;
  - it is panned to the side of your head it is on;
  - it arrives late by the speed of sound, 343 m/s (a sniper 300 m away is heard about 0.9 s
    after the flash).
- **Grenade blasts** are placed the same way.
- **Overlap:** every sample has six voices (raylib sound aliases), so bursts and their tails
  overlap.
- **Volume:** follows the audio setting and mute (N, `[` and `]`, or the map screen).

## Tracers
Bullets are no longer drawn in flight, and bots and police no longer draw tracer lines. Muzzle
flashes, impact puffs and the report show who is shooting and from where.

## The web build on iPhone
Safari only lets a page start sound from a touchend or click. The start button now resumes
audio after the game has created its audio device. Every touch, click or key press on the page
also resumes it, in case the first unlock was missed. The phone's silent switch still mutes
the web build.
