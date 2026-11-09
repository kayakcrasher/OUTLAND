#!/usr/bin/env python3
"""Generate the Verda Fulmo motorcycle (assets/verda/vehicles/runtime/motorcycle_body.glb) and its
spoked wheel (motorcycle_wheel.glb).

A standard roadster: a tubular frame, a green tank with a cream stripe and a gold badge, a long
black saddle, an air-cooled twin with finned cylinders, twin chrome pipes down the right side, a
raked fork with a round headlamp and flat bars, mirrors, fenders front and back, a tail lamp and a
plate. Built from boxes with vertex colours like the other vehicles. +Z is the front, Y is up,
metres, ground at Y = 0, 2.15 m long; the wheels (radius 0.31) sit at z +0.72 and -0.70 and are
drawn by the game from motorcycle_wheel.glb.

    python3 tools/generate_motorcycle.py           # write the models
    python3 tools/generate_motorcycle.py --check   # fail if a committed model is stale
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_bus_model as kit  # noqa: E402  (box() and mesh())
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/verda/vehicles/runtime"

GREEN = (18, 112, 64)
GREEN_DARK = (12, 78, 44)
CREAM = (232, 222, 196)
GOLD = (214, 172, 74)
CHROME = (200, 202, 206)
STEEL = (64, 66, 70)
BLACK = (20, 20, 22)
ENGINE = (150, 152, 156)
FIN = (110, 112, 116)
LAMP = (255, 246, 214)
RED = (200, 26, 26)
AMBER = (255, 168, 36)
PLATE = (236, 232, 220)

FRONT_AXLE, REAR_AXLE, WHEEL_R = .72, -.70, .31


def tube(box, a, b, size, colour, steps=6):
    """A straight tube from a to b (y, z) along the bike's centre line, built from stepped boxes."""
    for k in range(steps):
        t0, t1 = k / steps, (k + 1) / steps
        y0, z0 = a[0] + (b[0] - a[0]) * t0, a[1] + (b[1] - a[1]) * t0
        y1, z1 = a[0] + (b[0] - a[0]) * t1, a[1] + (b[1] - a[1]) * t1
        box(-size, size, min(y0, y1) - size, max(y0, y1) + size, min(z0, z1) - size, max(z0, z1) + size, colour)


