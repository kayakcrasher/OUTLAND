# The Vakeroj: ranches, cattle and big rigs

*Vakero* is Esperanto for cowboy (plural *vakeroj*), in keeping with the island's Esperanto names.

The Vakeroj are Verda's ranching families. There are five ranches, one in each county, and each
family grows crops and raises beef cattle. The ranchers ship their cattle from Porto Luma: a cattle
truck takes them from the ranches to the cattle quay at the cargo terminal, where they go aboard
the ship, and each ranch is paid for its own. Big rigs supply the towns too, and Verda Transporto
(see [trucking.md](trucking.md)) runs more of its own.

Code: `world/Ranches.hpp` (sites and layout, the stockyard's too), `dev::build_verda_towns` (`ranches()` lays them
out), `game::ranch::Ranches` (families, herds, side-by-sides) and `game::freight::Freight` (the
rigs). Models: `tools/generate_ranch_models.py`. Tests: `outland_ranch_tests`.

## The ranches
| Ranch | Family | County | Where |
|---|---|---|---|
| Ranĉo Verdaj Kampoj | Garza | Verdkerno | off the north highway, out of Verda City |
| Ranĉo Montaro | Montoya | Altverdo | north ring road |
| Ranĉo Sunleviĝo | Villarreal | Lumamaro | east highway to Porto Luma |
| Ranĉo Kanoj | Cortez | Marĉfluo | south ring road |
| Ranĉo Pinarbo | Salazar | Ferarbaro | west ring road |

Each ranch has a farm track off the highway to the yard, a two-storey farmhouse (home), a red
gambrel barn (open at both ends, with a hayloft, stalls and hay), two grain bins, a windmill
over a stock tank, a fenced pasture with a feed bunk and troughs, and a corral behind the
pasture with a loading chute. The cattle truck never stops on the highway: a dirt **cattle
track** leaves the farm track at the yard, runs up between the wheat and the barn, along the
back of the farm past the chute, and down the far side of the pasture onto the highway again,
so a rig drives round without backing up (`ranches::cattle_track`, `ranches::load_frame`). There is a 3 x 3 corn field (2.2 m corn you can walk through) and a 3 x 2 wheat
field, hay bales, the family's side-by-side in the yard, and loot in the barn and the house.
The terrain levels a pad around each farm. Each ranch is its own settlement on the map, with a
welcome sign.

**Bovbazaro de Porto Luma** is the island's one big stockyard (cattle market). It sits on the ring
road at the edge of Porto Luma. The cattle truck backs onto a chute into a receiving pen, which
opens on each side into a big holding pen with feed bunks and troughs. Behind them are the sale
barn (the auction ring), an office and hay, and beside them a gravel truck park where the two big
rigs live. The yard is never empty: it starts with fifteen head, and the oldest are sold at
auction once it holds thirty.

**Brutkajo de Porto Luma**, the cattle quay, is at the north end of the cargo terminal's quay
(see [trucking.md](trucking.md)): a pen behind a loading chute on the quay lane, and a cattle
gangway up the side of the ship at the berth.

## The families
Each family has four members: the rancher rides herd in the pasture, his wife works the fields,
the son drives the side-by-side round the farm roads (laps of the fields, with a rest in the
yard), and the daughter minds the barn and the corral. They come in to the porch for lunch
(12:00-13:00) and are home in the farmhouse at night (20:00-6:00). Every one wears a
**cattleman hat** (made for the game; drawn on the head of any body, `HeldGun::crown`) and
carries a **shotgun**. Close by, a tag shows their name ("Rafael Garza · Vakero").

They keep to themselves. **Shoot at one of them, shoot their cattle, or drive off in their
side-by-side**, and the whole family comes for you with shotguns for 90 s, or until you die.
Only that family: the neighbours and the gangs stay out of it. Killed, they are back the next
morning. Far from the player (420 m) they sleep and cost nothing.

## Cattle
Each ranch runs ten head of Hereford and Angus. They graze, wander the pasture, turn and walk
with a sway, and keep a little apart. A shot cow drops. Calves are born each morning, up to four,
bringing the herd back up to ten. When the cattle truck is coming, three head are driven through
the lane at the back of the pasture into the corral. When it pulls up, they go single file up the chute into the trailer.
At the cattle quay they come off down the chute into the pen. A little later they walk single
file across the quay and up the gangway into the ship, and they are gone: shipped. The ranch is
paid **450 kr a head** (`Ranches::cattle_price`; a line on screen says who was paid what, and
`RanchState::earned` keeps the total).

## The side-by-side
`sxs` in the vehicle manifest: a two-seat UTV (like a Polaris Ranger or Can-Am Defender) with a
roll cage and roof, light bar, half doors, a brush guard and a cargo box. Anyone can drive it.
Taking it on the ranch angers the family.

## The big rigs
`semi`: a long-nose conventional tractor (Peterbilt 379/389 style) with a chrome grille and
stacks, sleeper, fuel tanks and tandem duals. It pulls an **articulated trailer**: the trailer's
tandem trails the fifth wheel wherever the tractor goes, including when the player drives it,
and never folds past 80 degrees, however sharp the corner.
- **The supply rig** hauls a 53 ft dry van (white with a green stripe) round the bus loop and
  pulls up in Espera, Porto Luma, Suda Haveno, Roka and Verda City to unload supplies.
- **The cattle rig** hauls a polished livestock "pot" trailer. It runs the cattle quay and the
  five ranch gates in the order with fewest turnabouts, loads up to four head at each ranch and
  unloads them at the cattle quay to be shipped, paying each ranch for its own head. (On a map
  without the cargo terminal it unloads at the stockyard instead.)
- **Verda Transporto's own drivers**: a container shuttle between the depot and the cargo
  terminal, a second supply rig on the bus loop (starting halfway round), and a run from the
  terminal to Roka and back to the depot. See [trucking.md](trucking.md).

They all drive like the buses, kinematically along the road. They slow for towns and corners, wait
behind anything in the lane, and squeeze past cars parked at the kerb. A rig that is wrecked or
taken by the player leaves its run. Drivers sit at the wheel; the cattle hauler wears a hat.

## Dev
`OUTLAND_DEV_HAT=1` puts a Vakero hat on the player (to check how hats sit).
