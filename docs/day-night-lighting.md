# Day/night lighting

Verda now has a sky that follows the time of day. That means sunrise and sunset colours, dark blue nights with a moon and stars, street lamps that come on at dusk with warm pools of light, glowing fires, and car headlights at night.

## Time in each mode

| Mode | Time |
|---|---|
| Explore | Follows island life's clock (the `MON 07:37` at the top), so the sky matches residents' schedules |
| Zombie Survival | Starts at 17:30 and runs an hour per real minute, so night falls about three minutes in. The time shows at the top |
| Battle Royale | Fixed late-afternoon light (15:30), so every match is fair to see in |
| Dev Lab | Starts at noon; **F6 / F7** step the hour back and forward |

Dev builds also take `OUTLAND_DEV_HOUR=22` to force any hour in any mode, for example for screenshots. It combines with `OUTLAND_DEV_SPAWN`.

## How it's drawn (no lighting shader, so it's cheap on phones)

`world::sky::sky_at(hour)` gives one `SkyState`:
- sky colours;
- an ambient colour;
- daylight, lamp and star levels;
- sun and moon directions.

It's built from 11 keyframes (night, dawn, morning, midday, afternoon, sunset, dusk), eased with smoothstep. Nothing changes by more than a few colour steps per game minute.

Each frame is drawn in three steps:
1. **Sky gradient**, drawn behind the world and pre-divided by the ambient colour, so it ends up the colour it should be.
2. **World**, drawn as before, then **one full-screen multiply** by the ambient colour.
   - This darkens everything at dusk and night: buildings, terrain, characters, cars, the gun.
   - It's skipped when the ambient is white (midday), so daytime costs nothing extra.
3. **Lights**, added on top with additive blending, still depth-tested so buildings hide them:
   - **Street lamps (about 1 300):** a bright bulb with a soft halo on the head of each street light, and a warm pool of light on the ground under it. They fade in from about 19:00 and are off by morning.
     - Within 240 m (`lamp_pool_range`) a lamp lights the ground.
     - Further out, to 900 m (`lamp_glow_range`), only its bulb glows, drawn bigger with distance. A town is a cluster of lights across the dark.
     - The bus stops' and the boulevard's lampposts light up too, from a lantern on top.
   - **Fires** (the trailer park's fire pit): an orange flickering glow and ground pool, day and night, stronger at night.
   - **Headlights:** a pool of light ahead of the car you're driving, after dark.
   - **Sun, moon and stars:** the sun rises in the east at about 06:00 and sets in the west around 18:00–19:00, the moon is opposite, and 260 stars fade in at night.

Glows are camera-facing sprites that fade from a bright centre to black, which adds nothing in additive blending. All of it is a few hundred triangles per frame.

Light sources come from the map (`collect_lights`): street-light models (the lamp head is 0.9 m out on the arm, 6.2 m up), the transit lampposts and bonfires.

Every town street is lit (`dev::street_lighting`, run last in the world kit). Along every street, lane and suburb road 5 m wide or more (not the highways or dirt tracks), a street light stands at least every 28 m (`street_light_spacing`), alternating sides. It goes on the verge, 1.3 m off the road, with its arm out over the road. It goes wherever there isn't one already and nothing else stands: a house, a tree, a wall or another road. That added 471 to the island:

| Town | Street lights |
|---|---|
| Verda City | 349 |
| Porto Luma | 137 |
| Roka | 77 |
| Espera | 75 |
| Suda Haveno | 73 |
| Each hamlet | 5 to 12 | They're rebuilt automatically when assets are added or removed in the builder.

## Validation

`outland_day_night_tests`:
- noon is unmultiplied, with lamps off and no stars;
- midnight is dark blue, with the moon up, lamps on and stars out;
- sunrise and sunset times;
- warm sunset horizon;
- every minute of the day changes colours and daylight only slightly;
- dawn brightens and dusk darkens;
- hours wrap, and NaN counts as noon;
- the pre-multiplied sky comes out as intended;
- the published map's street lamps (heads up the pole, pools under them) and the fire pit; `outland_road_audit_tests` keeps every one of them off the carriageways.

Checked in the running game under Xvfb: downtown at noon, sunset, dusk and midnight; a lamp-lit parking lot; the trailer park at night with its fire. FPS was unchanged within noise (24–30 under software GL).

## Not done yet

- **No directional sunlight or shadows:** buildings are evenly lit, just tinted. That needs a lighting shader.
- **Windows don't light up at night,** and interiors are as dark as outside.
- **Night doesn't change gameplay yet:** bots and NPCs see as far at night as by day. Darkness should shorten sight outside lamp pools.
- **No weather.**

## Sun, sky light and haze (WorldShading)

`engine::render::WorldShading` lights the world with the time of day:

- **Shading.** Every model with normals is lit per vertex by the sun (`SkyState::sun`, warm, strongest
  when high, gone at night) plus a sky light from above and a dimmer ground bounce from below, so
  walls facing the sun are bright and those away from it are in shade. The terrain's normals follow
  its slopes, so hillsides catch the sun or fall into shadow.
- **Haze.** Everything fades into the horizon's colour with distance (from 90 m, thickest at 700 m,
  never more than 72%), so distant hills and towns melt into the sky instead of popping in.
- **Textures** loaded with models are mipmapped and trilinear-filtered (anisotropic 4x on desktop
  GL), so distant roads and walls don't shimmer.
- Two shaders, GLSL 330 on desktop and GLSL 100 on phones and browsers: a lit one for models, and a
  haze-only one for geometry without normals (shapes drawn one by one and the recorded road and
  building batches). Characters keep raylib's shader (it does their skinning).
- `OUTLAND_SHADING=0` turns it off; if the shaders fail to compile everything draws as before.
- Desktops and browsers now start the world at full resolution (phones at 75%); the dynamic
  resolution still steps down if the frame rate drops.
