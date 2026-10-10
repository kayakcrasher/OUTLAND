# Building purposes in the Creator

What a building is *for* is now authored in MAP BUILDER. Explore's island life reads it to decide who lives, works, drinks, prays and gets patched up where.

## Purposes

| Purpose | Use |
|---|---|
| AUTO | The default; same behaviour as before (see below) |
| HOME | |
| SHOP | |
| PUB | |
| CHURCH | |
| POLICE | |
| CLINIC | |
| OFFICE | |
| GARAGE | |
| INDUSTRY | |
| DOCK | |
| FARM | |
| VACANT | Keeps a building out of island life, including the downtown kit buildings |

**AUTO** keeps the old designation from building style, ID keywords ("pub", "church" and so on) and town identity. Every untouched building behaves exactly as before.

## In the builder

- **Select a building**, then press **USE** (right side, under EXPORT) or **U** to cycle its purpose. **Shift+U** cycles backwards.
  - This works on procedural houses and on any building-sized model: Main Street rows, towers, sheds, houses.
  - The button shows the current purpose, for example `USE: PUB`.
  - Walls, slabs, props, roads and spawns have no purpose, so the button stays hidden.
- **Buildings assembled from parts** (walls + slabs) have no single object to select. For those, place a **Building Purpose** marker (GAMEPLAY tab):
  - put it **just inside the front door**, and **ROTATE** it so its lime arrow points **out of the door**;
  - residents use the point 2 m out along the arrow as the door, and walk in to the marker.
  - A marker inside a building that is already a place retags that building. VACANT markers remove it.
- Every change goes through UNDO/REDO and autosave like any other edit.

## Saved with the map

- Map format version 7 adds `PURPOSE "<id>" <value>` records after each settlement's buildings, assets and markers.
- Version 6 maps load unchanged, with every purpose AUTO.
- A purpose naming an object that doesn't exist, or an out-of-range value, rejects the file, just like any other malformed record.

## Island life (`LifePopulation`)

- **Authored purposes win** over designation. They're never converted by the automatic pass ("a second shop becomes the pub" and similar).
- **Any building-sized model with a purpose becomes a place**, in the town nearest to it, not only the downtown kit.
- **Every workplace now opens with at least one worker** before the rest of the town picks the nearest job. Without this, outlying offices and quays were left empty while the town centre took every hand.

## The dressed towns

`outland_build_world` now places a purpose marker inside each of its 10 part-built buildings:

| Town | Building | Purpose |
|---|---|---|
| Roka | works office | OFFICE |
| Roka | yard building | INDUSTRY |
| Porto Luma | harbour office | OFFICE |
| Porto Luma | quay building | DOCK |
| Suda Haveno, Espera | houses | HOME |

Before, these buildings didn't exist for island life at all. They now house and employ people. `maps/verda_world.map` has been regenerated; the change is those 10 markers and their purposes.

## Validation

`outland_building_purpose_tests` (DEV) covers:
- cycling the purposes;
- which objects can carry a purpose;
- authored POLICE/PUB/VACANT on procedural houses;
- a purposed model becoming a place in its own town;
- a marker creating a place with its door out of the front;
- a marker retagging a building without duplicating it;
- authored workplaces being staffed;
- determinism;
- the map round trip for buildings, assets and markers;
- rejection of bad records;
- version 6 maps still loading;
- the full builder flow: place a marker, select it, cycle HOME → SHOP, and cycle a house AUTO → HOME → AUTO.

`outland_world_kit_tests`: all 10 part-built buildings are places, including offices and a dock, and each has residents or workers.

Checked in the real builder: the Building Purpose card is in GAMEPLAY, a placed marker selects as `USE: HOME`, and U turns it into `USE: PUB`.

DEV 44/44, release 35/35.

## Not done yet

- Purposes aren't shown in Explore itself; they appear only in the builder. A sign or label when you look at a building would help.
- No per-building capacities: how many people live or work there still follows the purpose's defaults.
- Residents never change purpose. A shop is a shop for the whole session.
