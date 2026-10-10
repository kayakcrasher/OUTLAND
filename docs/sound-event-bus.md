# Shared sound-event bus

Everything on Verda that makes noise emits a sound event onto one bus (`game::sound::SoundBus`). Everything that listens reads only the events it hasn't seen yet. Each listener decides for itself whether it was in earshot, so nobody learns a position they couldn't have heard.

## Events

| Kind | Who emits | Radius |
|---|---|---|
| Gunshot | player, combat bots | rifle 300 m, pistol 180 m |
| Footstep | sprinting player (>7 m/s), running bots | 14 m |
| Engine | the car you drive, above 2 m/s | 40 m × speed factor (0.5–2) |
| Horn | the car you drive (SPRINT) | 120 m |
| Impact, Shout | reserved for zombies, witnesses and police | 25 m / 45 m |

- Each event carries a kind, a position, a radius, a source and a time:
  - the source is 0 for the player, 1 and up for bots, and negative for anonymous sources;
  - the time comes from the session clock, which stops while the game is paused.
- Events last 1 s. A serial number lets every listener consume each event exactly once.

## Listeners

- **Combat bots** (`BattleRoyaleBots`, via `BotWorld::sounds`) hear everything on the bus.
  - A sound from a known source gives an approximate contact.
  - An anonymous sound sends the bot to investigate.
  - Bots put their own gunfire and running footsteps onto the same bus.
  - Without a shared bus (unit tests), a match uses a private one.
- **NPCs** (`NpcSystem`, via `NpcContext::sounds`):
  - Hostiles and creatures go to search wherever they heard something.
  - Civilians flee gunshots within half the shot's audible range (150 m for a rifle).
  - Combat bot bodies are skipped, since the bots have their own brains.
- **Residents** (`LifeSimulation::hear`): every gunshot on the bus, from anyone, sends nearby residents home to shelter.

## Next consumers

- **Witnesses and police:** a witness hears a gunshot, then reports it (a Shout) and calls the police.
- **Zombie ecology:** zombies are attracted to engines, gunshots and impacts.
- **The audio mixer:** it could play distant gunfire from the same events.

Tests: `outland_sound_bus_tests`.
