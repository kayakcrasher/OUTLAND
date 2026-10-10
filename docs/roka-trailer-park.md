# Roka trailer park

Twelve mobile homes from the Trailer Park pack (El Bolillo Duro, CC0) now stand on the flat ground about 220 m south-west of Roka's centre, around (−1900, 610).

## The park

- **Trailers:** two rows of six off a gravel lane, on 13 m lots.
  - Each home's narrow end faces the lane, with its porch and steps toward it.
  - The six models are mixed along the rows, with a few degrees of skew so the rows don't look machine-placed.
- **Yards:**
  - a drivable hatchback on the pad beside most homes (9 of 12);
  - barrels and pallets out back;
  - shade trees on alternate lots.
- **Common ground** at the west end: a fire pit with three benches.
- **Enclosure and lighting:** street lights along the lane, and a chainlink fence around the park with the gate at the lane's east end.
- **Access:** a gravel road from the gate to Roka's north–south street.

## Living there

- **Enterable trailers:** climb the porch steps and walk in through the door. The door and screen-door panels are knocked out of collision and drawing; the window screens are knocked out too, so windows are open to bullets (Battle Royale).
  - The doorway is 0.88 m wide, so line up with it.
- **Explore:** every trailer is a HOME, via a Building Purpose marker just inside its door.
  - The park adds three residents per home to Roka's population, and people live in all twelve.
- **MAP BUILDER:** the six trailers are in BUILDINGS ("Trailer Home 1–6", search "trailer").
  - A trailer's door is on its long side, not the −Z end. If you place one by hand, drop a Building Purpose marker just inside its door, arrow out.

## Assets

The pack's JPEG textures were re-embedded as PNG by `tools/convert_glb_jpeg_to_png.py`:
- raylib's default build can't decode JPEG, so they would otherwise draw untextured on phones;
- the conversion made each model about 0.45 MB bigger (1.15 → 1.6 MB);
- CTest `outland_trailer_texture_validation` fails if a JPEG comes back.

About 200 older urban models (fences, dumpsters, road pieces) also embed JPEG. The same tool can convert them if they show up white on your phone.

## Dev builds

`OUTLAND_DEV_SPAWN="x,z,yaw"` starts any mode at that spot, for example `OUTLAND_DEV_SPAWN="-1842,610,-90"` for the park gate looking down the lane.

## Validation

`outland_world_kit_tests`:
- the park is built with 12 trailers;
- a body walks from the lane up every porch (side steps on models 1–4, front steps on 5–6) and through every door onto the floor inside;
- every trailer is a home with residents.

`maps/verda_world.map` was regenerated with the park. Kit asset IDs in the towns generated after it were renumbered.
