#!/usr/bin/env python3
"""Generate the Picolando Kuriero (assets/verda/vehicles/runtime/pizza_body.glb), Picolando's
pizza delivery car.

A short, bubbly three-door hatchback you can see coming a kilometre off: tomato-red paint with a
white roof, a chequered yellow-and-black band all round at the waist, a green-white-red flash over
the bonnet, yellow wheel arches, big round headlamps, and on the roof the giant lit slice - a
yellow wedge of cheese on its crust with pepperoni on top, pointing the way it goes, on a light
box with chequered sides. The hot box rides in the back, behind the glass.

Built from boxes with vertex colours like the other vehicles. +Z is the front, Y is up, metres,
ground at Y = 0; the driver sits on the left (+X). The wheels are the pickup's.

    python3 tools/generate_pizza_car.py           # write the model
    python3 tools/generate_pizza_car.py --check   # fail if the committed model is stale
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_bus_model as kit  # noqa: E402  (box() and mesh())
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
BODY = ROOT / "assets/verda/vehicles/runtime/pizza_body.glb"

RED = (214, 34, 30)
RED_DARK = (160, 22, 22)
WHITE = (244, 242, 236)
YELLOW = (255, 204, 32)
CHEESE = (255, 214, 72)
CRUST = (196, 128, 52)
PEPPERONI = (170, 36, 28)
BASIL = (48, 150, 64)
GREEN = (0, 146, 70)
BLACK = (20, 20, 22)
DARK = (34, 34, 36)
CHROME = (206, 208, 210)
GLASS = (40, 56, 64, 120)
SEAT = (60, 60, 64)
LAMP = (255, 248, 222)
AMBER = (255, 168, 36)
TAIL = (220, 24, 24)
PLATE = (236, 232, 220)
GLOW = (255, 236, 150)

W = .86                     # half width
FRONT, REAR = 2.0, -1.95
AXLES = (1.3, -1.25)


def build():
    kit.boxes.clear()
    box = kit.box
    # Interior: floor, two seats in front, the bench behind, the dash and the wheel, and the hot box
    # in the back (red, with the brand's chequer on its lid).
    box(-W + .08, W - .08, .3, .34, -1.75, 1.0, DARK)
    for x0, x1 in ((.08, W - .1), (-W + .1, -.08)):
        box(x0, x1, .34, .58, -.55, -.05, SEAT)
        box(x0, x1, .58, 1.08, -.62, -.48, SEAT)
    box(-W + .12, W - .12, .34, .56, -1.2, -.85, SEAT)
    box(-W + .1, W - .1, .62, .88, .66, 1.0, DARK)
    box(.28, .48, .82, 1.0, .54, .58, BLACK)
    box(-.55, .55, .34, .86, -1.82, -1.32, RED)                          # the hot box
    for i in range(6):
        box(-.55 + i * .1833, -.55 + (i + 1) * .1833, .86, .88, -1.82, -1.32, YELLOW if i % 2 == 0 else BLACK)
    # Lower body (red), bonnet with the tricolour flash, hatch.
    box(-W, W, .26, .82, REAR, FRONT, RED)
    box(-W + .03, W - .03, .82, .86, 1.0, FRONT - .04, RED)             # bonnet
    for i, colour in enumerate((GREEN, WHITE, RED_DARK)):
        x = -.27 + i * .18
        box(x, x + .18, .86, .865, 1.05, FRONT - .08, colour)
    for side in (-1, 1):
        box(side * (W - .02), side * (W + .05), .2, .3, REAR + .5, FRONT - .5, BLACK)     # sills
        # The chequered band at the waist, all down the side.
        n = 26
        for i in range(n):
            z0 = REAR + .08 + i * (FRONT - REAR - .16) / n
            z1 = z0 + (FRONT - REAR - .16) / n
            box(side * W, side * (W + .014), .56, .64, z0, z1, YELLOW if i % 2 == 0 else BLACK)
            box(side * W, side * (W + .014), .64, .72, z0, z1, BLACK if i % 2 == 0 else YELLOW)
        box(side * W, side * (W + .008), .3, .82, .3, .33, RED_DARK)                   # the door's shut lines
        box(side * W, side * (W + .008), .3, .82, -.95, -.92, RED_DARK)
        box(side * W, side * (W + .025), .76, .8, -.6, -.45, CHROME)                   # handle
        box(side * W, side * (W + .12), .88, .98, .85, .97, WHITE)                     # mirrors
        for z in AXLES:
            box(side * (W - .02), side * (W + .02), .26, .66, z - .44, z + .44, YELLOW)  # yellow arches
    # The cabin: glass all round under a white roof, white pillars.
    box(-W + .08, W - .08, .86, 1.36, -1.75, 1.0, GLASS)
    box(-W + .06, W - .06, 1.36, 1.46, -1.7, .82, WHITE)
    for side in (-1, 1):
        for z in (-1.72, -.2, .96):
            box(side * (W - .1), side * (W - .05), .86, 1.4, z - .05, z + .05, WHITE)
    # The roof sign: a light box with chequered sides, and the giant slice on it, point forward.
    box(-.42, .42, 1.46, 1.5, -1.1, .3, BLACK)                           # its rack
    box(-.36, .36, 1.5, 1.72, -.95, .15, GLOW)                           # the lit box
    for side in (-1, 1):
        for i in range(8):
            z0 = -.95 + i * .1375
            box(side * .36, side * .365, 1.5, 1.72, z0, z0 + .1375, RED if i % 2 == 0 else WHITE)
    # The slice: a stack of narrowing slabs from the crust at the back to the point at the front.
    steps = 14
    back, tip = -1.15, .55
    for i in range(steps):
        z0 = back + i * (tip - back) / steps
        z1 = z0 + (tip - back) / steps
        half = .62 * (1 - (i + .5) / steps) + .03
        box(-half, half, 1.72, 1.8, z0, z1, CHEESE)
    box(-.65, .65, 1.72, 1.86, back - .1, back + .04, CRUST)            # the crust along the back
    for x, z in ((-.28, -.85), (.26, -.8), (0, -.45), (-.12, -.05), (.1, .25)):
        box(x - .09, x + .09, 1.8, 1.82, z - .09, z + .09, PEPPERONI)    # pepperoni
    for x, z in ((.3, -.55), (-.3, -.5), (.08, -.75)):
        box(x - .04, x + .04, 1.8, 1.815, z - .03, z + .03, BASIL)      # basil
    # Nose: big round-ish headlamps, a chrome grin, yellow fog lamps; the plate.
    box(-W - .02, W + .02, .28, .42, FRONT, FRONT + .08, WHITE)
    box(-.36, .36, .46, .6, FRONT - .02, FRONT + .01, BLACK)
    box(-.3, .3, .5, .56, FRONT + .01, FRONT + .015, CHROME)
    for side in (-1, 1):
        box(side * .48, side * .78, .54, .78, FRONT - .02, FRONT + .015, LAMP)
        box(side * .52, side * .74, .58, .74, FRONT + .015, FRONT + .02, (255, 255, 255))
        box(side * .56, side * .74, .32, .4, FRONT + .08, FRONT + .1, YELLOW)
        box(side * .8, side * .85, .5, .56, FRONT - .02, FRONT + .01, AMBER)
    box(-.24, .24, .3, .4, FRONT + .08, FRONT + .09, PLATE)
    # Tail: a white bumper, tall tail lamps, the plate.
    box(-W - .02, W + .02, .28, .42, REAR - .08, REAR, WHITE)
    for side in (-1, 1):
        box(side * .6, side * .84, .52, .9, REAR - .01, REAR + .02, TAIL)
        box(side * .6, side * .84, .46, .52, REAR - .01, REAR + .02, AMBER)
    box(-.24, .24, .5, .6, REAR - .02, REAR - .01, PLATE)
    box(.45, .55, .2, .28, REAR - .2, REAR + .1, DARK)                   # exhaust
    return truck.glb(kit.mesh(), "picolando_kuriero", True)


if __name__ == "__main__":
    data = build()
    if "--check" in sys.argv:
        if not BODY.exists() or BODY.read_bytes() != data:
            raise SystemExit(f"Stale model {BODY.name}: run {Path(__file__).name}")
    else:
        BODY.write_bytes(data)
        print(f"wrote {BODY.relative_to(ROOT)}: {len(data)} bytes")
