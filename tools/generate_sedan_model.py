#!/usr/bin/env python3
"""Generate the Verda Sedano (assets/verda/vehicles/runtime/sedan_body.glb), a family saloon.

A four-door, three-box saloon - the island's other everyday car beside the little hatchback:
a long bonnet and a proper boot, a deep maroon body with chrome bumpers, a chrome waist strip
and window surrounds, a slatted grille, square headlamps and wide red tail lamps, four doors
with chrome handles, a bench of seats front and back. Residents and island traffic drive it.

Built from boxes with vertex colours like the other vehicles. +Z is the front, Y is up, metres,
ground at Y = 0; the driver sits on the left (+X). The wheels are the pickup's.

    python3 tools/generate_sedan_model.py           # write the model
    python3 tools/generate_sedan_model.py --check   # fail if the committed model is stale
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_bus_model as kit  # noqa: E402  (box() and mesh())
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
BODY = ROOT / "assets/verda/vehicles/runtime/sedan_body.glb"

PAINT = (122, 26, 38)
PAINT_DARK = (92, 18, 28)
CHROME = (200, 202, 204)
BLACK = (20, 20, 22)
DARK = (34, 34, 36)
GLASS = (40, 56, 64, 120)
SEAT = (150, 128, 100)
LAMP = (255, 246, 214)
AMBER = (255, 168, 36)
RED = (200, 26, 26)
PLATE = (236, 232, 220)

W = .9                      # half width
FRONT, REAR = 2.35, -2.35
AXLES = (1.45, -1.35)
DOORS = (.9, -.15, -1.2)    # door shut lines (front edge of each door, then the rear edge)


def build():
    kit.boxes.clear()
    box = kit.box
    # Interior: floor, front and rear benches, the dash and the wheel.
    box(-W + .08, W - .08, .3, .34, -1.3, .9, DARK)
    for z0, z1 in ((-.55, -.05), (-1.3, -.85)):
        box(-W + .12, W - .12, .34, .58, z0, z1, SEAT)
        box(-W + .12, W - .12, .58, 1.05, z0 - .08, z0 + .06, SEAT)
    box(-W + .1, W - .1, .62, .86, .62, .9, DARK)
    box(.3, .5, .8, .98, .5, .54, BLACK)
    # Lower body, bonnet and boot.
    box(-W, W, .26, .8, REAR, FRONT, PAINT)
    box(-W + .03, W - .03, .8, .84, .95, FRONT - .04, PAINT)            # bonnet
    box(-W + .03, W - .03, .8, .88, REAR + .04, -1.4, PAINT)            # boot lid
    box(-.03, .03, .84, .845, 1.0, FRONT - .1, PAINT_DARK)              # bonnet crease
    for side in (-1, 1):
        box(side * (W - .02), side * (W + .05), .2, .3, REAR + .55, FRONT - .55, BLACK)   # sills
        box(side * W, side * (W + .012), .6, .64, REAR + .1, FRONT - .1, CHROME)           # waist strip
        for z in DOORS:
            box(side * W, side * (W + .008), .3, .8, z - .012, z + .012, PAINT_DARK)       # shut lines
        for z in (.55, -.5):
            box(side * W, side * (W + .025), .68, .72, z - .07, z + .07, CHROME)            # handles
        box(side * W, side * (W + .12), .86, .96, .78, .9, PAINT_DARK)                      # mirrors
        for z in AXLES:
            box(side * (W - .02), side * (W + .01), .26, .62, z - .44, z + .44, BLACK)       # arches
    # The cabin: glass all round, chrome surrounds, a maroon roof and pillars.
    box(-W + .08, W - .08, .84, 1.36, -1.35, .95, GLASS)
    box(-W + .1, W - .1, 1.36, 1.44, -1.15, .75, PAINT)
    for side in (-1, 1):
        box(side * (W - .1), side * (W - .06), .84, 1.4, -.18, -.1, PAINT)                 # B pillars
        box(side * (W - .09), side * (W - .07), 1.33, 1.37, -1.3, .9, CHROME)              # window tops
        for z in (-1.33, .93):
            box(side * (W - .1), side * (W - .05), .84, 1.38, z - .05, z + .05, PAINT)     # A and C pillars
    # Nose: chrome bumper, slatted grille, square headlamps, indicators, plate.
    box(-W - .02, W + .02, .3, .44, FRONT, FRONT + .1, CHROME)
    box(-.5, .5, .5, .76, FRONT - .02, FRONT + .01, CHROME)
    for y in (.54, .6, .66, .72):
        box(-.46, .46, y, y + .03, FRONT + .01, FRONT + .02, BLACK)
    for side in (-1, 1):
        box(side * .56, side * .84, .54, .74, FRONT - .02, FRONT + .01, LAMP)
        box(side * .62, side * .82, .45, .5, FRONT, FRONT + .02, AMBER)
    box(-.24, .24, .32, .42, FRONT + .1, FRONT + .11, PLATE)
    # Tail: chrome bumper, wide tail lamps, plate.
    box(-W - .02, W + .02, .3, .44, REAR - .1, REAR, CHROME)
    for side in (-1, 1):
        box(side * .3, side * .86, .56, .74, REAR - .01, REAR + .02, RED)
        box(side * .3, side * .45, .5, .55, REAR - .01, REAR + .02, AMBER)
    box(-.24, .24, .48, .58, REAR - .02, REAR - .01, PLATE)
    box(.5, .6, .2, .28, REAR - .2, REAR + .1, DARK)                     # exhaust
    return truck.glb(kit.mesh(), "verda_sedano", True)


if __name__ == "__main__":
    data = build()
    if "--check" in sys.argv:
        if not BODY.exists() or BODY.read_bytes() != data:
            raise SystemExit(f"Stale model {BODY.name}: run {Path(__file__).name}")
    else:
        BODY.write_bytes(data)
        print(f"wrote {BODY.relative_to(ROOT)}: {len(data)} bytes")
