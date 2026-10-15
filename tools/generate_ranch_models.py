#!/usr/bin/env python3
"""Generate the Vakeros' ranch country: vehicles, livestock, gear and farm buildings.

Vehicles (assets/verda/vehicles/runtime/, +Z front, Y up, metres, ground at Y = 0, driver on +X;
the game draws the four turning wheels from truck_wheel.glb at the manifest's anchors):
  sxs_body.glb            a two-seat side-by-side (UTV, like a Polaris Ranger / Can-Am Defender):
                          tube roll cage with a roof, two bucket seats behind a short hood, half
                          doors, a tilting cargo box, skid plate, brush guard, light bar.
  semi_body.glb           a long-nose conventional road tractor (an American 18-wheeler's tractor,
                          Peterbilt 379/389 style): square hood, chrome grille and bumper, air
                          cleaners, twin chrome stacks, sleeper cab, fuel tanks, fifth wheel; the
                          tandem's second drive axle (static duals) is part of the body.
  trailer_box.glb         a 16 m (53 ft) dry van, white, a green VERDA stripe, landing gear and a
                          spread tandem with duals at the back. Kingpin TRAILER_KINGPIN from the
                          nose; the game hitches it to the tractor (game::freight).
  trailer_cattle.glb      a 16 m "pot" livestock trailer: polished aluminium with rows of air slots,
                          a drop deck between the gooseneck and the rear axles, rear swing gate.
Livestock and gear (assets/verda/ranch/):
  cow_hereford.glb, cow_angus.glb   a beef cow (red with a white face / black), facing +Z.
  cowboy_hat.glb          a felt cattleman hat: pinched crown, band, brim curled up at the sides;
                          the crown's base ring is at Y = HAT_SEAT, the brim's front toward +Z.
Farm buildings and props (assets/verda/ranch/, catalogued in RanchAssetCatalog.hpp):
  barn.glb                a red gambrel barn 14 x 22 m, open sliding doors at both ends, hayloft,
                          white trim, cupola; stalls and hay inside (enterable).
  grain_bin.glb           a corrugated steel grain bin with a cone roof and a ladder.
  windmill.glb            a steel water-pumping windmill (Aermotor style) over a stock tank.
  crop_corn.glb           a 12 x 12 m tile of corn rows, 2.2 m high (walk through it).
  crop_wheat.glb          a 12 x 12 m tile of ripe wheat, 0.9 m high (walk through it).
  hay_bale.glb            a round bale, 1.5 m.
  water_trough.glb        a galvanised stock tank.
  loading_chute.glb       a cattle loading chute: a ramp up to a trailer deck, by the road.
  feed_bunk.glb           a wooden feed bunk.
  fence_8m.glb            an 8 m run of post-and-rail fence (pastures, corrals).

    python3 tools/generate_ranch_models.py            # write the models
    python3 tools/generate_ranch_models.py --check    # fail if the committed models are stale
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_drop_models as drop  # noqa: E402  (Mesh with quads and blocks)
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
VEHICLES = ROOT / "assets/verda/vehicles/runtime"
RANCH = ROOT / "assets/verda/ranch"
CATALOG = ROOT / "include/outland/creator/RanchAssetCatalog.hpp"

# Kept in step with include/outland/game/freight/Freight.hpp.
TRAILER_LENGTH = 16.0
TRAILER_KINGPIN = 1.0      # kingpin from the trailer's nose
TRAILER_AXLES = (-6.2, -7.45)  # rear tandem, from the trailer's centre (z)
FIFTH_WHEEL = -2.35        # tractor: the fifth wheel's centre from the tractor body's centre
HAT_SEAT = .055            # hat: height of the crown's base ring above the model's bottom

# --------------------------------------------------------------------------- colours
BLACK = (24, 24, 26, 255)
DARK = (40, 40, 42, 255)
TYRE = (28, 28, 30, 255)
CHROME = (206, 208, 212, 255)
ALU = (182, 186, 190, 255)
ALU_DARK = (128, 132, 136, 255)
GLASS = (40, 58, 70, 255)
LAMP = (255, 246, 210, 255)
AMBER = (255, 168, 36, 255)
TAIL = (200, 24, 20, 255)
WHITE = (236, 236, 232, 255)
VERDA_GREEN = (0, 140, 64, 255)
SEAT = (46, 44, 40, 255)


class Mesh(drop.Mesh):
    def block(self, corners, colour):
        """Eight corners: bottom four round the base, then the top four in the same order. Either
        way round: corners given clockwise seen from above are flipped so the faces point out."""
        b0, b1, b2, b3, t0, t1, t2, t3 = corners
        if drop.normal(t0, t1, t2)[1] < 0:
            corners = [b0, b3, b2, b1, t0, t3, t2, t1]
        super().block(corners, colour)

    def cylinder(self, axis, centre, radius, length, colour, segments=12, caps=True, cap_colour=None):
        """A cylinder along `axis` ('x', 'y' or 'z') centred on `centre`."""
        cx, cy, cz = centre

        def point(a, t):
            u, v = radius * math.cos(a), radius * math.sin(a)
            if axis == 'x':
                return (cx + t, cy + u, cz + v)
            if axis == 'y':
                return (cx + u, cy + t, cz + v)
            return (cx + u, cy + v, cz + t)

        h = length / 2
        for i in range(segments):
            a0, a1 = 2 * math.pi * i / segments, 2 * math.pi * (i + 1) / segments
            self.quad(point(a0, -h), point(a1, -h), point(a1, h), point(a0, h), colour, both=True)
        if caps:
            for t in (-h, h):
                for i in range(segments):
                    a0, a1 = 2 * math.pi * i / segments, 2 * math.pi * (i + 1) / segments
                    c = (cx + (t if axis == 'x' else 0), cy + (t if axis == 'y' else 0), cz + (t if axis == 'z' else 0))
                    self.tri(c, point(a0, t), point(a1, t), cap_colour or colour)

    def tri(self, a, b, c, colour):
        n = drop.normal(a, b, c)
        base = len(self.positions)
        self.positions.extend([a, b, c, a, c, b])
        back = (-n[0], -n[1], -n[2])
        self.normals.extend([n, n, n, back, back, back])
        self.colours.extend([colour] * 6)
        self.indices.extend([base, base + 1, base + 2, base + 3, base + 4, base + 5])

    def tube(self, a, b, radius, colour, segments=6):
        """A thin tube from point a to point b (roll cages, rails, ladders)."""
        d = [b[i] - a[i] for i in range(3)]
        length = math.sqrt(sum(x * x for x in d))
        if length < 1e-6:
            return
        d = [x / length for x in d]
        ref = (0, 1, 0) if abs(d[1]) < .9 else (1, 0, 0)
        u = cross(d, ref)
        u = [x / math.sqrt(sum(y * y for y in u)) for x in u]
        v = cross(d, u)
        ring = []
        for i in range(segments):
            ang = 2 * math.pi * i / segments
            ring.append([radius * (math.cos(ang) * u[k] + math.sin(ang) * v[k]) for k in range(3)])
        for i in range(segments):
            j = (i + 1) % segments
            p0 = tuple(a[k] + ring[i][k] for k in range(3))
            p1 = tuple(a[k] + ring[j][k] for k in range(3))
            p2 = tuple(b[k] + ring[j][k] for k in range(3))
            p3 = tuple(b[k] + ring[i][k] for k in range(3))
            self.quad(p0, p1, p2, p3, colour, both=True)

    def wheel(self, x, y, z, radius, width, colour=TYRE, hub=CHROME):
        self.cylinder('x', (x, y, z), radius, width, colour, segments=14, cap_colour=DARK)
        side = 1 if x > 0 else -1
        self.cylinder('x', (x + side * (width / 2 + .005), y, z), radius * .55, .02, hub, segments=10)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def mirror_x(fn):
    for side in (-1, 1):
        fn(side)


# --------------------------------------------------------------------------- side-by-side
SXS_RED = (168, 30, 28, 255)
SXS_RED_DARK = (120, 20, 20, 255)
CAGE = (30, 32, 30, 255)


def side_by_side():
    m = Mesh()
    W = .78
    # Frame, skid plate and floor.
    m.box(-.55, .55, .28, .42, -1.35, 1.25, DARK)
    m.box(-W + .05, W - .05, .42, .48, -.55, .75, DARK)                        # floor pan
    # Front: short sloped hood with headlights, fenders over the front wheels, brush guard.
    m.block([(-W, .62, .9), (W, .62, .9), (W, .62, 1.45), (-W, .62, 1.45),
             (-W + .05, 1.0, .75), (W - .05, 1.0, .75), (W - .1, .88, 1.38), (-W + .1, .88, 1.38)], SXS_RED)
    m.box(-W + .02, W - .02, .48, .62, .78, 1.42, SXS_RED_DARK)
    for side in (-1, 1):
        m.box(side * .45, side * .66, .74, .84, 1.38, 1.46, LAMP)               # headlights
        m.box(side * (W - .02), side * (W + .06), .62, .78, .6, 1.42, SXS_RED)  # front fender flare
        m.box(side * (W - .02), side * (W + .06), .62, .78, -1.35, -.55, SXS_RED)  # rear fender flare
    m.box(-.6, .6, .5, .56, 1.5, 1.56, BLACK)                                   # bumper bar
    for x in (-.45, .45):
        m.tube((x, .5, 1.53), (x, .95, 1.5), .03, BLACK)                         # brush guard uprights
    m.tube((-.45, .95, 1.5), (.45, .95, 1.5), .03, BLACK)
    # Dash, steering wheel, two bucket seats.
    m.box(-W + .08, W - .08, .8, 1.02, .62, .78, DARK)
    m.cylinder('z', (.38, 1.12, .5), .17, .04, BLACK, segments=12, caps=False)
    m.tube((.38, 1.12, .52), (.38, .96, .7), .02, BLACK)
    for x in (-.38, .38):
        m.box(x - .24, x + .24, .48, .78, -.45, .1, SEAT)                       # seat base
        m.box(x - .24, x + .24, .78, 1.38, -.52, -.38, SEAT)                    # back
        m.box(x - .24, x + .24, 1.3, 1.45, -.5, -.42, SEAT)                     # headrest
    m.box(-.1, .1, .48, .9, -.4, .55, DARK)                                     # console
    # Half doors.
    for side in (-1, 1):
        m.box(side * (W - .02), side * (W + .02), .48, .95, -.5, .55, SXS_RED)
    # Roll cage with a roof.
    for side in (-1, 1):
        x = side * (W - .04)
        m.tube((x, .48, .72), (x, 1.62, .25), .035, CAGE)                       # A pillar
        m.tube((x, .48, -.62), (x, 1.82, -.55), .035, CAGE)                     # B pillar
        m.tube((x, 1.62, .25), (x, 1.82, -.55), .035, CAGE)                     # roof rail
        m.tube((x, 1.0, -.6), (x, 1.0, .65), .03, CAGE)                         # side bar
    m.tube((-W + .04, 1.82, -.55), (W - .04, 1.82, -.55), .035, CAGE)
    m.tube((-W + .04, 1.62, .25), (W - .04, 1.62, .25), .035, CAGE)
    m.block([(-W, 1.84, -.62), (W, 1.84, -.62), (W, 1.64, .3), (-W, 1.64, .3),
             (-W, 1.88, -.62), (W, 1.88, -.62), (W, 1.68, .3), (-W, 1.68, .3)], (36, 38, 36, 255))  # roof
    m.box(-.45, .45, 1.6, 1.68, .27, .33, AMBER)                                 # light bar
    # Cargo box behind the cab.
    m.box(-W, W, .74, .8, -1.5, -.62, DARK)                                     # box floor
    for side in (-1, 1):
        m.box(side * (W - .04), side * W, .8, 1.12, -1.5, -.62, SXS_RED)
    m.box(-W, W, .8, 1.12, -1.5, -1.46, SXS_RED)                                # tailgate
    m.box(-W, W, .8, 1.12, -.66, -.62, SXS_RED)
    for side in (-1, 1):
        m.box(side * .55, side * .7, .78, .88, -1.52, -1.5, TAIL)
    m.box(-.62, .62, .4, .62, -1.56, -1.48, BLACK)                              # hitch receiver
    return m


# --------------------------------------------------------------------------- semi tractor
SEMI_PAINT = (24, 64, 112, 255)       # deep blue
SEMI_PAINT_DARK = (16, 44, 80, 255)
SEMI_STRIPE = (210, 170, 60, 255)


def semi_tractor():
    m = Mesh()
    W = 1.25
    # Ladder frame from bumper to tail, fifth wheel.
    for x in (-.5, .5):
        m.box(x - .08, x + .08, .62, .92, -3.55, 3.5, DARK)
    m.box(-.5, .5, .7, .86, -3.5, 3.4, DARK)
    m.box(-.8, .8, 1.08, 1.18, FIFTH_WHEEL - .75, FIFTH_WHEEL + .75, DARK)       # fifth wheel plate
    m.box(-.62, .62, .92, 1.08, FIFTH_WHEEL - .6, FIFTH_WHEEL + .6, BLACK)
    # Long square hood and grille.
    m.box(-.98, .98, 1.15, 2.12, 1.25, 3.42, SEMI_PAINT)                         # hood
    m.box(-.9, .9, 2.12, 2.18, 1.3, 3.38, SEMI_PAINT_DARK)
    m.box(-.82, .82, 1.0, 2.28, 3.42, 3.56, CHROME)                              # grille surround
    for i in range(9):
        y = 1.12 + i * .13
        m.box(-.74, .74, y, y + .05, 3.56, 3.58, (150, 152, 156, 255))           # grille bars
    m.box(-.1, .1, 2.28, 2.42, 3.4, 3.5, CHROME)                                 # hood ornament
    m.box(-W - .05, W + .05, .78, 1.08, 3.55, 3.85, CHROME)                      # bumper
    for side in (-1, 1):
        # Fenders over the steer axle, headlights, mirrors, air cleaners, step and fuel tank.
        m.block([(side * .98, .95, 1.6), (side * (W + .05), .95, 1.6), (side * (W + .05), .95, 3.4), (side * .98, .95, 3.4),
                 (side * .98, 1.6, 1.85), (side * (W + .05), 1.45, 1.85), (side * (W + .05), 1.45, 3.15), (side * .98, 1.6, 3.15)], SEMI_PAINT)
        m.box(side * .98, side * 1.2, 1.4, 1.62, 3.35, 3.48, LAMP)
        m.box(side * 1.0, side * 1.22, 1.62, 1.7, 3.3, 3.45, AMBER)
        m.cylinder('z', (side * 1.12, 2.3, 1.05), .24, .7, CHROME, segments=12)  # air cleaner
        m.box(side * (W + .02), side * (W + .18), 2.7, 3.4, .55, .65, BLACK)      # mirror
        m.tube((side * W, 2.8, .6), (side * (W + .1), 2.8, .6), .02, CHROME)
        m.cylinder('z', (side * .95, .95, -.15), .33, 1.25, CHROME, segments=14)  # fuel tank
        m.box(side * .9, side * 1.28, .55, .62, .55, .9, (90, 92, 94, 255))       # step
        # Twin chrome stacks behind the sleeper.
        m.cylinder('y', (side * 1.1, 2.75, -1.55), .1, 3.1, CHROME, segments=10)
        m.cylinder('y', (side * 1.1, 4.32, -1.55), .11, .06, BLACK, segments=10)
        # Rear mud flaps and lights.
        m.box(side * .9, side * 1.3, .3, .95, -3.5, -3.45, BLACK)
        m.box(side * .7, side * .95, .92, 1.02, -3.56, -3.5, TAIL)
    # Cab and sleeper.
    m.box(-W, W, 1.15, 2.95, .25, 1.25, SEMI_PAINT)                              # cab lower / cowl
    m.box(-W, W, 2.95, 3.35, .55, 1.25, GLASS)                                   # windscreen (flat, upright)
    m.box(-W + .05, W - .05, 2.0, 2.15, 1.0, 1.26, SEMI_PAINT_DARK)              # dash line
    m.box(-W, W, 1.15, 3.55, -.15, .55, SEMI_PAINT)                              # cab back / doors
    for side in (-1, 1):
        m.box(side * (W - .01), side * (W + .01), 2.35, 3.25, -.05, .5, GLASS)   # side windows
    m.box(-W, W, 3.35, 3.55, .55, 1.3, SEMI_PAINT)                               # visor / roof front
    m.box(-W, W, 3.55, 3.62, -.15, 1.3, SEMI_PAINT_DARK)                         # roof
    m.box(-W, W, 1.15, 3.7, -1.85, -.15, SEMI_PAINT)                             # sleeper
    m.box(-W + .02, W - .02, 3.7, 3.78, -1.85, -.2, SEMI_PAINT_DARK)
    for side in (-1, 1):
        m.box(side * (W - .01), side * (W + .02), 1.75, 1.85, -1.8, 1.25, SEMI_STRIPE)  # stripe
        m.box(side * (W - .01), side * (W + .01), 2.6, 3.0, -1.4, -.95, GLASS)  # sleeper window
    for x in (-.6, -.2, .2, .6):
        m.box(x - .07, x + .07, 3.62, 3.7, 1.15, 1.25, AMBER)                    # cab marker lights
    # The tandem's second drive axle: static duals (the first drive axle and the steer axle are
    # the game's turning wheels).
    for side in (-1, 1):
        for k in (0, 1):
            m.wheel(side * (.78 + k * .28), .52, -3.05, .52, .26)
    return m


# --------------------------------------------------------------------------- trailers
def trailer_common(m, L, deck_y):
    """Landing gear, rear tandem with duals, bumper, lights; origin at the trailer's centre."""
    half = L / 2
    z_gear = half - 3.2
    for side in (-1, 1):
        m.box(side * .85, side * .95, .25, deck_y, z_gear - .08, z_gear + .08, DARK)    # landing legs
        m.box(side * .78, side * 1.02, .2, .26, z_gear - .2, z_gear + .2, DARK)         # pads
        for z in TRAILER_AXLES:
            for k in (0, 1):
                m.wheel(side * (.78 + k * .28), .52, z, .52, .26)
        m.box(side * .2, side * 1.25, 1.05, 1.18, -half + .9, -half + 1.4, BLACK)       # mud flap hanger
        m.box(side * .95, side * 1.25, .35, 1.05, -half + 1.35, -half + 1.4, BLACK)     # mud flaps
        m.box(side * .75, side * 1.05, 1.15, 1.3, -half - .02, -half + .02, TAIL)
    m.box(-1.0, 1.0, .5, .62, -half + .2, -half + .35, (210, 40, 40, 255))              # ICC bumper
    for side in (-1, 1):
        m.box(side * .55, side * .65, .62, 1.2, -half + .25, -half + .32, DARK)
    m.box(-.6, .6, deck_y - .3, deck_y, TRAILER_AXLES[0] - 1.0, TRAILER_AXLES[1] + 1.0, DARK)  # slider


