# Esperanto signage and the Verda flag

Verda is an Esperanto-speaking island (see [verda-lore.md](verda-lore.md)), so everything written in the world is now in Esperanto. The signs and flags are built from the map and island life, so they appear in every mode and follow Creator edits.

## What's on the island

**Shopfronts**: a board over the door of every working building. Each town gets different names, nearest the centre first.

| Kind of building | Sign |
|---|---|
| Shop | One of 12 trades: VENDEJO, BAKEJO, LIBREJO, APOTEKO, HORLOĜISTO and others, with the owner below: "ĈE NOVAK" |
| Pub | A name, e.g. LA VERDA STELO, LA FORGESITA INSULO, LA KOSMONAŬTO, with "DRINKEJO" below |
| Church | PREĜEJO, "DE SANKTA ..." |
| Police | POLICO, on navy |
| Clinic | KLINIKO, "URĜA HELPO" |
| Offices | The first is REGISTARA PALACO in the capital and URBODOMO elsewhere; the rest are POŜTOFICEJO, BANKO DE VERDA, RADIO VERDA, INSTITUTO 47 and others |
| Garage | GARAĜO / AŬTOMEKANIKEJO, "BENZINO · OLEO · RADOJ" |
| Works | FABRIKO N-RO 7, KONSERVFABRIKO, SEGEJO and others, "ENIRO NUR POR LABORISTOJ" |
| Docks | HAVENO, FIŜMERKATO, HAVENOFICEJO, "NE FUMU" |
| Fields | KOLĤOZO «VERDA ESPERO», on a post |

**Town entries:**
- Where each road enters a town there's a green "BONVENON AL / ROKA / RESPUBLIKO VERDA" sign on the incoming driver's right. The capital's says "ĈEFURBO DE VERDA" instead.
- "ĜIS REVIDO!" is on the back.

**Distance signs:**
- On each way out of town, a sign lists the towns that road is the best way to, with road distances, e.g. "Porto Luma  1,6 km".
- Distances are measured on the road graph and use the Esperanto decimal comma.

**Street names** on every downtown traffic signal: two blades, one for each crossing street. They include ZAMENHOF-STRATO, AVENUO DE LA PACO, KOSMONAŬTA AVENUO, BULVARDO VERDA STELO and STRATO DE LA EKSPERIMENTO.

**Bus stops**: "BUSHALTEJO / LINIO n". Shelters carry the sign on the roof, benches on a post.

**The flag** (green and white stripes, white hoist triangle, green star) flies:
- on a pole on every town square, with a tall one on the capital's courthouse square;
- the square carries the plaque "PLACO DE LA RESPUBLIKO / FONDITA 1961";
- from a staff beside the sign on police stations and town halls;
- from the roofs of the tallest towers.

All flags ripple in the wind.

**Names:**
- Residents are Slavic and Anglo, with Esperantised first names: Petro Novak, Johano Walker, Ludmila Kovač.
- The HUD line naming the nearest resident uses the sign font, so ŭ, č and ć show correctly.
- The home screen now greets you with "BONVENON AL VERDA".

## How it works

- **Layout** is plain data, testable without a window: `game::culture::build_signage(region, island)` in `src/game/culture/Signage.cpp`.
  - **Shopfronts** come from island life's places, so they use the same purposes as residents (authored in the Creator or designated).
    - Procedural houses: the sign sits on the front wall above the 2.35 m doorway.
    - Model buildings: the facade is found by marching from outside the door toward the interior at sign height until something solid is hit.
  - **Town radius** is the furthest workplace door plus 18 m. Entry signs go where roads cross that circle.
  - **Onward distances** use a shortest-path search over the road network that never goes back through the town.
  - **Street names** are assigned per straight road line. Each signal picks the two nearest lines that cross.
  - **Flagpole spots** around a square are searched outward until one is clear of buildings, props and roads.
- **Words** live in `src/game/culture/Esperanto.cpp` (`place_sign`, `street_name`, `format_distance`, `upper`). Add trades, pub names or streets to the lists there.
- **Drawing** is `game::culture::SignRenderer`:
  - boards and posts first;
  - then all lettering in one batch from the font atlas, as 3D quads, mipmapped so distant text doesn't shimmer;
  - then the flags as a waving 12×4 cloth grid with the generated flag texture.
  
  Signs are drawn within 120 m, their text within 75 m, and flags within 500 m. They darken at night with the rest of the world.
- **Font:** `assets/fonts/DejaVuSans-Bold.ttf`, unmodified, under the license in `assets/fonts/LICENSE-DejaVu.txt`. It's loaded with ASCII, Latin-1 and Latin Extended-A, which covers ĉ ĝ ĥ ĵ ŝ ŭ and Slavic č ć ž š. Without the file the game falls back to raylib's font.
- **Rebuilds:** the Renderer rebuilds signs when a mode starts and after Creator edits (when the builder closes). In Explore it uses the running life simulation's island, so a shop's sign names its actual shopkeeper.

## Tests

`outland_signage_tests` (`tests/integration/culture/test_signage.cpp`) covers:
- **Text:**
  - UTF-8 decoding and uppercase, including ĉ → Ĉ and ž → Ž;
  - every sign line and every resident's name printable in the sign font;
  - distances in Esperanto style.
- **The flag:** stripes, triangle and star in the right places.
- **Placement on the published map:**
  - a shopfront for every workplace, at least 90% of them flush on their wall;
  - POLICO, PREĜEJO, KLINIKO and REGISTARA PALACO downtown;
  - an entry sign for every town, beside the road and never on it;
  - the capital's exits pointing on to all four other towns;
  - two different street names at every downtown signal;
  - a sign at every bus stop;
  - a clear flagpole on every town square, plus police, town hall and tower flags.
- **Determinism.**

The published map gets 110 signs and 12 flags, built in about 50 ms.
