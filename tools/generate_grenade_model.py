#!/usr/bin/env python3
"""Generate the frag grenade (assets/verda/combat/frag_grenade.glb): an olive egg-shaped body,
a fuse head, the spoon lever down one side and the pull ring. Vertex colours, metres, about
150 vertices; used for loot pickups and for grenades in flight.

    python3 tools/generate_grenade_model.py            # write the model
    python3 tools/generate_grenade_model.py --check    # fail if the committed model is stale
"""
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/verda/combat/frag_grenade.glb"
OLIVE = (74, 88, 52, 255)
STEEL = (150, 152, 150, 255)
DARK = (46, 48, 44, 255)


def build():
    positions, normals, colours, indices = [], [], [], []

    def add_box(x0, x1, y0, y1, z0, z1, colour):
        faces = [((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
                 ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
                 ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
                 ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
                 ((0, 0, 1), [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)]),
                 ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)])]
        for normal, quad in faces:
            base = len(positions)
            positions.extend(quad)
            normals.extend([normal] * 4)
            colours.extend([colour] * 4)
            indices.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    # Body: a slightly egg-shaped sphere, 6.4 cm across and 8.6 cm tall, sitting on y = 0.
    rings, segments = 7, 10
    rx, ry, cy = .032, .043, .043
    start = len(positions)
    for r in range(rings + 1):
        phi = math.pi * r / rings
        for s in range(segments):
            theta = 2 * math.pi * s / segments
            n = (math.sin(phi) * math.cos(theta), math.cos(phi), math.sin(phi) * math.sin(theta))
            positions.append((rx * n[0], cy + ry * n[1], rx * n[2]))
            normals.append(n)
            colours.append(OLIVE)
    for r in range(rings):
        for s in range(segments):
            a = start + r * segments + s
            b = start + r * segments + (s + 1) % segments
            c, d = a + segments, b + segments
            indices.extend([a, c, d, a, d, b])
    add_box(-.012, .012, .082, .1, -.012, .012, STEEL)          # fuse head
    add_box(.012, .02, .03, .098, -.008, .008, STEEL)           # spoon down the side
    add_box(-.003, .003, .1, .104, -.016, .016, DARK)           # ring
    add_box(-.003, .003, .1, .124, .014, .018, DARK)
    add_box(-.003, .003, .1, .124, -.018, -.014, DARK)
    add_box(-.003, .003, .12, .124, -.016, .016, DARK)
    return positions, normals, colours, indices


if __name__ == "__main__":
    data = truck.glb(build(), "frag_grenade", False)
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_bytes() != data:
            raise SystemExit(f"Stale grenade model: run {Path(__file__).name}")
    else:
        OUT.write_bytes(data)
        print(f"wrote {OUT.relative_to(ROOT)}: {len(data)} bytes")