def trailer_box():
    m = Mesh()
    L, W = TRAILER_LENGTH, 1.3
    half = L / 2
    floor = 1.3
    m.box(-W, W, floor, 4.1, -half, half, WHITE)                                         # the box
    m.box(-W - .01, W + .01, floor - .05, floor + .12, -half, half, ALU)                  # bottom rail
    m.box(-W - .01, W + .01, 4.02, 4.12, -half, half, ALU)                               # top rail
    for side in (-1, 1):
        m.box(side * (W + .005), side * (W + .02), 2.35, 2.95, -half + 1.5, half - 1.0, VERDA_GREEN)  # stripe
        for z in [half - 1.0 - i * 1.2 for i in range(13)]:
            m.box(side * (W + .005), side * (W + .02), floor + .12, 4.02, z - .04, z, (214, 214, 210, 255))  # side posts
        for z in (half - .3, 0, -half + .3):
            m.box(side * (W + .005), side * (W + .025), 3.95, 4.02, z - .06, z + .06, AMBER)  # markers
    m.box(-W, W, floor, 4.1, -half - .03, -half, ALU)                                    # rear doors
    m.box(-.01, .01, floor, 4.1, -half - .05, -half - .03, DARK)
    for x in (-.6, -.35, .35, .6):
        m.box(x - .02, x + .02, floor + .2, 3.9, -half - .07, -half - .04, ALU_DARK)     # lock rods
    m.box(-.45, .45, .9, floor, half - 1.6, half - .4, DARK)                             # gooseneck / kingpin plate
    m.box(-.6, .6, 3.6, 4.0, half, half + .25, (60, 62, 64, 255))                        # reefer-less nose cap
    trailer_common(m, L, floor)
    return m


