# Battle Royale combat AI

Battle Royale now drops **23 bots** onto Verda with you. They fight you and each other, and the last one standing wins.

## Playing

- On the home screen, the button next to **BATTLE ROYALE** cycles **BOTS: EASY / MEDIUM / HARD**.
- Bots land around the towns, at least 150 m from you and 60 m from each other. After the first 90 s they drift toward the capital, in a circle that tightens over time.
- **HUD:**
  - `ALIVE n   KILLS k   BOTS level` along the top, with a three-line kill feed underneath;
  - a red marker around the crosshair points toward whoever just shot you;
  - golden tracers show bot gunfire.
- You win when no bots are left ("LAST ONE STANDING - YOU WIN"). If you die, the screen shows `PLACED #n` and the feed names the bot that got you.

## How a bot thinks (`game::ai::CombatBot`)

**Perception: it only knows what it perceives.**
- **Sight:** a 120° field of view, out to 170 m, with a clear line of sight. Sight uses `CombatWorld`, the same traces as bullets, so walls block it and open windows don't. Anyone within 3 m is noticed whichever way the bot faces.
- **Hearing:**
  - rifle shots carry 300 m and pistol shots 180 m;
  - running footsteps carry 14 m;
  - a heard sound gives only an approximate position (about 12% of the distance off).
- **Memory:** a bot remembers where it last saw or heard each enemy for 20 s.
- **Being hit:** the bot turns toward where the shot came from.

**Decisions.** At each decision tick the bot scores these options and picks the best:
- **Engage** an enemy it can see and has reacted to.
- **Take cover:** it searches rings around itself for a point hidden from the threat, never closer to it.
- **Reload.**
- **Heal:** one medkit gives +45 over 4 s, and being shot interrupts it.
- **Push** to an enemy's last known position.
- **Flank** to a side point that has a line of sight to the enemy.
- **Retreat** when dying with no medkit.
- **Investigate** a sound.
- **Roam.**

**Acting.**
- **While engaged:** the bot strafes, holds its preferred range, and closes in when out of range.
- **Rifle bots** fire 3–5 round bursts; **pistol bots** fire paced single shots.
- **Aim** starts wide and settles while the bot tracks a target. Moving, its own or the target's, costs accuracy.

### Difficulty

Every level has the same health (100), weapons, ammunition, medkit, speed, view distance, field of view and hearing. Only thinking and shooting change.

| | Easy | Medium | Hard |
|---|---|---|---|
| Reaction (time of sight before acting) | 0.95 s | 0.55 s | 0.28 s |
| Aim error → settled floor | 6° → 1.5° | 3.5° → 0.9° | 2.2° → 0.45° |
| Decision interval | 0.65 s | 0.4 s | 0.22 s |
| Decision quality (chance of best option) | 62% | 80% | 95% |
| Aggression | 0.3 | 0.5 | 0.7 |
| Leads moving targets | no | no | yes |

Measured in tests:
- **First shot at an enemy in the open:** 0.96 s on Easy, 0.56 s on Medium, 0.30 s on Hard.
- **Hit rate at 40 m:** 10% on Easy, 28% on Medium, 54% on Hard.

## Code

- `include/outland/game/ai/CombatBot.hpp`, `src/game/ai/CombatBot.cpp`: the brain. It is headless and deterministic per seed. The world reaches it only through `BotEnvironment` (`line_of_sight`, `move`).
- `include/outland/game/ai/BattleRoyaleBots.hpp`, `src/game/ai/BattleRoyaleBots.cpp`: the match.
  - Handles the drop, the player as agent 0, gunshot and footstep sounds, the closing circle and the kill feed.
  - Resolves bot shots as hitscan against the world and vehicles (`CombatWorld`) and against body boxes. Headshots do ×2, as for the player.
- `NpcSystem` bot bodies (`spawn_bot` / `set_bot` / `find_bot` / `clear_bots`):
  - The system draws and animates the bodies, and lets the player's bullets hit them out to 320 m.
  - Damage is routed back to the bot by `Renderer`, so health is owned by the brain.
  - Bodies survive `reconcile` and are removed by `reset_session`.
- `Renderer`: starts the match on entering Battle Royale, updates and syncs it every frame, and draws the tracers and HUD.

## Validation

`outland_combat_bot_tests` covers:
- same body and senses at every level;
- no knowledge through walls;
- field of view;
- approximate hearing and turning to a gunshot;
- reaction-time ordering, with no shot before reacting;
- accuracy ordering, with no shots or hits through a wall;
- wounded bots under fire take cover;
- reload, heal, and damage interrupting the heal;
- push/flank, and retreat;
- bot-versus-bot duels;
- player damage and the kill feed;
- NpcSystem bodies;
- a deterministic 23-bot drop on the real island (on land, not inside geometry, away from the player).

DEV 41/41, release 34/34. A real match was played under Xvfb (software GL).

## Not done yet

- Navigation is straight-line with sliding and stuck recovery. There is no navmesh, so bots don't plan routes through buildings or up stairs.
- Bots don't loot: they start fully kitted and never pick up items.
- The circle steers bots but doesn't damage anyone outside it yet.
- No vehicles for bots, no crouch/prone, and no grenades.
- Bot bodies haven't been checked closely on screen. They use the same character renderer as NPCs.
