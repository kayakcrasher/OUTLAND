# Gang wars: who runs Verda

A gangster's goal in Explore is to make their gang the biggest on the island, then the only one: every town in its colours and every ranching family riding with it or paying it. Any of the five gangs can be joined (see [careers](careers.md)); the rest fight back, and the Policio chases everyone.

Logic: `game::mafia::Territory` (turf, hold, families, tags). Tags are drawn by `game::mafia::GangLogos`. Wiring: `Renderer.cpp`.

## The five gangs

Each gang starts with every town in its county, its major city included. A crew is posted in each town's hideout.

| Gang | English | Colour | Home county | Logo |
|---|---|---|---|---|
| La Verda Mano | The Green Hand | green | Verdkerno (Verda City) | an open hand, LVM |
| Montaj Lupoj | Mountain Wolves | grey | Altverdo | a wolf's head, ML |
| La Ora Ankro | The Golden Anchor | gold | Lumamaro | an anchor, OA |
| Marĉaj Ranoj | Marsh Frogs | teal | Marĉfluo | a frog, MR |
| Feraj Hundoj | Iron Dogs | rust | Ferarbaro | a spiked collar, FH |

La Verda Mano, round the capital, is the hardest crew to fight.

## The ranching families

Each county has one ranch, and each ranch's family is a faction in its own right: Garza, Montoya, Villarreal, Cortez and Salazar. Their stance depends on the island's seed:

- **Allied** (two families): they ride with their county's gang. Their ranch counts for that gang. Lean on them and that gang comes after you too.
- **Fiercely independent** (two families): they fight before they pay.
- **Open** (one family): independent, but they will pay a gang that asks.

A ranch counts for a gang when the family is allied with it or paying it (**extorted**).

## Turf and hold

Every town and ranch is turf. A town has an owner and a **hold** from 0 to 100.

| What happens in a town | Hold |
|---|---|
| A rival kills one of the owner's crew | −7 |
| A rival sprays a tag | −12 |
| An AI gang raids it | −9 |
| The owner sprays a tag | +6 |
| Contraband delivered to the owner's hideout | +25 |
| Nobody contests it | +1 every 30 s |

When the hold runs out, the town goes to whichever gang pushed hardest. The hold restarts at 50, and the crew there turns to the new colours.

- **AI gangs** don't sit still. About every 2½ minutes one pushes into the nearest town held by someone else and sends a hit squad. Your gang's moves are yours to make.
- **Rival turf:** the 5 s warning near a rival hideout still applies (see careers).

## Spray tags

- Buy **spray paint** at general stores ($15), or find it in loot. Joining a gang gives 4 cans.
- Face a wall within about 4 m and press **T** (or the **TAG** touch button). One can paints your gang's logo, about 2 × 1 m.
- A tag paints over any tag within 2.5 m. Re-spraying your own tag changes nothing.
- A tag in your own town shores up the hold. A tag in a rival town wears its hold down and starts a gang war: that gang sends a hit squad after you.
- Each hideout starts with its gang's tag by the door. Tags are saved.

## Gang jobs

These join the hits, drive-bys and rig robberies in "the word":

| Job | What | Pay | Respect |
|---|---|---|---|
| **SMUGGLE** | Drive to a dock to pick up contraband off a boat, then drive it home to your hideout. Someone tips off the police at the pickup: you leave with 2 stars. A delivery adds 25 hold. | 1300 | 12 |
| **EXTORT** | Drive to a ranch and lean on the family. Open families pay. Fierce or allied ones fight: take out 2 of them and they pay. Either way, the ranch then counts for your gang. | 700 | 9 |
| **TAKE TURF** | Your rivals' weakest town: kill their crew and tag their walls until it falls to you. | 1500 | 15 |

## The gang map

For gang members, the map (**M**) becomes a gang map:

- Each turf is a circle in its holder's colour, with the holder's name and a hold bar.
- Tag dots show where the walls are painted.
- **WHO RUNS VERDA** ranks the gangs and families by towns held, then ranches.

A turf strip on the HUD shows the town you're in, who holds it and their hold. Gang news (flips, raids, gang wars) appears above the bottom panel.

## Winning

When every town is your gang's and every family rides with you or pays you, a gold **"<GANG> RULE VERDA"** banner goes up. Holding it is another matter: the police keep coming.

## Saving and dev

- Territory is saved beside the Explore profile (`<profile>.territory`): owners, holds, family stances and tags. It is saved every couple of seconds whenever something changes.
- Dev builds log the counts: `OUTLAND territory: N turfs, N tags, N docks`.
- `outland_territory_tests` covers hold, flips, tags painting over, extortion by stance, raids and the save. `outland_career_tests` covers the three jobs.