def trailer_cattle():
    m = Mesh()
    L, W = TRAILER_LENGTH, 1.28
    half = L / 2
    neck_end = half - 3.4          # the upper deck over the gooseneck
    belly_end = TRAILER_AXLES[0] + 1.5
    # Gooseneck (high front deck), drop belly (low), rear deck over the axles.
    sections = [(neck_end, half, 1.35), (belly_end, neck_end, .62), (-half, belly_end, 1.35)]
    for z0, z1, floor in sections:
        m.box(-W, W, floor, floor + .1, z0, z1, ALU_DARK)
        m.box(-W, W, 3.95, 4.05, z0, z1, ALU)                                            # roof
        for side in (-1, 1):
            m.box(side * (W - .03), side * W, floor + .1, 3.95, z0, z1, ALU)             # side skin
            # Rows of air slots.
            y = floor + .55
            while y < 3.75:
                z = z0 + .25
                while z < z1 - .55:
                    m.box(side * (W - .001), side * (W + .01), y, y + .12, z, z + .45, (26, 28, 30, 255))
                    z += .65
                y += .5
    for side in (-1, 1):
        m.box(side * (W - .01), side * (W + .02), 3.86, 3.95, -half, half, ALU_DARK)     # roof rail
        m.box(side * .2, side * W, .62, 1.35, belly_end - .05, belly_end + .05, ALU)     # belly step walls
        m.box(side * .2, side * W, .62, 1.35, neck_end - .05, neck_end + .05, ALU)
    m.box(-W, W, .62, 1.35, belly_end - .05, belly_end, ALU)
    m.box(-W, W, .62, 1.35, neck_end, neck_end + .05, ALU)
    m.box(-W, W, 1.35, 3.95, -half - .04, -half, ALU_DARK)                               # rear swing gate
    m.box(-.03, .03, 1.4, 3.9, -half - .07, -half - .04, BLACK)
    m.box(-W, W, 1.35, 4.05, half, half + .04, ALU)                                      # nose
    m.box(-.45, .45, .9, 1.35, half - 1.6, half - .4, DARK)
    trailer_common(m, L, 1.35)
    return m


