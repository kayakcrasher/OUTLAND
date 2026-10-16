# Government buildings and the stadium ground

## Registaro de Verda (Verda City)

Registaro de Verda is the island's parliament. It stands at the head of Verda City's south blocks, across the street from the apartments and offices and beside the highway in from the south. The world kit searches outward from the city centre for the nearest level, free lot on a paved street, and this is where that search lands.

The building is neoclassical stone, 48 × 22 m, with its front facing the street:
- a tall centre block with six columns under a pediment carrying the arms of Verda;
- broad front steps up to a raised floor 1.5 m above the ground;
- lower wings either side;
- a drum and a green copper dome with a lantern over the centre, the top 25.5 m up;
- Verda's flags on two poles at the foot of the steps;
- a 14 m lawn between the street and the steps.

The ground floor under the dome is one enterable hall, holding the president's desk on a dais and rows of red benches. The wings are sealed offices. The door sign reads **REGISTARO DE VERDA · PARLAMENTO · RESPUBLIKO VERDA**.

## Urbodomoj (county town halls)

There is one Urbodomo in the main town of each of the other four counties: Espera, Porto Luma, Suda Haveno and Roka. The tiny hamlets and neighbourhoods get none.

Each is two storeys and 20 × 14 m:
- cream render with stone quoins;
- a four-column porch under a small pediment, up three steps;
- a clock tower with a copper cupola, 17 m high;
- a flag on the roof.

Its ground floor is an enterable hall with the clerks' counter and benches for the public. The sign reads **URBODOMO** and the town's name. Each one stands near its town centre on a paved street.

## What they are for (for now)

Both kinds are **Office** places, so island life gives them clerks who work there.

Where a seat of government stands, the town's other offices are ordinary businesses. The "REGISTARA PALACO" sign used to go to whichever office was nearest the capital's centre; it no longer does.

A dedicated government role (laws, taxes, elections and the like) is planned for the next data reset, when a new building purpose can be added without breaking saves.

## Baseball stadium ground

A 150 × 150 m block of flat land east of Orienta, beyond the highway, is kept free for a future baseball stadium. It is centred at (606, 46): `dev::stadium_reserve`, with a half-width of `dev::stadium_half`.

The world kit claims this block before it places the government buildings, so nothing placed then or later can take it. The world-kit test checks that nothing solid stands there.

## Code

- **Models:** `parliament()` and `town_hall()` in `tools/generate_city_kit.py`, written to `assets/verda/city/civic/` and catalogued as `city_parliament` and `city_town_hall`. The Builder gained round columns, drums, domes and pediments.
- **Placement:** `government()` in `src/dev/VerdaWorldKit.cpp`, which runs last so older pieces keep their ids. It uses `civic_lot`, the same level-and-free lot search the police stations use.
- **Signs:** `src/game/culture/Signage.cpp`.
- **Test:** `outland_world_kit_tests` checks that the parliament, four town halls and the stadium ground exist, and that the seats of government have clerks.
