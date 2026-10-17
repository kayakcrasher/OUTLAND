#!/usr/bin/env python3
"""Generate La Verda Mano's sports car (assets/verda/vehicles/runtime/green_hand_gt_body.glb).

The Verda Mano GT: a low, wide two-door coupe in the Green Hand's banner. Pearl white, with the
banner's green cross laid over it - a wide green stripe from the nose over the roof to the tail,
crossed by a green band over the roof and down both doors - edged in gold pinstripe. A long
bonnet with twin vents, wide rear haunches, side skirts, a rear wing on gold endplates, slim
lamps and four exhausts. The game paints the banner's star, hand and crown on the bonnet and the
banner on both doors (game::mafia::draw_green_hand_livery).

Built from boxes with vertex colours like the other vehicles. +Z is the front, Y is up, metres,
ground at Y = 0; the driver sits on the left (+X). The wheels are the pickup's.

    python3 tools/generate_green_hand_car.py           # write the model
    python3 tools/generate_green_hand_car.py --check   # fail if the committed model is stale
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_bus_model as kit  # noqa: E402  (box() and mesh())
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
BODY = ROOT / "assets/verda/vehicles/runtime/green_hand_gt_body.glb"

PEARL = (240, 242, 236)
PEARL_SHADE = (214, 218, 212)
GREEN = (0, 153, 0)
GREEN_DARK = (0, 96, 40)
GOLD = (232, 186, 52)
BLACK = (18, 19, 22)
DARK = (30, 30, 32)
CHROME = (196, 198, 200)
GLASS = (30, 46, 40, 120)
SEAT = (24, 60, 34)
LAMP = (255, 246, 210)
AMBER = (255, 168, 36)
RED = (210, 24, 24)

W = .97                     # half width
FRONT, REAR = 2.3, -2.3
AXLES = (1.4, -1.35)
STRIPE = .26                # half width of the long stripe
BAND = (-.55, .05)          # the cross band over the roof and doors (z range)


def build():
    kit.boxes.clear()
    box = kit.box
    # Interior: floor, two bucket seats in green, the dash, a gold-rimmed wheel.
    box(-W + .08, W - .08, .3, .34, -1.0, 1.0, DARK)
    for x in (.42, -.42):
        box(x - .26, x + .26, .34, .56, -.75, -.2, SEAT)
        box(x - .26, x + .26, .56, 1.02, -.8, -.68, SEAT)
    box(-W + .1, W - .1, .56, .8, .55, .8, DARK)
    box(.32, .52, .7, .9, .38, .42, GOLD)
    # Lower body: pearl, with the band (the cross's arm) down both doors, skirts, haunches.
    box(-W, W, .24, .7, FRONT - 1.35, FRONT, PEARL)
    box(-W, W, .24, .7, REAR, BAND[0], PEARL)
    box(-W, W, .24, .7, BAND[1], FRONT - 1.35, PEARL)
    box(-W, W, .24, .7, BAND[0], BAND[1], GREEN)
    for side in (-1, 1):
        box(side * (W - .02), side * (W + .06), .2, .3, REAR + .5, FRONT - .55, BLACK)          # skirts
        box(side * W, side * (W + .07), .3, .66, REAR + .15, REAR + 1.3, PEARL_SHADE)           # haunches
        for z in BAND:                                                                          # gold edges of the band
            box(side * W, side * (W + .008), .24, .7, z - .02, z + .02, GOLD)
        box(side * W, side * (W + .008), .55, .57, REAR + .1, FRONT - .1, GOLD)                 # waist pinstripe
        box(side * W, side * (W + .02), .58, .62, -.15, -.02, CHROME)                           # door handle
        box(side * W, side * (W + .13), .74, .82, .55, .65, GREEN_DARK)                         # mirrors
    # The long stripe over the bonnet and the boot (the cross's upright), gold-edged.
    box(-W + .03, W - .03, .7, .74, FRONT - 1.35, FRONT - .03, PEARL)
    box(-STRIPE, STRIPE, .74, .745, FRONT - 1.35, FRONT - .03, GREEN)
    box(-W + .03, W - .03, .7, .77, REAR + .03, -1.15, PEARL)
    box(-STRIPE, STRIPE, .77, .775, REAR + .03, -1.15, GREEN)
    for x in (-STRIPE, STRIPE):
        box(x - .012, x + .012, .74, .748, FRONT - 1.35, FRONT - .03, GOLD)
        box(x - .012, x + .012, .77, .778, REAR + .03, -1.15, GOLD)
    for x in (-.55, .55):                                                                       # bonnet vents
        box(x - .14, x + .14, .74, .76, FRONT - 1.0, FRONT - .55, BLACK)
    # Greenhouse: low and raked, tinted green; a pearl roof crossed by the stripe and the band.
    box(-W + .1, W - .1, .7, 1.12, -1.15, .95, GLASS)
    box(-W + .12, W - .12, 1.12, 1.18, -.95, .65, PEARL)
    box(-STRIPE, STRIPE, 1.18, 1.185, -.95, .65, GREEN)
    box(-W + .12, W - .12, 1.18, 1.185, BAND[0], BAND[1], GREEN)
    for z in (-1.1, .9):
        for side in (-1, 1):
            box(side * (W - .12), side * (W - .06), .7, 1.16, z - .05, z + .05, BLACK)
    # The wing: black blade on gold endplates, the stripe across it.
    box(-.9, .9, 1.02, 1.06, REAR + .05, REAR + .38, BLACK)
    box(-STRIPE, STRIPE, 1.06, 1.065, REAR + .05, REAR + .38, GREEN)
    for side in (-1, 1):
        box(side * .84, side * .9, .74, 1.08, REAR + .05, REAR + .4, GOLD)
    # Nose and tail: a low black splitter, slim headlamps, a full-width light bar at the back,
    # four exhausts.
    box(-W, W, .18, .26, FRONT - .05, FRONT + .1, BLACK)
    box(-.6, .6, .3, .5, FRONT - .02, FRONT + .01, BLACK)                                       # grille
    box(-.12, .12, .36, .46, FRONT + .01, FRONT + .02, GOLD)                                    # the badge
    for side in (-1, 1):
        box(side * .62, side * .92, .6, .66, FRONT - .03, FRONT, LAMP)
        box(side * .62, side * .92, .54, .58, FRONT - .03, FRONT, AMBER)
        for x in (.32, .48):
            box(side * (x - .05), side * (x + .05), .24, .32, REAR - .1, REAR, CHROME)
    box(-.92, .92, .6, .66, REAR - .02, REAR, RED)
    box(-.7, .7, .2, .34, REAR - .04, REAR, BLACK)                                              # diffuser
    for z in AXLES:
        for side in (-1, 1):
            box(side * (W - .02), side * (W + .01), .24, .58, z - .42, z + .42, DARK)
    return truck.glb(kit.mesh(), "verda_mano_gt", True)


if __name__ == "__main__":
    data = build()
    if "--check" in sys.argv:
        if not BODY.exists() or BODY.read_bytes() != data:
            raise SystemExit(f"Stale model {BODY.name}: run {Path(__file__).name}")
    else:
        BODY.write_bytes(data)
        print(f"wrote {BODY.relative_to(ROOT)}: {len(data)} bytes")