# --------------------------------------------------------------------------- cattle
def cow(coat, face, accent):
    m = Mesh()
    HORN = (226, 214, 180, 255)
    HOOF = (40, 34, 30, 255)
    # Barrel (body), deeper in the middle, rump and chest.
    m.block([(-.33, .75, -.85), (.33, .75, -.85), (.33, .75, .62), (-.33, .75, .62),
             (-.36, 1.42, -.9), (.36, 1.42, -.9), (.36, 1.42, .66), (-.36, 1.42, .66)], coat)
    m.box(-.3, .3, .68, .76, -.6, .4, accent)                                            # belly line
    m.block([(-.3, .82, .62), (.3, .82, .62), (.26, .9, .86), (-.26, .9, .86),
             (-.33, 1.4, .62), (.33, 1.4, .62), (.26, 1.36, .86), (-.26, 1.36, .86)], coat)  # chest / shoulders
    m.box(-.18, .18, .62, .82, .55, .86, accent)                                         # brisket
    # Neck and head (white-faced on a Hereford), muzzle, ears, horns.
    m.block([(-.17, 1.02, .82), (.17, 1.02, .82), (.15, 1.0, 1.08), (-.15, 1.0, 1.08),
             (-.2, 1.38, .8), (.2, 1.38, .8), (.16, 1.32, 1.06), (-.16, 1.32, 1.06)], coat)
    m.block([(-.15, .82, 1.06), (.15, .82, 1.06), (.11, .8, 1.36), (-.11, .8, 1.36),
             (-.16, 1.3, 1.04), (.16, 1.3, 1.04), (.12, 1.12, 1.34), (-.12, 1.12, 1.34)], face)
    m.box(-.11, .11, .8, .94, 1.34, 1.44, (60, 46, 44, 255))                              # muzzle
    for side in (-1, 1):
        m.box(side * .15, side * .3, 1.18, 1.26, 1.02, 1.12, coat)                       # ears
        m.box(side * .1, side * .14, 1.12, 1.16, 1.3, 1.33, BLACK)                       # eyes
        m.tube((side * .13, 1.3, 1.06), (side * .26, 1.36, 1.1), .025, HORN, segments=5)
        m.tube((side * .26, 1.36, 1.1), (side * .3, 1.44, 1.16), .02, HORN, segments=5)
    # Legs and hooves (the front two a little forward), tail with a switch.
    for x in (-.2, .2):
        for z in (-.7, .5):
            m.box(x - .07, x + .07, .08, .78, z - .07, z + .07, coat if z < 0 else accent)
            m.box(x - .08, x + .08, 0, .1, z - .08, z + .09, HOOF)
    m.tube((0, 1.36, -.9), (0, .7, -1.0), .03, coat, segments=5)
    m.box(-.05, .05, .5, .72, -1.04, -.96, accent)
    return m


