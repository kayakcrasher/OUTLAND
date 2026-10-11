#!/usr/bin/env python3
"""Generate the gun optics found as loot (assets/verda/combat/red_dot_sight.glb, scope_2x.glb,
scope_4x.glb): an open reflex red dot, a short 2x tube in rings and a squat 4x prism scope.
Vertex colours, metres, a few hundred vertices each. In the hands the game draws them itself on
each gun's rail (CombatRenderer); these models are the pickups on the ground and on shelves.

    python3 tools/generate_sight_models.py            # write the models
    python3 tools/generate_sight_models.py --check    # fail if the committed models are stale
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/verda/combat"
BLACK = (32, 34, 35, 255)
DARK = (22, 23, 24, 255)
LENS = (70, 110, 150, 255)
GLASS = (120, 180, 210, 255)
RED = (230, 40, 30, 255)
FIBRE = (120, 200, 60, 255)


class Mesh:
    def __init__(self):
        self.positions, self.normals, self.colours, self.indices = [], [], [], []

    def box(self, x0, x1, y0, y1, z0, z1, colour):
        faces = [((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
                 ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
                 ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
                 ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
                 ((0, 0, 1), [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)]),
                 ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)])]
        for normal, quad in faces:
            base = len(self.positions)
            self.positions.extend(quad)
            self.normals.extend([normal] * 4)
            self.colours.extend([colour] * 4)
            self.indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    def tube(self, z0, z1, radius, y, colour, cap=None, segments=12):
        """A cylinder along Z with flat end caps (`cap` colours the front one: the lens)."""
        ring0, ring1 = len(self.positions), len(self.positions) + segments
        for z in (z0, z1):
            for i in range(segments):
                a = 2 * math.pi * i / segments
                self.positions.append((radius * math.cos(a), y + radius * math.sin(a), z))
                self.normals.append((math.cos(a), math.sin(a), 0))
                self.colours.append(colour)
        for i in range(segments):
            j = (i + 1) % segments
            self.indices.extend([ring0 + i, ring0 + j, ring1 + j, ring0 + i, ring1 + j, ring1 + i])
        for z, sign, tint in ((z0, -1, colour), (z1, 1, cap or colour)):
            centre = len(self.positions)
            self.positions.append((0, y, z))
            self.normals.append((0, 0, sign))
            self.colours.append(tint)
            rim = len(self.positions)
            for i in range(segments):
                a = 2 * math.pi * i / segments
                self.positions.append((radius * math.cos(a), y + radius * math.sin(a), z))
                self.normals.append((0, 0, sign))
                self.colours.append(tint)
            for i in range(segments):
                j = (i + 1) % segments
                if sign > 0:
                    self.indices.extend([centre, rim + i, rim + j])
                else:
                    self.indices.extend([centre, rim + j, rim + i])

    def data(self):
        return self.positions, self.normals, self.colours, self.indices


def red_dot():
    m = Mesh()
    m.box(-.014, .014, 0, .008, -.02, .02, BLACK)          # mount and clamp
    m.box(-.018, -.014, .008, .046, -.015, .015, BLACK)    # open hood
    m.box(.014, .018, .008, .046, -.015, .015, BLACK)
    m.box(-.018, .018, .046, .051, -.015, .015, BLACK)
    m.box(-.014, .014, .012, .044, .006, .008, GLASS)      # window
    m.box(-.002, .002, .026, .03, .0045, .006, RED)        # the dot's emitter glint
    m.box(.018, .022, .02, .03, -.006, .006, DARK)         # brightness knob
    return m


def scope_2x():
    m = Mesh()
    for z in (-.03, .03):
        m.box(-.008, .008, 0, .024, z - .005, z + .005, BLACK)  # rings
    m.tube(-.06, .05, .0145, .032, BLACK)
    m.tube(.04, .072, .019, .032, BLACK, LENS)
    m.tube(-.075, -.05, .017, .032, BLACK, LENS)
    m.box(-.004, .004, .046, .052, -.01, .01, DARK)        # turret
    return m


def scope_4x():
    m = Mesh()
    m.box(-.015, .015, 0, .016, -.045, .045, BLACK)       # base
    m.box(-.017, .017, .016, .046, -.055, .055, BLACK)     # prism housing
    m.tube(.05, .088, .021, .036, BLACK, LENS)
    m.tube(-.088, -.05, .017, .036, BLACK, LENS)
    m.box(-.004, .004, .046, .052, -.035, .035, FIBRE)     # illumination fibre
    return m


MODELS = {"red_dot_sight": red_dot, "scope_2x": scope_2x, "scope_4x": scope_4x}

if __name__ == "__main__":
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
        raise SystemExit("Stale sight models: " + ", ".join(stale) + f" (run {Path(__file__).name})")
