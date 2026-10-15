# The Vakeroj: ranches, cattle and big rigs

*Vakero* is Esperanto for cowboy (plural *vakeroj*), in keeping with the island's Esperanto names.

The Vakeroj are Verda's ranching families. There are five ranches, one in each county, and each
family grows crops and raises beef cattle. Two 18-wheelers work the roads: one supplies the
towns, the other hauls cattle from the ranches to the stockyard.

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
over a stock tank, a fenced pasture with a feed bunk and troughs, and a corral by the road with a
loading chute. There is a 3 x 3 corn field (2.2 m corn you can walk through) and a 3 x 2 wheat
field, hay bales, the family's side-by-side in the yard, and loot in the barn and the house.
The terrain levels a pad around each farm. Each ranch is its own settlement on the map, with a
welcome sign.

**Bovbazaro de Porto Luma** is the island's one big stockyard (cattle market). It sits on the ring
road at the edge of Porto Luma. The cattle truck backs onto a chute into a receiving pen, which
opens on each side into a big holding pen with feed bunks and troughs. Behind them are the sale
barn (the auction ring), an office and hay, and beside them a gravel truck park where the two big
rigs live. The yard is never empty: it starts with fifteen head, and the oldest are sold at
auction once it holds thirty.

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
the lane into the corral. When it pulls up, they go single file up the chute into the trailer.
At the stockyard they come off down the chute into the pen.

## The side-by-side
`sxs` in the vehicle manifest: a two-seat UTV (like a Polaris Ranger or Can-Am Defender) with a
roll cage and roof, light bar, half doors, a brush guard and a cargo box. Anyone can drive it.
Taking it on the ranch angers the family.

## The big rigs
`semi`: a long-nose conventional tractor (Peterbilt 379/389 style) with a chrome grille and
stacks, sleeper, fuel tanks and tandem duals. It pulls an **articulated trailer**: the trailer's
tandem trails the fifth wheel wherever the tractor goes, including when the player drives it,
and never folds past a right angle.
- **The supply rig** hauls a 53 ft dry van (white with a green stripe) round the bus loop and
  pulls up in Espera, Porto Luma, Suda Haveno, Roka and Verda City to unload supplies.
- **The cattle rig** hauls a polished livestock "pot" trailer. It runs the stockyard and the
  five ranch gates in the order with fewest turnabouts, loads up to four head at each ranch and
  unloads at Porto Luma's stockyard.

Both drive like the buses, kinematically along the road. They slow for towns and corners, wait
behind anything in the lane, and squeeze past cars parked at the kerb. A rig that is wrecked or
taken by the player leaves its run. Drivers sit at the wheel; the cattle hauler wears a hat.

## Dev
`OUTLAND_DEV_HAT=1` puts a Vakero hat on the player (to check how hats sit).