# --------------------------------------------------------------------------- cowboy hat
FELT = (120, 86, 52, 255)
FELT_DARK = (84, 58, 34, 255)
BAND = (40, 28, 20, 255)


def cowboy_hat():
    """Brim 0.36 m across and 0.32 m front to back, crown 0.12 m high, pinched at the front."""
    m = Mesh()
    seg = 20
    # The brim: an oval ring, flat at front and back and rolled up at the sides.
    rx, rz, inner = .18, .16, .1
    def brim_y(a):
        return HAT_SEAT + .035 * abs(math.cos(a)) ** 2      # sides (x) curl up
    outer = [(rx * math.cos(2 * math.pi * i / seg), brim_y(2 * math.pi * i / seg), rz * math.sin(2 * math.pi * i / seg)) for i in range(seg)]
    hole = [(inner * math.cos(2 * math.pi * i / seg), HAT_SEAT, inner * 1.12 * math.sin(2 * math.pi * i / seg)) for i in range(seg)]
    for i in range(seg):
        j = (i + 1) % seg
        m.quad(hole[i], hole[j], outer[j], outer[i], FELT, both=True)
    # Brim edge binding (thin), so it reads from the side.
    for i in range(seg):
        j = (i + 1) % seg
        a, b = outer[i], outer[j]
        m.quad(a, b, (b[0], b[1] - .012, b[2]), (a[0], a[1] - .012, a[2]), FELT_DARK, both=True)
    # The crown: an oval tube tapering up, with a front pinch and a centre crease.
    levels = [(0, 1.0), (.03, .99), (.085, .96), (.12, .9)]
    rings = []
    for h, k in levels:
        ring = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            x, z = inner * k * math.cos(a), inner * 1.12 * k * math.sin(a)
            if h > .06 and z > 0:
                x *= 1 - .22 * (z / (inner * 1.12)) * ((h - .06) / .06)   # the pinch at the front
            ring.append((x, HAT_SEAT + h, z))
        rings.append(ring)
    for r in range(len(rings) - 1):
        colour = BAND if r == 0 else FELT
        for i in range(seg):
            j = (i + 1) % seg
            m.quad(rings[r][i], rings[r][j], rings[r + 1][j], rings[r + 1][i], colour, both=True)
    top = rings[-1]
    crease = (0, HAT_SEAT + .095, 0)
    for i in range(seg):
        j = (i + 1) % seg
        m.tri(top[i], top[j], crease, FELT_DARK)
    return m


# --------------------------------------------------------------------------- farm buildings
BARN_RED = (150, 36, 30, 255)
BARN_RED_DARK = (110, 26, 22, 255)
TRIM = (236, 232, 222, 255)
ROOF = (70, 72, 76, 255)
WOOD = (120, 90, 60, 255)
HAY = (214, 186, 96, 255)
HAY_DARK = (180, 150, 70, 255)


