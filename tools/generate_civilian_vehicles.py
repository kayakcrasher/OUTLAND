#!/usr/bin/env python3
"""Generate four more of Verda's everyday vehicles (assets/verda/vehicles/runtime/<id>_body.glb).

- wagon    Verda Familio: a long-roof estate car in olive green with wood-grain sides, a roof rack.
- muscle   Verda Tempesto: a two-door muscle coupe - long bonnet with a scoop, fastback, twin white
           racing stripes over deep blue, fat chrome bumpers, twin exhausts.
- van      Verda Furgono: a panel van - a short cab and a tall white cargo box with a green band,
           twin rear doors.
- flatbed  Verda Platformo: a working flatbed - an orange cab, a timber deck with stake sides.

Built from boxes with vertex colours like the other vehicles. +Z is the front, Y is up, metres,
ground at Y = 0; the driver sits on the left (+X). The wheels are the pickup's.

    python3 tools/generate_civilian_vehicles.py           # write the models
    python3 tools/generate_civilian_vehicles.py --check   # fail if a committed model is stale
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_bus_model as kit  # noqa: E402  (box() and mesh())
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/verda/vehicles/runtime"

CHROME = (200, 202, 204)
BLACK = (20, 20, 22)
DARK = (28, 28, 30)   # (under 90 in all: the repaint leaves it be)
GLASS = (40, 56, 64, 120)
SEAT = (150, 128, 100)
LAMP = (255, 246, 214)
AMBER = (255, 168, 36)
RED = (200, 26, 26)
PLATE = (236, 232, 220)
WOOD = (122, 78, 44)
WOOD_DARK = (92, 58, 32)


def interior(box, w, z_front, z_rear, floor, seat_rows):
    box(-w + .08, w - .08, floor, floor + .04, z_rear, z_front, DARK)
    for z0 in seat_rows:
        box(-w + .12, w - .12, floor + .04, floor + .28, z0, z0 + .5, SEAT)
        box(-w + .12, w - .12, floor + .28, floor + .75, z0 - .08, z0 + .06, SEAT)


def nose(box, w, front, paint_dark, lamps_y=.6, grille=(.5, .76)):
    box(-w - .02, w + .02, .3, .44, front, front + .1, CHROME)
    box(-.5, .5, grille[0], grille[1], front - .02, front + .01, BLACK)
    for side in (-1, 1):
        box(side * .56, side * (w - .06), lamps_y - .08, lamps_y + .1, front - .02, front + .01, LAMP)
        box(side * .62, side * (w - .08), .45, .5, front, front + .02, AMBER)
    box(-.24, .24, .32, .42, front + .1, front + .11, PLATE)


def tail(box, w, rear, y=.56):
    box(-w - .02, w + .02, .3, .44, rear - .1, rear, CHROME)
    for side in (-1, 1):
        box(side * .3, side * (w - .04), y, y + .18, rear - .01, rear + .02, RED)
        box(side * .3, side * .45, y - .06, y - .01, rear - .01, rear + .02, AMBER)
    box(-.24, .24, y - .08, y + .02, rear - .02, rear - .01, PLATE)


def arches(box, w, axles, top=.62):
    for side in (-1, 1):
        for z in axles:
            box(side * (w - .02), side * (w + .01), .26, top, z - .44, z + .44, BLACK)


def wagon():
    kit.boxes.clear()
    box = kit.box
    paint, dark = (86, 104, 58), (64, 78, 44)
    w, front, rear = .9, 2.4, -2.35
    interior(box, w, .9, -2.2, .3, (-.55, -1.3))
    box(-w + .12, w - .12, .34, .4, -2.2, -1.5, DARK)                       # load floor
    box(-w, w, .26, .8, rear, front, paint)                                 # lower body
    box(-w + .03, w - .03, .8, .84, .95, front - .04, paint)                # bonnet
    for side in (-1, 1):
        box(side * w, side * (w + .015), .38, .74, rear + .15, front - .7, WOOD)        # wood-grain sides
        for z in (rear + .15, front - .7):
            box(side * w, side * (w + .02), .38, .74, z - .03, z + .03, WOOD_DARK)
        box(side * (w - .02), side * (w + .05), .2, .3, rear + .5, front - .55, BLACK)  # sills
        box(side * w, side * (w + .12), .86, .96, .78, .9, dark)                         # mirrors
        box(side * (w - .1), side * (w - .05), .84, 1.46, -.2, -.12, paint)              # B pillar
        box(side * (w - .1), side * (w - .05), .84, 1.46, -1.25, -1.17, paint)           # C pillar
        box(side * (w - .1), side * (w - .05), .84, 1.46, rear + .06, rear + .14, paint) # D pillar
        box(side * (w - .1), side * (w - .05), .84, 1.44, .9, .98, paint)                # A pillar
    arches(box, w, (1.45, -1.4))
    # The long roof over glass all round, and a chrome roof rack.
    box(-w + .08, w - .08, .84, 1.42, rear + .08, .95, GLASS)
    box(-w + .1, w - .1, 1.42, 1.5, rear + .06, .78, paint)
    for x in (-.62, .62):
        box(x - .03, x + .03, 1.5, 1.6, rear + .3, .5, CHROME)
    for z in (rear + .5, -.6, .3):
        box(-.66, .66, 1.58, 1.61, z - .03, z + .03, CHROME)
    nose(box, w, front, dark)
    tail(box, w, rear)
    box(.5, .6, .2, .28, rear - .2, rear + .1, DARK)
    return truck.glb(kit.mesh(), "verda_familio", True)


def muscle():
    kit.boxes.clear()
    box = kit.box
    paint, dark, stripe = (24, 42, 104), (20, 35, 87), (236, 236, 230)   # (dark close enough to repaint with it)
    w, front, rear = .94, 2.45, -2.3
    interior(box, w, .5, -1.4, .28, (-.5,))
    box(-w + .12, w - .12, .32, .6, -1.35, -.95, SEAT)                      # rear bench
    box(-w, w, .24, .74, rear, front, paint)
    box(-w + .03, w - .03, .74, .79, .55, front - .04, paint)               # the long bonnet
    box(-.28, .28, .79, .9, 1.0, 1.7, dark)                                 # scoop
    box(-.22, .22, .82, .88, 1.7, 1.72, BLACK)
    for side in (-1, 1):
        box(side * (w - .02), side * (w + .05), .18, .28, rear + .55, front - .6, BLACK)
        box(side * w, side * (w + .02), .5, .54, rear + .2, front - .2, CHROME)
        box(side * w, side * (w + .008), .28, .74, .5 - .012, .5 + .012, dark)          # door shut line
        box(side * w, side * (w + .008), .28, .74, -.75 - .012, -.75 + .012, dark)
        box(side * w, side * (w + .12), .8, .9, .48, .58, dark)
    arches(box, w, (1.5, -1.4), top=.66)
    # Cabin and a fastback stepping down to the tail.
    box(-w + .1, w - .1, .79, 1.24, -.9, .5, GLASS)
    box(-w + .12, w - .12, 1.24, 1.3, -.75, .3, paint)
    for k, (z0, z1, top) in enumerate(((-1.25, -.9, 1.16), (-1.6, -1.25, 1.02), (-1.95, -1.6, .9), (rear + .04, -1.95, .8))):
        box(-w + .1, w - .1, .74, top, z0, z1, GLASS if k < 2 else paint)
    for side in (-1, 1):
        box(side * (w - .1), side * (w - .05), .79, 1.28, .45, .55, paint)
    # Twin white stripes front to back.
    for x in (-.22, .1):
        box(x, x + .12, .79, .8, .55, front - .04, stripe)
        box(x, x + .12, 1.3, 1.31, -.75, .3, stripe)
        box(x, x + .12, .8, .81, rear + .04, -1.95, stripe)
    nose(box, w, front, dark, lamps_y=.58, grille=(.46, .7))
    tail(box, w, rear, y=.54)
    box(-.92, .92, .76, .82, rear + .02, rear + .2, dark)                   # ducktail
    for x in (.42, .62):
        box(x - .06, x + .06, .2, .3, rear - .25, rear + .1, CHROME)        # twin exhausts
    return truck.glb(kit.mesh(), "verda_tempesto", True)


def van():
    kit.boxes.clear()
    box = kit.box
    white, grey, green = (232, 232, 226), (190, 192, 190), (30, 132, 70)
    w, front, rear, split = 1.0, 2.6, -2.6, 1.15
    interior(box, w, 2.0, split, .42, (1.3,))
    box(-w, w, .3, 1.0, split, front, white)                                # cab lower
    box(-w + .04, w - .04, 1.0, 1.04, 1.95, front - .04, white)             # short bonnet
    box(-w + .08, w - .08, 1.04, 1.9, split + .05, 1.95, GLASS)             # windscreen and cab windows
    box(-w + .06, w - .06, 1.9, 2.08, split, 1.95, white)                   # cab roof
    for side in (-1, 1):
        box(side * (w - .1), side * (w - .05), 1.0, 1.95, 1.9, 1.98, white)
        box(side * w, side * (w + .14), 1.25, 1.4, 1.85, 1.95, BLACK)       # mirrors
        box(side * (w - .02), side * (w + .05), .2, .32, rear + .4, front - .6, BLACK)
    # The cargo box, a green band and a lighter roof.
    box(-w, w, .3, 2.55, rear, split, white)
    box(-w - .01, w + .01, 1.3, 1.6, rear + .05, split - .05, green)
    box(-w + .02, w - .02, 2.55, 2.6, rear, split, grey)
    box(-.01, .01, .35, 2.5, rear - .01, rear, BLACK)                       # rear doors' split
    for x in (-.3, .3):
        box(x - .08, x + .08, 1.2, 1.26, rear - .03, rear - .01, CHROME)
    arches(box, w, (1.75, -1.65), top=.7)
    nose(box, w, front, grey, lamps_y=.78, grille=(.62, .9))
    tail(box, w, rear, y=.62)
    return truck.glb(kit.mesh(), "verda_furgono", True)


def flatbed():
    kit.boxes.clear()
    box = kit.box
    cab, cab_dark = (220, 112, 32), (176, 86, 22)
    w, front, rear, split = 1.0, 2.95, -2.95, 1.25
    interior(box, w, 2.2, split, .45, (1.45,))
    box(-w, w, .32, 1.05, split, front, cab)
    box(-w + .04, w - .04, 1.05, 1.1, 2.25, front - .04, cab)
    box(-w + .08, w - .08, 1.1, 1.92, split + .08, 2.25, GLASS)
    box(-w + .06, w - .06, 1.92, 2.08, split, 2.25, cab)
    box(-.6, .6, 2.08, 2.18, 1.5, 2.1, AMBER)                               # beacon bar
    for side in (-1, 1):
        box(side * (w - .1), side * (w - .05), 1.05, 1.95, 2.2, 2.28, cab_dark)
        box(side * w, side * (w + .14), 1.3, 1.45, 2.15, 2.25, BLACK)
    # Chassis, the timber deck and its stake sides, a headboard behind the cab.
    box(-.55, .55, .35, .6, rear + .1, split, BLACK)
    box(-w, w, .9, 1.02, rear, split - .1, WOOD)
    for z in range(8):
        zz = rear + .2 + z * .52
        box(-w, w, 1.02, 1.025, zz, zz + .03, WOOD_DARK)
    box(-w, w, 1.02, 2.0, split - .2, split - .1, DARK)                     # headboard
    for side in (-1, 1):
        for z in (rear + .15, -1.4, -.1, split - .3):
            box(side * (w - .08), side * w, 1.02, 1.55, z - .05, z + .05, DARK)
        box(side * (w - .06), side * w, 1.45, 1.52, rear + .1, split - .25, DARK)
    for x in (-.85, .85):
        box(x - .1, x + .1, .5, .9, rear + .1, rear + .3, BLACK)            # rear mudflaps
    arches(box, w, (1.95, -1.9), top=.72)
    nose(box, w, front, cab_dark, lamps_y=.8, grille=(.6, .98))
    tail(box, w, rear, y=.66)
    return truck.glb(kit.mesh(), "verda_platformo", True)


MODELS = {"wagon": wagon, "muscle": muscle, "van": van, "flatbed": flatbed}

if __name__ == "__main__":
    stale = []
    for ident, build in MODELS.items():
        path = OUT / f"{ident}_body.glb"
        data = build()
        if "--check" in sys.argv:
            if not path.exists() or path.read_bytes() != data:
                stale.append(path.name)
        else:
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {len(data)} bytes")
    if stale:
        raise SystemExit("Stale models: " + ", ".join(stale) + f" - run {Path(__file__).name}")
