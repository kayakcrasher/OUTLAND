# Verda environment and audio first pass

The target is believable proportions, bright natural colours, readable silhouettes,
layered environments and restrained geometry. All assets in this pass are original
procedural shape/material blockouts. No PUBG or Fortnite assets or recordings are used.

## Included

15 reusable asset types, each exported at three detail levels: grass and dirt ground
tiles, field/verge grass, weeds, pebble scatter, two bushes, two rocks, two trees,
a rural house, pistol and rifle. OBJ assets have flat normals and UVs mapped into a shared
64px palette texture and a single material. Units are metres, Y is up, front is -Z,
and environment origins are at the ground centre. Gun pivots are at the muzzle,
with barrels pointing +Z. Grass blades have explicit two-sided
opaque geometry. Keep OBJ, MTL and PNG files together.

Run `python3 tools/generate_verda_kit.py` to generate
`assets/verda/starter/`. Open `preview.html` in a browser for drag rotation,
zoom, LOD switching and triangle counts. `contact_sheet.svg` is a static overview.
`manifest.json` records triangle counts and bounds. Exported OBJ assets are for
reuse/import; the current game still draws procedural shapes and does not load
these OBJ files. The draw functions implement the same visual direction.

Live rendering now has gable roofs, entry steps, sills, trim, chimneys, brighter
vegetation, varied tree sizes, branching and cheaper canopy geometry. Houses use
their stored rotation, and collision follows it. House decoration disappears at
90m; tree detail reduces at 65m and 140m, with a 260m cutoff. Buildings stop
drawing at 320m. Grass thins at 40m/58m and shrinks over the outer 10m of its 70m
range; weeds, bushes and flowers have shorter ranges. Vegetation is excluded from
collidable objects and roads. Distances are provisional, requiring device profiling.

Roads draw an upward-facing ribbon sampled every 2m from the terrain, replacing
per-frame mesh/model allocation and destruction. This is still immediate geometry:
it should eventually be cached and batched with chunk streaming. Roads and terrain
use different tessellation, so steep slopes can still cause intersection artifacts.

## Audio

`python3 tools/generate_verda_audio.py` generates an original eight-second wind loop
and three variations each for grass, dirt and gravel footsteps. They are sound-design
sketches, not recorded final Foley. Files are mono, 22.05kHz, 16-bit PCM.
Wind streams and loops; footsteps are loaded once. Roughly one footstep plays per
1.65m of actual horizontal ground movement, giving faster cadence when sprinting.
Jumping, menu entry, and teleporting reset the cadence. Missing files or audio-device
failure do not stop the game. The audio device and sounds are released on exit.

Tap/click the upper-right audio button or press **M** to mute.
**[** lowers volume; **]** raises it. Master level starts at 65%, with quieter
wind and footsteps mixed separately. These settings are session-only.
The first environment audio pass now has accompanying gunshots, reload and hit sounds;
see [Combat](systems/COMBAT.md). Vehicles, music and spatial enemies remain future work.
Asphalt temporarily shares the gravel footstep set.

## Termux / X11 test

Use the existing Termux raylib, compiler, CMake and pkg-config setup.
Install Python if needed with `pkg install python`. From the repository root:

```bash
bash scripts/build_verda.sh
bash scripts/run_x11.sh
```

The build generates assets and copies them beside `build/outland`.
For a standard CMake workflow, asset generation is also an executable dependency:

```bash
cmake -S . -B build -DOUTLAND_BUILD_TESTS=ON
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Enter Dev Lab and press **2** for Espera:

1. Walk around both sides of the village. Check roof slopes, windows, steps,
   tree trunks/branches and brighter colours. Check for missing triangles,
   hovering roads, grass through walls, and collision that does not match houses.
2. Walk off-road, onto the main dirt road and onto the cross-village gravel road.
   Listen for surface changes. Sprint, stand still, push against a wall, jump,
   teleport, mute/unmute, and try the volume keys. Check for clipping or a wind-loop tick.
3. Walk toward and away from houses/trees. Look for excessive LOD popping.
4. Compare FPS at the same village spot and camera direction against the previous
   build. Run for a few minutes to check heat/stutter and exit/relaunch.

Report phone model, raylib version (`pkg-config --modversion raylib`), FPS at that
spot, whether audio works, and any visual problems. Screenshots or a short capture
will help tune the next pass. Termux/X11 results validate this desktop-style build
on your phone; a native Android package still needs its own profiling and audio-path check.

## Validation and limits

Existing core tests pass. A headless integration test records raylib calls and
checks roof winding, terrain-following road geometry, detail reduction, rotated
collision, two-sided foliage, road exclusion, audio cadence, muting, resource
cleanup and device failure. The Python validator checks all 45 exported OBJs
for indices, normals, UVs, nondegenerate geometry and decreasing LOD budgets,
plus fifteen unclipped WAVs and wind seam continuity.

The available cloud workspace supports syntax compilation with raylib 6.0 headers
and direct core/headless test compilation. It lacks CMake and the raylib library,
so a full game build, audible listening test and real FPS measurement require
Termux/X11. A browser sandbox restriction prevented a full browser preview run;
the static contact sheet was visually inspected, and the offline preview JavaScript
was checked with a headless DOM/canvas harness for asset selection, LOD, rotation,
zoom and reset.

The nine-stage roadmap remains: ground, trees/bushes, rocks, houses,
fences/walls/gates, roads, props, vehicles, interiors. This pass establishes the
first four at blockout fidelity and repairs a road rendering bottleneck.
Next: terrain material blending and worn paths, better canopy shapes, modular
house walls/roofs with actual openings, then fence/prop clutter. Interiors are
not functional yet: house doors/windows are exterior decoration and collision
still treats each house as a solid footprint.
