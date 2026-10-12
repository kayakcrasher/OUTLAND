# La Mafio - the Krimuloj of Verda

Explore only. Verda's five counties (zonoj) each have a gang, and every town has a crew of its
county's gang. The crew has taken over a building (a pub where there is one, else a works, a
garage or a shop) and stands about it armed and in plain sight. Their banner, a pole and a
striped flag in the gang's colour, flies by the door. The island map marks every hideout with a
diamond in the gang's colour.

| Zono | Gang | English | Level | Arms |
|---|---|---|---|---|
| Verdkerno | La Verda Mano | The Green Hand | Hard (the hardest) | rifles, SMG |
| Altverdo | Montaj Lupoj | Mountain Wolves | Medium | rifle, shotgun, pistol |
| Lumamaro | La Ora Ankro | The Golden Anchor | Medium | SMG, pistol, rifle |
| Marĉfluo | Marĉaj Ranoj | Marsh Frogs | Easy | shotgun, pistol, SMG |
| Ferarbaro | Feraj Hundoj | Iron Dogs | Medium | shotgun, rifle, pistol |

Crews are 4 strong (8 for the Green Hand in Verda City, 3 in the highway hamlets): 40 Krimuloj
on the island.

## Leave them be

The police are paid off. Nobody reports the Krimuloj, and the Krimuloj answer for themselves:
leave them alone and they leave you alone. Shoot at one (a round passing within a body's width
counts) or hurt one, and the whole gang turns on you "in self defence" for 90 seconds after the
last provocation. Shooting on a gang's turf, or in a fight with a gang, raises no wanted level.
If you die, the gangs forget.

A gang's rounds pass its own people, so a crew does not gun itself down in a crossfire.

## Gang wars

Every 2.5 to 5 minutes a crew near the player (within 450 m) is hit by the nearest rival crew.
Two Krimuloj (three from the Green Hand) take a parked car near their hideout and drive it by
road onto the other gang's turf. They get out there and fight it out, and the two gangs stay
at war for four minutes. The survivors go home. Any Krimulo killed respawns at the hideout 75
seconds later, out of the player's sight where possible.

## Poker at La Lasta Pramo (Verda City)

The Green Hand's crew in Verda City holds a pub with a poker table. Stand at the table and
press USE (E) to sit down with up to 300 kr against VITO, BRUNO and LA KALVA:

- Texas Hold'em, fixed limit: blinds 5/10, bets 10 before the turn and 20 after, four bets a
  street, with side pots for short stacks.
- FOLD (F), CHECK/CALL (C), RAISE/BET (R), DEAL/NEXT HAND (N or Enter), LEAVE (L or Escape).
- The Krimuloj estimate their equity against the hands still in and compare it with the pot
  odds, with the odd bluff. A Krimulo who goes broke buys back in.
- Your wallet changes only when you leave, by what you won or lost. If the game closes at the
  table, nothing is lost. The game pauses while you play.
- The table breaks up if the Green Hand turns on you.

## Code

- `game::mafia::Mafio` (`src/game/mafia/Mafio.cpp`): the gangs, crews, hostility, provocation,
  respawns, sleep (crews over 420 m from the player stand still) and wars. The crews are
  CombatBots in the shared `ai::BattleRoyaleBots` (same bodies, bullets and drivers as Battle
  Royale), with `BotEnvironment::hostile` deciding who fights whom.
- `ai::BotDrivers::dispatch`: a hit squad (driver and passengers) to a goal by road.
- `game::casino::Poker` (pure rules) and `game::casino::PokerTable` (screen and input).
- `game::mafia::draw_turf`: banners and the poker table in the world.
- Tests: `outland_mafio_tests` (a crew in every town, five gangs, peace, self defence,
  friendly fire, respawn, a hit squad driving onto rival turf and the fight) and
  `outland_poker_tests` (hand ranking, equity, betting, side pots, chips conserved).