def body():
    kit.boxes.clear()
    box = kit.box
    # The frame: the backbone from the steering head to the swingarm pivot, the down tube to the
    # engine's cradle, the seat rails back to the tail, and the swingarm out to the rear axle.
    head = (.98, .52)
    tube(box, head, (.86, -.38), .035, STEEL)                     # backbone under the tank and saddle
    tube(box, head, (.36, .30), .035, STEEL)                      # down tube
    tube(box, (.36, .30), (.30, -.22), .03, STEEL, 4)             # cradle under the engine
    for side in (-1, 1):                                          # swingarm, either side of the wheel
        x = side * .1
        for k in range(6):
            t0, t1 = k / 6, (k + 1) / 6
            z0, z1 = -.22 + (REAR_AXLE + .22) * t0, -.22 + (REAR_AXLE + .22) * t1
            y0, y1 = .34 + (WHEEL_R - .34) * t0, .34 + (WHEEL_R - .34) * t1
            box(x - .02, x + .02, min(y0, y1) - .025, max(y0, y1) + .025, min(z0, z1), max(z0, z1), STEEL)
        box(x - .025, x + .025, .36, .74, -.66, -.58, CHROME)     # shock absorbers
        box(x - .03, x + .03, .5, .62, -.66, -.58, AMBER)         # (their springs)
    # The engine: an air-cooled twin, its finned cylinders standing up in a V, the case below.
    box(-.16, .16, .24, .44, -.22, .30, ENGINE)
    box(-.18, .18, .26, .36, -.10, .18, FIN)
    for z, lean in ((.18, 1), (-.08, -1)):
        for k in range(5):
            y = .44 + k * .07
            zz = z + lean * k * .025
            box(-.13, .13, y, y + .045, zz - .07, zz + .07, FIN)
            box(-.10, .10, y + .045, y + .07, zz - .05, zz + .05, ENGINE)
    box(-.06, .06, .78, .83, -.05, .12, BLACK)                    # air box under the tank
    # Twin pipes down the right side, chrome silencers out the back.
    for k, y in enumerate((.26, .36)):
        box(.17, .23, y, y + .06, -.15 + k * .05, .25, CHROME)
        box(.19, .25, y + .02, y + .1, -.92, -.15, CHROME)
        box(.2, .24, y + .03, y + .09, -.95, -.92, BLACK)
    # The tank: green over the backbone, a cream stripe and a gold badge each side.
    box(-.15, .15, .84, 1.02, -.02, .48, GREEN)
    box(-.12, .12, 1.02, 1.07, .02, .44, GREEN)
    box(-.155, .155, .9, .95, .02, .46, CREAM)
    for side in (-1, 1):
        box(side * .155 - .005, side * .155 + .005, .93, 1.0, .16, .3, GOLD)
    box(-.04, .04, 1.07, 1.09, .2, .28, CHROME)                    # filler cap
    # The saddle, long and flat, over the seat rails; a grab rail behind it.
    box(-.14, .14, .83, .92, -.62, -.02, BLACK)
    box(-.12, .12, .92, .95, -.58, -.06, BLACK)
    box(-.13, .13, .9, .94, -.72, -.62, CHROME)
    # The tail: the rear fender over the wheel, the lamp and the plate.
    box(-.09, .09, .62, .66, -1.05, -.48, GREEN_DARK)
    box(-.09, .09, .5, .62, -1.07, -1.03, GREEN_DARK)
    box(-.06, .06, .66, .72, -1.07, -1.02, RED)
    for side in (-1, 1):
        box(side * .12 - .025, side * .12 + .025, .63, .68, -.98, -.94, AMBER)
    box(-.09, .09, .44, .56, -1.075, -1.06, PLATE)
    # The fork: two chrome legs raked back from the front axle to the steering head, the yokes.
    for side in (-1, 1):
        x = side * .08
        for k in range(7):
            t0, t1 = k / 7, (k + 1) / 7
            z0, z1 = FRONT_AXLE + (head[1] - FRONT_AXLE) * t0, FRONT_AXLE + (head[1] - FRONT_AXLE) * t1
            y0, y1 = WHEEL_R + (head[0] + .08 - WHEEL_R) * t0, WHEEL_R + (head[0] + .08 - WHEEL_R) * t1
            box(x - .022, x + .022, y0, y1, min(z0, z1) - .02, max(z0, z1) + .02, CHROME if k < 4 else STEEL)
    box(-.11, .11, .98, 1.04, .5, .6, STEEL)                       # top yoke
    box(-.11, .11, .86, .9, .54, .64, STEEL)                       # bottom yoke
    # The front fender hugging the wheel.
    box(-.08, .08, .63, .67, .5, .98, GREEN_DARK)
    box(-.08, .08, .52, .63, .95, 1.0, GREEN_DARK)
    # The headlamp: a round-ish chrome bucket and its lens; indicators either side.
    box(-.1, .1, .9, 1.08, .64, .78, CHROME)
    box(-.12, .12, .92, 1.06, .66, .76, CHROME)
    box(-.08, .08, .93, 1.05, .78, .8, LAMP)
    for side in (-1, 1):
        box(side * .17 - .025, side * .17 + .025, .95, 1.0, .62, .67, AMBER)
    # Flat bars, black grips, levers and round mirrors on stalks; the clocks.
    box(-.36, .36, 1.1, 1.13, .5, .54, CHROME)
    box(-.1, .1, 1.05, 1.1, .5, .54, STEEL)
    for side in (-1, 1):
        box(side * .39 - .03, side * .39 + .03, 1.09, 1.14, .49, .55, BLACK)
        box(side * .3 - .01, side * .3 + .01, 1.11, 1.38, .5, .52, STEEL)
        box(side * .3 - .05, side * .3 + .05, 1.36, 1.44, .49, .52, CHROME)
        box(side * .26, side * .38, 1.1, 1.12, .55, .6, STEEL)     # levers
    box(-.07, .07, 1.08, 1.16, .58, .64, BLACK)
    box(-.05, .05, 1.1, 1.15, .64, .645, CREAM)
    # Footpegs and the side stand's foot.
    for side in (-1, 1):
        box(side * .2, side * .3, .3, .33, -.08, -.02, BLACK)
        box(side * .2, side * .26, .34, .37, -.42, -.36, BLACK)    # the pillion's
    box(-.24, -.21, .02, .3, .05, .09, STEEL)
    return truck.glb(kit.mesh(), "verda_fulmo", False)


