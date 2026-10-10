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
   - **Street lamps (81):** a bright bulb with a soft halo on the head of each street light, and a warm pool of light on the ground under it. They fade in from about 19:00 and are off by morning.
   - **Fires** (the trailer park's fire pit): an orange flickering glow and ground pool, day and night, stronger at night.
   - **Headlights:** a pool of light ahead of the car you're driving, after dark.
   - **Sun, moon and stars:** the sun rises in the east at about 06:00 and sets in the west around 18:00–19:00, the moon is opposite, and 260 stars fade in at night.

Glows are camera-facing sprites that fade from a bright centre to black, which adds nothing in additive blending. All of it is a few hundred triangles per frame.

Light sources come from the map (`collect_lights`): street-light models (the lamp head is 0.9 m out on the arm, 6.2 m up) and bonfires. They're rebuilt automatically when assets are added or removed in the builder.

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
- the published map has 81 street lamps (heads up the pole, pools under them) and the fire pit.

Checked in the running game under Xvfb: downtown at noon, sunset, dusk and midnight; a lamp-lit parking lot; the trailer park at night with its fire. FPS was unchanged within noise (24–30 under software GL).

## Not done yet

- **No directional sunlight or shadows:** buildings are evenly lit, just tinted. That needs a lighting shader.
- **Windows don't light up at night,** and interiors are as dark as outside.
- **Night doesn't change gameplay yet:** bots and NPCs see as far at night as by day. Darkness should shorten sight outside lamp pools.
- **No weather.**
