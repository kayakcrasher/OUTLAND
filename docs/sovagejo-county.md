# Sovaĝejo's country and Fajrejo

Sovaĝejo is a little rural county now, not just one trailer park and one road. Its core is
unchanged: the trailer park, the homesteads, Sovaĝa Monto, the mountain road, its pullouts and
Supra Sovaĝejo ([sovaga-monto.md](sovaga-monto.md)). Round it are a few rough dirt tracks to
scattered country homes, and **Fajrejo**, a tiny settlement where everybody knows everybody.

Code:
- `world/SovagejoCounty.hpp` lays out the tracks, the home sites and Fajrejo, and shapes the land.
- `outland_build_world` (`sovagejo_county()` in `src/dev/VerdaWorldKit.cpp`) builds on them.
- The models come from `tools/generate_city_kit.py` (the "Sovaĝejo's country" section), in
  `assets/verda/city/country/`.

## The tracks
Seven tracks, each 30–150 m long, 5 m wide, dirt:

| Track | Leaves from | Goes to |
|---|---|---|
| Orienta vojo | the mountain road, just north of the trailer park's gate | Fajrejo |
| Kreka vojo | Fajrejo's lot (east end) | the Moreno farm by the creek, past Ŝimko's trailer |
| Arbara vojeto | Fajrejo's lot (west end) | the Kovaĉ family compound |
| Ĉasista vojeto | the mountain road | Haraldsen's hunting cabin |
| Okcidenta vojo | the mountain road | O'Neil's fishing trailer, toward the sea |
| Farmvojeto | Okcidenta vojo | the Lindqvist farm |
| Suda vojeto | the homestead road (between the homesteads) | the Petrov workshop |

Each track follows the land, evened out, and is never steeper than 12%. It meets the road it leaves
at that road's own height, and runs level for the first few metres.

## The ground
The ground is cut and filled to each track's bed (level across), and a level pad sits under each
home and under Fajrejo. The shaping is minor: at most about 4 m of cut or fill anywhere. It fades
out toward the mountain road, whose own bed is left exactly as it was. Sovaĝa Monto (289 m) is still
the island's highest ground. The west coast is untouched; its beaches are for a later update.

## The homes
Ten households out in the country, with plenty of empty land between them:

| Home | Family |
|---|---|
| A roadside trailer on the east road | Brandt (OVOJ · MIELO for sale) |
| A hunting cabin with a deer stand in the trees | Haraldsen (ĈASTERENO) |
| A family compound: two trailers round a fire pit, a workshop, a rail fence | Kovaĉ (LIGNO VENDATA) |
| A farmhouse by the creek: hay, a trough, a shed | Moreno (FREŜA LAKTO) |
| A trailer with a workshop shack and a boat | Ŝimko |
| A farmhouse with a windmill, out west | Lindqvist |
| A fisher's trailer with two boats, nearest the sea | O'Neil (FREŜAJ FIŜOJ) |
| A farmhouse, the grandmother's trailer and a workshop full of cars | Petrov |

Every yard has old cars that run and parts donors on blocks, a generator, a woodpile and an
outhouse or shed. Each home has a mailbox with the family's name on a hand-painted board, and the
bigger homes fly the Apelacio.

## Fajrejo
A gravel lot, 16 m wide, at the end of the east road, with the other two tracks leaving its ends.
Round it:
- **Preĝejo de la Monto**, a white Baptist chapel ("DIMANĈE JE LA 10A · ĈIUJ BONVENAS");
- **Vilaĝa Vendejo**, the general store: a false front over a porch, a counter and shelves inside
  ("PANO · LAKTO · MUNICIO · LOGILOJ");
- **Riparo**, the mechanic's shed, with a drive-in door;
- four houses: two log cabins, a farmhouse and a trailer.

Pickups and a side-by-side are parked on the lot. There are benches by the store, two lights, and
the Apelacio on a tall pole. A board on the way in reads "FAJREJO · ĈI TIE ĈIUJ KONAS ĈIUJN ·
VETURU MALRAPIDE".

## People
Every new home is a HOME (enterable: cabins, farmhouses and trailers). The store is a shop, the
sheds and workshops are garages, and the chapel is a church.

Island life counts everyone here as Sovaĝejo's (`world::county::near`, as for the mountain road),
so they shop at Fajrejo's store, go to its chapel and work in its sheds.

## Tests
`outland_sovagejo_county_tests` checks:
- each track joins what it leaves exactly, at its height, and ends at its home or in Fajrejo;
- the land is the track's profile, level across, no steeper than a rough track;
- every pad and Fajrejo stand level, in Ferarbaro and well up from the sea;
- the mountain road is untouched, and Sovaĝa Monto is still the highest ground;
- there are 8–12 country homes, and Fajrejo has its church, store, mechanic and houses;
- the yards (cars, donors, generators, woodpiles, the deer stand, boats);
- everyone is Sovaĝejo's in island life;
- the boards and the Apelacio flags.