def wheel():
    """A 1 m spoked wheel (narrow: x -0.1..0.1) with tread at y = 0, axle along X through y = 0.5."""
    positions, normals, colours, indices = [], [], [], []
    segments = 18

    def ring(radius, x, colour, outward):
        start = len(positions)
        for i in range(segments):
            a = 2 * math.pi * i / segments
            positions.append((x, .5 + radius * math.cos(a), radius * math.sin(a)))
            normals.append((0, math.cos(a), math.sin(a)) if outward else (1 if x > 0 else -1, 0, 0))
            colours.append(colour + (255,))
        return start

    def strip(a, b):
        for i in range(segments):
            j = (i + 1) % segments
            indices.extend([a + i, b + i, b + j, a + i, b + j, a + j])

    tyre, rim, hub = (24, 24, 26), (196, 198, 202), (150, 152, 156)
    outer_l, outer_r = ring(.5, -.1, tyre, True), ring(.5, .1, tyre, True)
    strip(outer_l, outer_r)
    strip(outer_r, outer_l)
    for x in (-.1, .1):
        side = ring(.5, x, tyre, False)
        rim_ring = ring(.36, x * .8, rim, False)
        inner = ring(.32, x * .8, rim, False)
        strip(side, rim_ring)
        strip(rim_ring, side)
        strip(rim_ring, inner)
        strip(inner, rim_ring)
    # The spokes: thin bars from the hub out to the rim, crossing.
    def bar(a, b, w=.012):
        (y0, z0), (y1, z1) = a, b
        dy, dz = y1 - y0, z1 - z0
        length = math.hypot(dy, dz)
        ny, nz = -dz / length * w, dy / length * w
        for x in (-.03, .03):
            start = len(positions)
            for (py, pz) in ((y0 + ny, z0 + nz), (y0 - ny, z0 - nz), (y1 - ny, z1 - nz), (y1 + ny, z1 + nz)):
                positions.append((x, .5 + py, pz))
                normals.append((1 if x > 0 else -1, 0, 0))
                colours.append(rim + (255,))
            indices.extend([start, start + 1, start + 2, start, start + 2, start + 3,
                            start, start + 2, start + 1, start, start + 3, start + 2])
    for k in range(12):
        a = 2 * math.pi * k / 12
        b = a + .5
        bar((.06 * math.cos(a), .06 * math.sin(a)), (.33 * math.cos(b), .33 * math.sin(b)))
    # The hub: a drum in the middle, both sides.
    for x in (-.06, .06):
        drum = ring(.08, x, hub, False)
        centre = len(positions)
        positions.append((x, .5, 0))
        normals.append((1 if x > 0 else -1, 0, 0))
        colours.append(hub + (255,))
        for i in range(segments):
            j = (i + 1) % segments
            indices.extend([centre, drum + i, drum + j, centre, drum + j, drum + i])
    return truck.glb((positions, normals, colours, indices), "verda_fulmo_wheel", False)


MODELS = {"motorcycle_body": body, "motorcycle_wheel": wheel}

if __name__ == "__main__":
    stale = []
    for ident, build in MODELS.items():
        path = OUT / f"{ident}.glb"
        data = build()
        if "--check" in sys.argv:
            if not path.exists() or path.read_bytes() != data:
                stale.append(path.name)
        else:
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {len(data)} bytes")
    if stale:
        raise SystemExit("Stale models: " + ", ".join(stale) + f" - run {Path(__file__).name}")
