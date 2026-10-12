#!/usr/bin/env python3
"""Generate Verda's emergency vehicles (assets/verda/vehicles/runtime/police_body.glb, ambulance_body.glb).

Original, light models built from boxes with vertex colours, like the pickup truck:

  police     a four-door sedan in the Policio's navy and white: navy body, white doors and roof,
             a blue-and-white chequered band along the sides, a push bar, a roof light bar
             (red and blue lenses; the game makes them flash), see-through glass, seats and a cage.
  ambulance  a box ambulance on a van cab: white body, a green waist stripe and red crosses on the
             sides and back, amber and red light bar, rear doors, side door, tinted cab glass.

+Z is the front, Y is up, metres, ground at Y = 0; the driver sits on the left (+X). The wheels
are the pickup's (tools/generate_truck_model.py), scaled by the manifest's radius.

    python3 tools/generate_emergency_vehicles.py           # write the models
    python3 tools/generate_emergency_vehicles.py --check   # fail if the committed models are stale
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_bus_model as kit  # noqa: E402  (box() and mesh())
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
POLICE = ROOT / "assets/verda/vehicles/runtime/police_body.glb"
AMBULANCE = ROOT / "assets/verda/vehicles/runtime/ambulance_body.glb"

NAVY = (22, 36, 84)
NAVY_DARK = (14, 24, 58)
WHITE = (238, 240, 242)
WHITE_SHADE = (206, 210, 214)
CHROME = (196, 198, 200)
DARK = (30, 30, 32)
GLASS = (36, 52, 62, 110)
SEAT = (54, 54, 58)
LAMP = (255, 246, 210)
AMBER = (255, 168, 36)
RED = (210, 24, 24)
BLUE = (40, 90, 255)
GREEN = (0, 140, 70)


def arches(half_w, axles, sill, top, colour_dark):
    for z in axles:
        for side in (-1, 1):
            kit.box(side * (half_w - .02), side * (half_w + .01), sill, top, z - .45, z + .45, colour_dark)


def light_bar(y, z, width, left, right, length=.3):
    """A roof bar: dark base, two pairs of lenses (left colour on the driver's side, +X)."""
    kit.box(-width / 2, width / 2, y, y + .05, z - length / 2, z + length / 2, DARK)
    for i, x0 in enumerate((-width / 2 + .04, -width / 4, .02, width / 4)):
        colour = right if i < 2 else left
        kit.box(x0, x0 + width / 4 - .06, y + .05, y + .16, z - length / 2 + .03, z + length / 2 - .03, colour)


def police():
    kit.boxes.clear()
    W = .92
    FRONT, REAR = 2.4, -2.4
    # Interior under the glass: floor, seats, dashboard, the cage between the rows.
    kit.box(-W + .06, W - .06, .42, .46, -1.2, 1.1, DARK)
    for z in (.2, -.75):
        kit.box(-W + .12, W - .12, .46, .78, z - .3, z + .2, SEAT)
        kit.box(-W + .12, W - .12, .78, 1.2, z - .38, z - .26, SEAT)
    kit.box(-W + .08, W - .08, .7, .95, .9, 1.15, DARK)
    kit.box(.25, .55, .88, 1.12, .78, .84, DARK)
    for x in (-.6, -.3, 0, .3, .6):
        kit.box(x - .015, x + .015, .8, 1.28, -.3, -.27, CHROME)
    # Lower body (navy), white doors with the chequered band, bonnet, boot.
    kit.box(-W, W, .3, .82, FRONT - 1.2, FRONT, NAVY)            # front wings
    kit.box(-W, W, .3, .82, REAR, REAR + 1.15, NAVY)             # rear quarters
    kit.box(-W, W, .3, .82, REAR + 1.15, FRONT - 1.2, WHITE)     # the doors
    for k in range(10):                                           # chequers along the doors' waist
        z0 = REAR + 1.15 + k * .245
        for row, y0 in enumerate((.6, .7)):
            colour = BLUE if (k + row) % 2 == 0 else WHITE
            for side in (-1, 1):
                kit.box(side * W, side * (W + .008), y0, y0 + .1, z0, z0 + .245, colour)
    kit.box(-W + .04, W - .04, .82, .86, FRONT - 1.2, FRONT - .05, NAVY_DARK)   # bonnet
    kit.box(-W + .04, W - .04, .82, .88, REAR + .05, REAR + .9, NAVY_DARK)       # boot lid
    kit.box(-W + .05, W - .05, .86, 1.3, -1.25, 1.2, GLASS)     # the greenhouse
    kit.box(-W + .03, W - .03, 1.3, 1.36, -1.3, 1.22, WHITE)     # roof
    for z in (-1.28, -.27, 1.2):                                  # pillars
        for side in (-1, 1):
            kit.box(side * (W - .06), side * W, .82, 1.32, z - .05, z + .05, WHITE_SHADE)
    for side in (-1, 1):                                          # door gaps, handles, mirrors
        kit.box(side * W, side * (W + .005), .32, .82, -.27, -.25, WHITE_SHADE)
        kit.box(side * W, side * (W + .02), .74, .78, .45, .6, CHROME)
        kit.box(side * W, side * (W + .02), .74, .78, -.6, -.45, CHROME)
        kit.box(side * W, side * (W + .16), .88, 1.0, 1.05, 1.15, NAVY_DARK)
    light_bar(1.36, -.1, 1.3, RED, BLUE)
    # Front: grille, headlights, push bar; rear: tail lights, bumper.
    kit.box(-.5, .5, .4, .7, FRONT - .04, FRONT, DARK)
    for side in (-1, 1):
        kit.box(side * .55, side * .86, .56, .72, FRONT - .04, FRONT, LAMP)
        kit.box(side * .55, side * .86, .48, .55, FRONT - .04, FRONT, AMBER)
        kit.box(side * .6, side * .9, .6, .8, REAR - .02, REAR + .02, RED)
        kit.box(side * .35, side * .45, .25, .85, FRONT + .1, FRONT + .2, DARK)   # push bar uprights
    kit.box(-.5, .5, .4, .5, FRONT + .1, FRONT + .2, DARK)
    kit.box(-.5, .5, .72, .8, FRONT + .1, FRONT + .2, DARK)
    kit.box(-W - .02, W + .02, .28, .42, FRONT, FRONT + .08, DARK)
    kit.box(-W - .02, W + .02, .28, .42, REAR - .08, REAR, DARK)
    kit.box(-.22, .22, .5, .62, REAR - .09, REAR - .08, (226, 224, 214))
    arches(W, (1.4, -1.35), .3, .62, DARK)
    return truck.glb(kit.mesh(), "verda_police_car", True)


def cross(side_x, y, z, size, facing_x=True):
    """A red cross on a white panel, on a side (x = side_x) or on the back (z = side_x)."""
    a, b = size / 2, size / 6
    if facing_x:
        out = .01 if side_x > 0 else -.01
        kit.box(side_x, side_x + out, y - b, y + b, z - a, z + a, RED)
        kit.box(side_x, side_x + out, y - a, y + a, z - b, z + b, RED)
    else:
        kit.box(-a, a, y - b, y + b, side_x - .01, side_x, RED)
        kit.box(-b, b, y - a, y + a, side_x - .01, side_x, RED)


def ambulance():
    kit.boxes.clear()
    W = 1.05
    FRONT, REAR = 3.0, -3.0
    CAB = 1.15   # the box starts behind the cab
    # Cab interior and glass.
    kit.box(-W + .06, W - .06, .6, .64, CAB, FRONT - .9, DARK)
    kit.box(-W + .12, W - .12, .64, 1.0, CAB + .05, CAB + .55, SEAT)
    kit.box(-W + .12, W - .12, 1.0, 1.5, CAB + .02, CAB + .14, SEAT)
    kit.box(-W + .08, W - .08, 1.0, 1.25, FRONT - 1.05, FRONT - .85, DARK)
    kit.box(.3, .6, 1.15, 1.42, FRONT - 1.1, FRONT - 1.04, DARK)
    # Cab body: bonnet, doors, glass, roof.
    kit.box(-W, W, .38, 1.05, FRONT - .9, FRONT, WHITE)
    kit.box(-W + .04, W - .04, 1.05, 1.1, FRONT - .9, FRONT - .05, WHITE_SHADE)
    kit.box(-W, W, .38, 1.12, CAB, FRONT - .9, WHITE)
    kit.box(-W + .04, W - .04, 1.12, 1.78, CAB + .02, FRONT - .92, GLASS)
    kit.box(-W + .02, W - .02, 1.78, 1.86, CAB, FRONT - .88, WHITE)
    for side in (-1, 1):
        kit.box(side * (W - .06), side * W, 1.1, 1.82, FRONT - .96, FRONT - .88, WHITE_SHADE)
        kit.box(side * W, side * (W + .2), 1.35, 1.6, FRONT - 1.0, FRONT - .9, DARK)   # mirrors
    # The box: white, green waist stripe, red crosses, windows in the side door, rear doors.
    kit.box(-W - .05, W + .05, .38, 2.62, REAR, CAB, WHITE)
    kit.box(-W - .02, W + .02, 2.62, 2.66, REAR + .05, CAB - .05, WHITE_SHADE)
    for side in (-1, 1):
        x = side * (W + .05)
        out = .008 if side > 0 else -.008
        kit.box(x, x + out, 1.0, 1.22, REAR, CAB, GREEN)          # waist stripe
        kit.box(x, x + out, .38, .5, REAR, CAB, GREEN)            # sill
        cross(x + out, 1.85, REAR + 1.6, .9)
    # Kerb-side door (-X) with a window, rear doors with windows, steps.
    kit.box(-W - .07, -W - .06, .5, 2.4, CAB - 1.2, CAB - .2, WHITE_SHADE)
    kit.box(-W - .08, -W - .07, 1.7, 2.2, CAB - 1.05, CAB - .35, GLASS)
    kit.box(-.02, .02, .45, 2.5, REAR - .02, REAR, WHITE_SHADE)
    for side in (-1, 1):
        kit.box(side * .2, side * .8, 1.75, 2.25, REAR - .03, REAR - .01, GLASS)
        kit.box(side * .82, side * 1.06, .7, 1.0, REAR - .03, REAR, RED)    # tail lights
        kit.box(side * .82, side * 1.06, 2.35, 2.55, REAR - .03, REAR, AMBER)
    cross(REAR - .02, 1.35, 0, .55, facing_x=False)
    kit.box(-W, W, .28, .4, REAR - .25, REAR, DARK)                # rear step
    # Light bars: on the cab roof and the box's front corners.
    light_bar(1.86, FRONT - 1.2, 1.5, RED, AMBER, .28)
    for side in (-1, 1):
        kit.box(side * (W - .25), side * (W - .02), 2.5, 2.66, CAB + .01, CAB + .08, RED)
    # Front: grille, headlights, bumper.
    kit.box(-.55, .55, .5, .95, FRONT - .03, FRONT, DARK)
    for side in (-1, 1):
        kit.box(side * .6, side * .95, .72, .92, FRONT - .03, FRONT, LAMP)
        kit.box(side * .6, side * .95, .62, .7, FRONT - .03, FRONT, AMBER)
    kit.box(-W - .02, W + .02, .32, .5, FRONT, FRONT + .1, DARK)
    arches(W + .05, (2.0, -1.9), .38, .82, DARK)
    return truck.glb(kit.mesh(), "verda_ambulance", True)


if __name__ == "__main__":
    outputs = {POLICE: police(), AMBULANCE: ambulance()}
    for path, data in outputs.items():
        if "--check" in sys.argv:
            if not path.exists() or path.read_bytes() != data:
                raise SystemExit(f"Stale emergency vehicle model {path.name}: run {Path(__file__).name}")
        else:
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {len(data)} bytes")