def barn():
    """Gambrel barn: 14 m wide (x), 22 m long (z), doors at both ends (z), eaves 5 m, ridge 10 m."""
    m = Mesh()
    W, L = 7.0, 11.0
    eave, knee, ridge = 5.0, 8.0, 10.2
    t = .2
    door = 2.4                     # half width of the end doors
    # Floor slab.
    m.box(-W, W, 0, .1, -L, L, (120, 116, 108, 255))
    # Long side walls with a row of windows.
    for side in (-1, 1):
        x0, x1 = (W - t, W) if side > 0 else (-W, -W + t)
        z = -L
        for zz in [-L + 2 + i * 4.5 for i in range(5)]:
            m.box(x0, x1, .1, eave, z, zz, BARN_RED)
            m.box(x0, x1, .1, 1.4, zz, zz + 1.2, BARN_RED)
            m.box(x0, x1, 2.4, eave, zz, zz + 1.2, BARN_RED)
            m.box(x0 - .02 * side, x1 + .02 * side, 1.4, 2.4, zz - .08, zz, TRIM)
            m.box(x0 - .02 * side, x1 + .02 * side, 1.4, 2.4, zz + 1.2, zz + 1.28, TRIM)
            z = zz + 1.2
        m.box(x0, x1, .1, eave, z, L, BARN_RED)
        m.box(x0 - .03 * side, x1 + .03 * side, eave - .25, eave, -L, L, TRIM)
    # Gambrel ends with big door openings (doors slid open beside them) and a hayloft door.
    profile = [(-W, eave), (-W * .62, knee), (0, ridge), (W * .62, knee), (W, eave)]
    for end in (-1, 1):
        z0, z1 = (L - t, L) if end > 0 else (-L, -L + t)
        m.box(-W, -door, .1, eave, z0, z1, BARN_RED)
        m.box(door, W, .1, eave, z0, z1, BARN_RED)
        m.box(-door, door, 4.0, eave, z0, z1, BARN_RED)
        # The gable above the eaves, as quads between the profile and the eave line.
        for (xa, ya), (xb, yb) in zip(profile, profile[1:]):
            m.quad((xa, eave, z1 if end > 0 else z0), (xb, eave, z1 if end > 0 else z0), (xb, yb, z1 if end > 0 else z0), (xa, ya, z1 if end > 0 else z0), BARN_RED, both=True)
        zf = z1 + .02 if end > 0 else z0 - .02
        m.box(-1.0, 1.0, 6.2, 7.9, min(zf, zf + .01 * end), max(zf, zf + .01 * end), BARN_RED_DARK)  # hayloft door
        m.box(-1.08, 1.08, 6.12, 6.2, min(zf, zf + .02 * end), max(zf, zf + .02 * end), TRIM)
        m.box(-1.08, 1.08, 7.9, 7.98, min(zf, zf + .02 * end), max(zf, zf + .02 * end), TRIM)
        # The two sliding doors pushed open, with white X braces.
        for side in (-1, 1):
            x0 = side * door
            x1 = side * (door + 2.4)
            zo = zf + .1 * end
            lo, hi = min(zo, zo + .08 * end), max(zo, zo + .08 * end)
            m.box(min(x0, x1), max(x0, x1), .1, 4.0, lo, hi, BARN_RED_DARK)
            m.tube((x0, .2, zo + .1 * end), (x1, 3.9, zo + .1 * end), .05, TRIM, segments=4)
            m.tube((x1, .2, zo + .1 * end), (x0, 3.9, zo + .1 * end), .05, TRIM, segments=4)
            m.box(min(x0, x1), max(x0, x1), 3.92, 4.0, lo - .02 * end, hi + .02 * end, TRIM)
        m.box(-door - .1, door + .1, 4.0, 4.15, min(zf, zf + .05 * end), max(zf, zf + .05 * end), TRIM)  # door header
        m.box(-door - 2.6, door + 2.6, 4.15, 4.3, min(zf, zf + .25 * end), max(zf, zf + .25 * end), DARK)  # door track
    # Roof: four slopes.
    over = .4
    for (xa, ya), (xb, yb) in zip(profile, profile[1:]):
        a0 = (xa, ya, -L - over)
        a1 = (xa, ya, L + over)
        b1 = (xb, yb, L + over)
        b0 = (xb, yb, -L - over)
        m.quad(a0, a1, b1, b0, ROOF, both=True)
    # Hayloft floor and some hay, stalls along one side.
    m.box(-W + t, W - t, 4.6, 4.75, -L + t, -2, WOOD)
    for i in range(6):
        z = -L + 1.5 + i * 1.6
        m.box(-W + .4, -W + 3.6, 4.75, 5.55, z, z + 1.1, HAY)
        m.box(W - 3.6, W - .4, 4.75, 5.55, z + .3, z + 1.4, HAY_DARK)
    for i in range(5):
        z = 1 + i * 2
        m.box(W - 3.2, W - 3.1, .1, 1.4, z, z + 1.9, WOOD)                              # stall partitions
        m.box(W - 3.2, W - t, 1.3, 1.4, z, z + .1, WOOD)
    for x in (-W + .1, W - .1):
        for z in (-L + 4, 0, L - 4):
            m.box(x - .15, x + .15, .1, eave, z - .15, z + .15, WOOD)                   # posts
    for z in (-L + 4, 0, L - 4):
        m.box(-.18, .18, .1, 4.6, z - .18, z + .18, WOOD)
    # Cupola on the ridge.
    m.box(-.9, .9, ridge - .3, ridge + 1.2, -.9, .9, TRIM)
    m.box(-.82, .82, ridge + .05, ridge + 1.0, -.92, .92, (60, 50, 44, 255))             # louvres
    m.block([(-1.15, ridge + 1.2, -1.15), (1.15, ridge + 1.2, -1.15), (1.15, ridge + 1.2, 1.15), (-1.15, ridge + 1.2, 1.15),
             (-.05, ridge + 2.0, -.05), (.05, ridge + 2.0, -.05), (.05, ridge + 2.0, .05), (-.05, ridge + 2.0, .05)], ROOF)
    m.tube((0, ridge + 2.0, 0), (0, ridge + 2.7, 0), .03, BLACK)
    m.box(-.35, .35, ridge + 2.45, ridge + 2.5, -.02, .02, BLACK)                         # weathervane
    return m


def grain_bin():
    m = Mesh()
    r, h = 3.0, 6.5
    seg = 24
    for k in range(int(h / .5)):
        shade = 196 if k % 2 == 0 else 176
        m.cylinder('y', (0, .25 + k * .5, 0), r, .5, (shade, shade + 2, shade + 6, 255), segments=seg, caps=False)
    m.cylinder('y', (0, .05, 0), r + .15, .1, (130, 126, 120, 255), segments=seg)        # pad
    # Cone roof.
    top = (0, h + 2.2, 0)
    for i in range(seg):
        a0, a1 = 2 * math.pi * i / seg, 2 * math.pi * (i + 1) / seg
        p0 = (r * 1.04 * math.cos(a0), h, r * 1.04 * math.sin(a0))
        p1 = (r * 1.04 * math.cos(a1), h, r * 1.04 * math.sin(a1))
        m.tri(p0, p1, top, (186, 188, 192, 255))
    m.cylinder('y', (0, h + 2.35, 0), .45, .3, (160, 162, 166, 255), segments=10)       # vent cap
    # Ladder up the side and a door.
    for x in (-.25, .25):
        m.tube((x, .3, r + .12), (x, h + .2, r + .12), .025, ALU_DARK)
    for y in [.6 + i * .4 for i in range(int((h - .5) / .4))]:
        m.tube((-.25, y, r + .12), (.25, y, r + .12), .02, ALU_DARK)
    m.box(-.6, .6, .3, 1.5, r - .05, r + .05, (150, 152, 156, 255))
    m.box(-.15, .15, .2, h, -r - .1, -r + .05, (150, 152, 156, 255))                     # auger chute
    return m


