# The map, the zones and the settings

## Map (MAPO)

Open the map with the **MAP · M** button at the top right, or the **M** key. Close it with **CLOSE**, **M** or Escape. Tap the map to set a waypoint ([below](#waypoints-and-the-compass)).

- **What it shows:** the whole island, drawn from the terrain, with:
  - the sea, beaches and hill shading;
  - the five zones in their colours, with their borders and names;
  - every road (paved dark, gravel tan, dirt brown);
  - the towns;
  - a 1 km grid, north arrow and 500 m scale bar.
- **You** are the yellow arrow, pointing the way you face. The panel names your current zone and your position.
- **Battle Royale** also draws the zone (blue) and the next circle (white, with a dashed line to it when you are outside it), and while the helicopters fly, their route across the island (yellow, dashed) with the three of them on it.
- **It's modal, but the world keeps running**, as in PUBG. Open it somewhere safe.
- **Cost:** the map image is baked once (320×320 terrain samples), the first time the map opens.

`game::MapScreen` (`src/game/MapScreen.cpp`) handles the drawing and touch input.

## Waypoints and the compass

- **Set a waypoint:** tap or click anywhere on the map. A pink pin goes there, with a dashed line from you and its distance. Tap the pin to clear it. In the world it stands as a pink column of light, and it clears itself when you get within 18 m ("Waypoint reached").
- **The compass** (top centre) marks the places that matter as small icons at their bearing:

  | Mark | Icon | When |
  |---|---|---|
  | Your job or police orders | gold diamond (blue on duty) | while you have one |
  | Your waypoint | pink pin | while set |
  | The safe zone | blue ring | Battle Royale, outside or near the edge of the next circle |
  | Your crew's hideout | hexagon in your gang's colour | gang members, the nearest one anywhere |
  | Rival hideouts | hexagon in their colour | within 400 m (220 m for civilians) |
  | Police station | blue shield | the nearest within 700 m (anywhere for officers) |
  | Hospital | red cross | within 2.5 km |
  | Shops | bag: general store, pharmacy, hardware, pub | the nearest of each within 450 m |
  | Bus stop | yellow H | the nearest within 260 m |
  | Ranches | barn | within 600 m |
  | Docks | anchor | gang members, the nearest within 700 m |
  | Training camp | target | officers |
  | Home | gold house | once you own the penthouse |

- **Leading marks** (the job, the waypoint, the safe zone) are always on the strip, with their distance under them. When one is behind you it's pinned to the end of the strip with an arrow pointing the way to turn.
- **Other marks** show only when they're in the strip's 120° view. Face one (within 6°) and its name and distance appear under the compass.
- The map draws the same marks, with the leading ones named.
- Logic: `game::hud::draw_compass` and `strip_spot` (`src/game/hud/Instruments.cpp`). The marks are gathered in `Renderer.cpp`.

## Zones (zonoj)

`world::zones::zone_at(x, z)` names the zone for any position (`include/outland/world/Zones.hpp`).

- **Verdkerno** is the capital district: Verda City and its edge, within about 560 m of the centre (`zones::capital_radius`). Its limit wobbles a little, like a city boundary.
- **The other four counties** share the rest of the island, each round its own town. Their borders wander by up to about 110 m, so they curve like county lines, and they meet at the city limit.
- Entering a zone shows a banner with its name, what the name means, and what the zone is like.

The zone list and Ferarbaro's lore are in [verda-lore.md](verda-lore.md).

## Settings (AGORDOJ)

The map screen's right panel is a short version of the pause menu's SETTINGS page (see [controls.md](controls.md)):

| Setting | Values |
|---|---|
| Mouse sensitivity (PC) or touch look sensitivity (touch screens) | in steps of 0.1 |
| Volume | in 10% steps |
| Sound | on / off |
| Graphics | AUTO (dynamic resolution, the default), FAST (fixed 60% resolution, for slow phones), SHARP (full resolution) |

- Settings save to `profiles/settings.cfg` beside the saves, and are applied at start.
- On the web build they sync to the browser's storage like the saves.
- `OUTLAND_RENDER_SCALE` still overrides the graphics setting.

Keyboard:
- **N** mutes (it was **M**; M now opens the map).
- **[** and **]** change the volume.

## Sovaĝejo

Ferarbaro's backcountry settlement (`ferarbaro_sovagejo`), built by `outland_build_world`.

- **The trailer park:** 12 enterable trailers, formerly Roka's, now Sovaĝejo's.
- **Seven homesteads** on a dirt road running south from Roka's main street, then west. Each has:
  - a shack home, a lean-to, and on some a second shack as a workshop;
  - a car that runs, with one or two **parts donors** on cinder blocks;
  - rain barrels, firewood, pallets, a fire pit;
  - a mended wooden fence and trees.
- **Parts donors** are vehicle markers whose ids contain `_donor_`. They have no engine and no fuel, are missing wheels, and look faded.
  - They can't be entered or driven, and residents never take them.
  - Battle Royale's fleet reset leaves them as they are.
- **Hand-painted boards** (`SignKind::Homemade`) stand along Ferarbaro's gravel and dirt roads about every 55 m, and on its highways about every 170 m.
- **Textures:** the low wooden fence models were re-embedded as PNG (`tools/convert_glb_jpeg_to_png.py`). raylib's default build can't decode JPEG, so they drew white before.

## Tests

- **`outland_zones_map_tests`:**
  - every seat is in its own zone, and the compass directions are right;
  - Verda City is Verdkerno, which covers 2–10% of the land, and the four counties begin just outside it, each holding over a sixth of the land;
  - the map projection puts north up, and the settings buttons fit and don't overlap at phone and desktop sizes;
  - settings survive a save and load, and bad values are clamped;
  - Sovaĝejo exists in Ferarbaro with dirt and gravel roads only, its trailers, homesteads, cars that run, parts donors and residents.
- **`outland_signage_tests`:** homemade boards appear only in Ferarbaro, including VERDA ESTAS LIBERA, LASU NIN VIVI and NENIU MASTRO, and Sovaĝejo has its own BONVENON AL SOVAĜEJO entry sign.
