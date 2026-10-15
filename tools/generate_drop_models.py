#!/usr/bin/env python3
"""Generate the Battle Royale drop models (assets/verda/aircraft/):

  drop_helicopter.glb  an olive transport helicopter in Verda's colours, both side doors open,
                       nose toward +Z, skids on y = 0. The main rotor and the tail rotor are drawn
                       by the game (they spin): hubs at ROTOR_HUB and TAIL_HUB below.
  parachute.glb        a ram-air canopy in Verda green and white with its lines, the harness
                       point (where the lines meet) at the origin, the canopy ~6.5 m above it,
                       span along X, nose toward +Z.

Vertex colours, metres.

    python3 tools/generate_drop_models.py            # write the models
    python3 tools/generate_drop_models.py --check    # fail if the committed models are stale
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_sight_models as kit  # noqa: E402  (Mesh: boxes and tubes)
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/verda/aircraft"

# Kept in step with src/game/aircraft/DropRenderer.cpp.
ROTOR_HUB = (0.0, 3.75, 0.2)
TAIL_HUB = (0.32, 3.05, -7.55)

OLIVE = (74, 88, 58, 255)
OLIVE_DARK = (52, 62, 42, 255)
BELLY = (60, 66, 58, 255)
GLASS = (40, 62, 82, 255)
INTERIOR = (30, 32, 30, 255)
SEAT = (60, 48, 36, 255)
METAL = (44, 46, 48, 255)
WHITE = (232, 234, 228, 255)
GREEN = (0, 150, 70, 255)
RED = (210, 30, 30, 255)
CANOPY_GREEN = (0, 158, 72, 255)
CANOPY_WHITE = (238, 240, 232, 255)
CANOPY_UNDER = (0, 112, 54, 255)
LINE = (210, 210, 200, 255)


class Mesh(kit.Mesh):
    def quad(self, a, b, c, d, colour, both=False):
        n = normal(a, b, c)
        base = len(self.positions)
        self.positions.extend([a, b, c, d])
        self.normals.extend([n] * 4)
        self.colours.extend([colour] * 4)
        self.indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])
        if both:
            self.quad(d, c, b, a, colour)

    def block(self, corners, colour):
        """Eight corners: bottom four (x0z0, x1z0, x1z1, x0z1) then the top four, same order."""
        b0, b1, b2, b3, t0, t1, t2, t3 = corners
        self.quad(b0, b3, b2, b1, colour)   # bottom
        self.quad(t0, t1, t2, t3, colour)   # top
        self.quad(b0, b1, t1, t0, colour)   # z0 side
        self.quad(b2, b3, t3, t2, colour)   # z1 side
        self.quad(b1, b2, t2, t1, colour)   # x1 side
        self.quad(b3, b0, t0, t3, colour)   # x0 side

    def frustum(self, z0, z1, half0, half1, y0a, y1a, y0b, y1b, colour):
        """A box along Z whose cross-section changes: at z0 half-width half0 from y0a to y1a,
        at z1 half-width half1 from y0b to y1b (a tapering tail boom, a nose)."""
        self.block([(-half0, y0a, z0), (half0, y0a, z0), (half1, y0b, z1), (-half1, y0b, z1),
                    (-half0, y1a, z0), (half0, y1a, z0), (half1, y1b, z1), (-half1, y1b, z1)], colour)

    def line(self, a, b, width, colour):
        """A thin cord from a to b: two crossed strips, seen from any side."""
        d = [b[i] - a[i] for i in range(3)]
        length = math.sqrt(sum(v * v for v in d)) or 1
        d = [v / length for v in d]
        for up in ((1, 0, 0), (0, 0, 1)):
            s = cross(d, up)
            n = math.sqrt(sum(v * v for v in s)) or 1
            s = [v / n * width for v in s]
            self.quad(tuple(a[i] - s[i] for i in range(3)), tuple(a[i] + s[i] for i in range(3)),
                      tuple(b[i] + s[i] for i in range(3)), tuple(b[i] - s[i] for i in range(3)), colour, both=True)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def normal(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = cross(u, v)
    length = math.sqrt(sum(x * x for x in n)) or 1
    return tuple(x / length for x in n)


def helicopter():
    m = Mesh()
    # Skids and their struts.
    for x in (-1.25, 1.25):
        m.box(x - .06, x + .06, 0, .1, -2.3, 2.3, METAL)
        m.box(x - .06, x + .06, .05, .13, 2.3, 2.62, METAL)          # the upturned toe
        for z in (-1.3, 1.3):
            m.box(x * .82 - .05, x * .82 + .05, .1, .78, z - .05, z + .05, METAL)
    # Cabin: belly, walls with the doors open (a dark hold, seats inside), roof.
    m.box(-1.3, 1.3, .7, .95, -2.1, 2.2, BELLY)
    m.box(-1.3, 1.3, .95, 2.65, -2.1, -.95, OLIVE)                  # behind the doors
    m.box(-1.3, 1.3, .95, 2.65, 1.25, 2.2, OLIVE)                   # cockpit sides
    m.box(-1.28, 1.28, .95, 2.6, -.95, 1.25, INTERIOR)              # the open hold
    m.box(-1.3, 1.3, 2.6, 2.75, -.95, 1.25, OLIVE)                  # over the doors
    for side in (-1, 1):
        x = side * .9
        m.box(x - .32, x + .32, .95, 1.4, -.8, .9, SEAT)              # bench seats facing out
        back = x - side * .32
        m.box(min(back, back + side * .1), max(back, back + side * .1), 1.4, 2.0, -.8, .9, SEAT)
    # Side windows in the cockpit, a white stripe and Verda's green band down each flank.
    for side in (-1, 1):
        x0 = side * 1.3
        m.box(min(x0, x0 + side * .02), max(x0, x0 + side * .02), 1.75, 2.35, 1.4, 2.1, GLASS)
        m.box(min(x0, x0 + side * .02), max(x0, x0 + side * .02), 1.45, 1.55, -2.1, -.95, WHITE)
        m.box(min(x0, x0 + side * .02), max(x0, x0 + side * .02), 1.3, 1.45, -2.1, -.95, GREEN)
    # The nose: tapering forward and down, a big windscreen.
    m.frustum(2.2, 3.4, 1.3, .7, .75, 2.65, 1.0, 1.9, OLIVE)
    m.frustum(2.25, 3.25, 1.26, .74, 1.7, 2.62, 1.55, 1.92, GLASS)
    m.frustum(3.4, 3.75, .7, .35, 1.0, 1.9, 1.2, 1.6, OLIVE_DARK)
    # Engine housing on the roof, exhausts, the mast.
    m.box(-.85, .85, 2.75, 3.25, -1.8, 1.5, OLIVE)
    m.frustum(-2.4, -1.8, .55, .85, 2.75, 3.1, 2.75, 3.25, OLIVE)
    for x in (-.6, .6):
        m.box(x - .16, x + .16, 2.95, 3.25, -2.5, -2.1, METAL)
    m.box(-.14, .14, 3.25, ROTOR_HUB[1] - .05, ROTOR_HUB[2] - .14, ROTOR_HUB[2] + .14, METAL)
    # Tail boom tapering to the fin, the stabiliser, a red light.
    m.frustum(-7.4, -2.1, .16, .55, 2.55, 2.85, 1.55, 2.6, OLIVE)
    m.block([(-.06, 2.55, -7.9), (.06, 2.55, -7.9), (.06, 2.6, -7.0), (-.06, 2.6, -7.0),
             (-.06, 3.9, -8.15), (.06, 3.9, -8.15), (.06, 3.0, -7.25), (-.06, 3.0, -7.25)], OLIVE_DARK)
    m.box(-1.05, 1.05, 2.55, 2.63, -6.7, -6.2, OLIVE_DARK)
    m.box(-.04, .04, 3.88, 3.98, -8.18, -8.08, RED)
    m.box(.06, .26, TAIL_HUB[1] - .06, TAIL_HUB[1] + .06, TAIL_HUB[2] - .06, TAIL_HUB[2] + .06, METAL)
    # The flag on the boom: green field, white band.
    for side in (-1, 1):
        x = side * .5
        m.box(min(x, x + side * .02), max(x, x + side * .02), 2.0, 2.45, -3.6, -2.8, GREEN)
        m.box(min(x, x + side * .03), max(x, x + side * .03), 2.15, 2.3, -3.6, -2.8, WHITE)
    return m


def parachute():
    m = Mesh()
    cells, span, chord, rise, height = 9, 8.4, 3.0, 1.5, 6.6
    radius = (span / 2) ** 2 / (2 * rise) + rise / 2          # the arc through the tips
    half_angle = math.asin((span / 2) / radius)
    thick = .32

    def arc(t, lift=0.0):
        a = -half_angle + 2 * half_angle * t
        return radius * math.sin(a), height + radius * math.cos(a) - radius + rise + lift

    for i in range(cells):
        t0, t1 = i / cells, (i + 1) / cells
        x0, y0 = arc(t0)
        x1, y1 = arc(t1)
        colour = CANOPY_GREEN if i % 2 == 0 else CANOPY_WHITE
        # Top skin (an aerofoil hump from leading to trailing edge), underside, nose openings.
        prev = None
        steps = 5
        for k in range(steps + 1):
            f = k / steps
            z = chord / 2 - chord * f
            hump = thick * math.sin(math.pi * min(1, f * 1.3)) if f < .77 else thick * (1 - f) / .23 * .55
            row = ((x0, y0 + hump, z), (x1, y1 + hump, z))
            if prev:
                m.quad(prev[0], prev[1], row[1], row[0], colour)
                m.quad(row[0], row[1], prev[1], prev[0], CANOPY_UNDER)
            prev = row
        # The open nose of each cell (dark), and the rib walls.
        m.quad((x0, y0, chord / 2), (x1, y1, chord / 2), (x1, y1 + thick * .5, chord / 2), (x0, y0 + thick * .5, chord / 2), INTERIOR, both=True)
    # Lines: from the leading and trailing edge of each rib down to the harness at the origin,
    # gathered into two risers just above it.
    for i in range(cells + 1):
        x, y = arc(i / cells)
        for z in (chord * .35, -chord * .3):
            riser = (math.copysign(.18, x) if abs(x) > .01 else .18, .55, 0)
            m.line((x, y - .02, z), riser, .008, LINE)
    for side in (-.18, .18):
        m.line((side, .55, 0), (side * .6, 0, 0), .02, METAL)
    return m


MODELS = {"drop_helicopter": helicopter, "parachute": parachute}

if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    stale = []
    for name, build in MODELS.items():
        data = truck.glb(build().data(), name, False)
        path = OUT / f"{name}.glb"
        if "--check" in sys.argv:
            if not path.exists() or path.read_bytes() != data:
                stale.append(path.name)
        else:
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {len(data)} bytes")
    if stale:
        raise SystemExit("Stale drop models: " + ", ".join(stale) + f" (run {Path(__file__).name})")