def windmill():
    m = Mesh()
    STEEL = (150, 152, 150, 255)
    H = 9.0
    # Four-legged lattice tower tapering to the top.
    corners = [(-1.4, -1.4), (1.4, -1.4), (1.4, 1.4), (-1.4, 1.4)]
    tops = [(-.3, -.3), (.3, -.3), (.3, .3), (-.3, .3)]
    for (bx, bz), (tx, tz) in zip(corners, tops):
        m.tube((bx, 0, bz), (tx, H, tz), .05, STEEL)
    for k in range(1, 6):
        f = k / 6
        y = H * f
        ring = [(bx + (tx - bx) * f, y, bz + (tz - bz) * f) for (bx, bz), (tx, tz) in zip(corners, tops)]
        for i in range(4):
            m.tube(ring[i], ring[(i + 1) % 4], .03, STEEL)
    m.tube((0, 0, 0), (0, H + .4, 0), .04, STEEL)                                         # pump rod
    m.box(-.35, .35, H, H + .5, -.5, .5, (120, 120, 118, 255))                            # gearbox
    # The wheel: 18 sails on a ring, facing +Z, and the tail vane.
    hub = (0, H + .3, .7)
    m.cylinder('z', hub, .18, .3, DARK, segments=8)
    for i in range(18):
        a = 2 * math.pi * i / 18
        tip = (hub[0] + 1.5 * math.cos(a), hub[1] + 1.5 * math.sin(a), hub[2])
        mid = (hub[0] + .5 * math.cos(a), hub[1] + .5 * math.sin(a), hub[2])
        side = (-math.sin(a) * .16, math.cos(a) * .16)
        m.quad((mid[0] - side[0], mid[1] - side[1], mid[2]), (mid[0] + side[0], mid[1] + side[1], mid[2] + .08),
               (tip[0] + side[0] * 1.6, tip[1] + side[1] * 1.6, tip[2] + .12), (tip[0] - side[0] * 1.6, tip[1] - side[1] * 1.6, tip[2]),
               (196, 198, 196, 255), both=True)
    for i in range(18):
        a0, a1 = 2 * math.pi * i / 18, 2 * math.pi * (i + 1) / 18
        m.tube((hub[0] + 1.5 * math.cos(a0), hub[1] + 1.5 * math.sin(a0), hub[2]),
               (hub[0] + 1.5 * math.cos(a1), hub[1] + 1.5 * math.sin(a1), hub[2]), .02, STEEL, segments=4)
    m.tube((0, H + .3, 0), (0, H + .3, -2.4), .04, STEEL)
    m.quad((0, H - .3, -1.6), (0, H + 1.0, -1.6), (0, H + .9, -2.8), (0, H - .2, -2.8), (190, 50, 40, 255), both=True)  # tail vane
    # Stock tank beside it.
    m.cylinder('y', (2.6, .35, 0), 1.3, .7, ALU, segments=18, cap_colour=(70, 100, 120, 255))
    m.tube((0, .2, 0), (1.6, .75, 0), .04, DARK)
    return m


def crop_corn():
    """A 12 x 12 m tile: eight rows along z of 2.2 m corn (crossed leafy strips with tassels)."""
    m = Mesh()
    S = 6.0
    STALK = (88, 128, 46, 255)
    LEAF = (70, 116, 40, 255)
    TASSEL = (200, 172, 92, 255)
    rows = 8
    for r in range(rows):
        x = -S + (r + .5) * (2 * S / rows)
        z = -S
        k = 0
        while z < S - .01:
            z1 = min(S, z + .75)
            h0 = 2.0 + .25 * math.sin(r * 1.7 + k * 2.3)
            h1 = 2.0 + .25 * math.sin(r * 1.7 + (k + 1) * 2.3)
            # A row is two crossed jagged strips (leaves) - cheap, reads as a wall of corn.
            m.quad((x - .28, .0, z), (x - .28, .0, z1), (x + .28, h1 * .55, z1), (x + .28, h0 * .55, z), LEAF, both=True)
            m.quad((x + .28, .0, z), (x + .28, .0, z1), (x - .28, h1 * .55, z1), (x - .28, h0 * .55, z), STALK, both=True)
            m.quad((x - .3, h0 * .5, z), (x - .3, h1 * .5, z1), (x + .3, h1 * .92, z1), (x + .3, h0 * .92, z), LEAF, both=True)
            m.quad((x + .3, h0 * .5, z), (x + .3, h1 * .5, z1), (x - .3, h1 * .92, z1), (x - .3, h0 * .92, z), STALK, both=True)
            m.quad((x, h0 * .9, z), (x, h1 * .9, z1), (x, h1 + .05, z1), (x, h0 + .05, z), TASSEL, both=True)
            z = z1
            k += 1
    m.box(-S, S, -.02, .0, -S, S, (92, 70, 48, 255))                                      # tilled soil
    return m


def crop_wheat():
    m = Mesh()
    S = 6.0
    GOLD = (214, 180, 86, 255)
    GOLD_DARK = (184, 150, 64, 255)
    m.box(-S, S, -.02, .0, -S, S, (120, 96, 60, 255))
    # A golden mat with a rippled top, as strips along z.
    strips = 16
    for i in range(strips):
        x0 = -S + i * (2 * S / strips)
        x1 = x0 + 2 * S / strips
        z = -S
        k = 0
        while z < S - .01:
            z1 = min(S, z + 1.0)
            h0 = .82 + .1 * math.sin(i * 1.3 + k * 1.9)
            h1 = .82 + .1 * math.sin(i * 1.3 + (k + 1) * 1.9)
            m.quad((x0, h0, z), (x0, h1, z1), (x1, h1 + .03, z1), (x1, h0 + .03, z), GOLD if (i + k) % 2 else GOLD_DARK, both=True)
            z = z1
            k += 1
        m.quad((x0, 0, -S), (x1, 0, -S), (x1, .85, -S), (x0, .85, -S), GOLD_DARK, both=True)
        m.quad((x0, 0, S), (x1, 0, S), (x1, .85, S), (x0, .85, S), GOLD_DARK, both=True)
    m.quad((-S, 0, -S), (-S, 0, S), (-S, .85, S), (-S, .85, -S), GOLD_DARK, both=True)
    m.quad((S, 0, -S), (S, 0, S), (S, .85, S), (S, .85, -S), GOLD_DARK, both=True)
    return m


def hay_bale():
    m = Mesh()
    m.cylinder('x', (0, .75, 0), .75, 1.4, HAY, segments=16, cap_colour=HAY_DARK)
    for x in (-.4, 0, .4):
        m.cylinder('x', (x, .75, 0), .76, .03, (150, 120, 60, 255), segments=16, caps=False)  # twine
    return m


def water_trough():
    m = Mesh()
    m.cylinder('y', (0, .35, 0), 1.1, .7, ALU, segments=18, caps=False)
    m.cylinder('y', (0, .6, 0), 1.08, .02, (70, 104, 124, 255), segments=18)             # water
    m.cylinder('y', (0, .02, 0), 1.1, .04, ALU_DARK, segments=18)
    return m


def loading_chute():
    """A ramp from the ground (z = -3.2) up to a trailer deck (1.35 m at z = +1.6), boarded sides."""
    m = Mesh()
    WOODG = (126, 104, 78, 255)
    m.block([(-.5, 0, -3.2), (.5, 0, -3.2), (.5, 1.25, 1.2), (-.5, 1.25, 1.2),
             (-.5, .1, -3.2), (.5, .1, -3.2), (.5, 1.35, 1.2), (-.5, 1.35, 1.2)], WOODG)       # ramp deck
    m.box(-.5, .5, 1.25, 1.35, 1.2, 1.7, WOODG)                                           # landing
    for side in (-1, 1):
        for i in range(6):
            z = -3.2 + i * .98
            y0 = (z + 3.2) / 4.4 * 1.25
            m.box(side * .55, side * .65, 0, y0 + 1.6, z - .06, z + .06, WOOD)            # posts
        for k in range(4):
            m.block([(side * .5, .4 + k * .35, -3.2), (side * .55, .4 + k * .35, -3.2), (side * .55, 1.65 + k * .35, 1.7), (side * .5, 1.65 + k * .35, 1.7),
                     (side * .5, .55 + k * .35, -3.2), (side * .55, .55 + k * .35, -3.2), (side * .55, 1.8 + k * .35, 1.7), (side * .5, 1.8 + k * .35, 1.7)], WOODG)
    return m


def ranch_fence():
    """An 8 m run of post-and-rail fence along X (four posts, three rails), centred."""
    m = Mesh()
    POST = (98, 78, 56, 255)
    RAIL = (138, 112, 80, 255)
    for x in (-4.0, -4 / 3, 4 / 3, 4.0):
        m.box(x - .07, x + .07, 0, 1.45, -.07, .07, POST)
    for y in (.45, .85, 1.25):
        m.box(-4.0, 4.0, y, y + .14, -.03, .03, RAIL)
    return m


def feed_bunk():
    m = Mesh()
    m.box(-2.0, 2.0, .45, .55, -.4, .4, WOOD)
    for side in (-1, 1):
        m.box(-2.0, 2.0, .55, .8, side * .4 - .04, side * .4 + .04, WOOD)
    for x in (-1.8, 1.8):
        for z in (-.35, .35):
            m.box(x - .06, x + .06, 0, .45, z - .06, z + .06, WOOD)
    m.box(-1.9, 1.9, .55, .65, -.3, .3, HAY)
    return m


# --------------------------------------------------------------------------- outputs
VEHICLE_MODELS = [
    ("sxs_body.glb", side_by_side),
    ("semi_body.glb", semi_tractor),
    ("trailer_box.glb", trailer_box),
    ("trailer_cattle.glb", trailer_cattle),
]
GEAR_MODELS = [
    ("cow_hereford.glb", lambda: cow((150, 62, 34, 255), (238, 232, 220, 255), (236, 230, 218, 255))),
    ("cow_angus.glb", lambda: cow((30, 28, 28, 255), (34, 32, 32, 255), (40, 36, 36, 255))),
    ("cowboy_hat.glb", cowboy_hat),
]
# Catalogued farm pieces: file, builder, id, name, category, footprint w, d, h.
FARM_PIECES = [
    ("barn.glb", barn, "ranch_barn", "Red gambrel barn (open ends, hayloft, stalls)", "Building", 14.6, 22.8, 12.9),
    ("grain_bin.glb", grain_bin, "ranch_grain_bin", "Grain bin (corrugated steel, cone roof)", "Building", 6.4, 6.6, 8.9),
    ("windmill.glb", windmill, "ranch_windmill", "Water-pumping windmill over a stock tank", "Prop", 5.6, 5.4, 10.9),
    ("crop_corn.glb", crop_corn, "ranch_crop_corn", "Corn field tile 12 m (walk-through)", "Road", 12, 12, 2.3),
    ("crop_wheat.glb", crop_wheat, "ranch_crop_wheat", "Wheat field tile 12 m (walk-through)", "Road", 12, 12, .95),
    ("hay_bale.glb", hay_bale, "ranch_hay_bale", "Round hay bale", "Prop", 1.5, 1.5, 1.5),
    ("water_trough.glb", water_trough, "ranch_water_trough", "Stock tank (water trough)", "Prop", 2.2, 2.2, .7),
    ("loading_chute.glb", loading_chute, "ranch_loading_chute", "Cattle loading chute", "Prop", 1.3, 4.9, 3.2),
    ("feed_bunk.glb", feed_bunk, "ranch_feed_bunk", "Wooden feed bunk", "Prop", 4, .9, .8),
    ("fence_8m.glb", ranch_fence, "ranch_fence_8m", "Post-and-rail ranch fence, 8 m", "Prop", 8.1, .2, 1.45),
]


def catalog_header():
    lines = [
        "#pragma once",
        "// Generated by tools/generate_ranch_models.py - do not edit.",
        "#include \"outland/creator/CreatorAssetRegistry.hpp\"",
        "namespace outland::creator {",
        "inline void register_ranch_assets(CreatorAssetRegistry& registry) {",
    ]
    for path, _, ident, name, category, w, d, h in FARM_PIECES:
        lines.append(f"    (void)registry.add({{.id = \"{ident}\", .name = \"{name}\", .category = CreatorAssetCategory::{category},")
        lines.append(f"        .model_path = \"assets/verda/ranch/{path}\", .thumbnail_path = \"\", .footprint = {{{float(w):.2f}F, {float(d):.2f}F, {float(h):.2f}F}},")
        lines.append("        .placement = {}, .default_scale = 1, .tags = {\"verda\", \"ranch\", \"farm\"}});")
    lines += ["}", "}  // namespace outland::creator", ""]
    return "\n".join(lines).encode()


def outputs():
    result = {}
    for name, build in VEHICLE_MODELS:
        result[VEHICLES / name] = truck.glb(build().data(), Path(name).stem, False)
    for name, build in GEAR_MODELS:
        result[RANCH / name] = truck.glb(build().data(), Path(name).stem, False)
    for name, build, *_ in FARM_PIECES:
        result[RANCH / name] = truck.glb(build().data(), Path(name).stem, False)
    result[CATALOG] = catalog_header()
    return result


if __name__ == "__main__":
    files = outputs()
    stale = [p for p, data in files.items() if not p.exists() or p.read_bytes() != data]
    if "--check" in sys.argv:
        for p in stale:
            print(f"stale: {p.relative_to(ROOT)}", file=sys.stderr)
        sys.exit(1 if stale else 0)
    for p, data in files.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
        print(f"wrote {p.relative_to(ROOT)}: {len(data)} bytes")
