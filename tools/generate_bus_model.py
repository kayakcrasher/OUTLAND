#!/usr/bin/env python3
"""Generate Verda's city bus (assets/verda/vehicles/runtime/bus_body.glb).

An original low-poly 12 m single-decker built from boxes with vertex colours (no textures, so
every raylib build draws it): Verda green below a white stripe, a dark glazed window band with
pillars, a white upper body and roof unit, a lit destination board, two doors on the kerb side
(-X: the bus's right, the side traffic keeps to; the driver sits on the left), headlights, tail lights, bumpers and wheel arches. +Z is the
front, Y is up, metres. Wheels are drawn separately by the game.

    python3 tools/generate_bus_model.py            # write the model
    python3 tools/generate_bus_model.py --check    # fail if the committed model is stale
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/verda/vehicles/runtime/bus_body.glb"

GREEN = (0, 122, 51)
WHITE = (238, 238, 228)
STRIPE = (250, 250, 245)
GLASS = (40, 58, 68, 100)      # tinted, see-through: the driver and seats show
SCREEN = (60, 84, 98, 75)
SEAT = (70, 74, 80)
FLOOR = (54, 56, 58)
DARK = (34, 34, 36)
GREY = (150, 152, 150)
AMBER = (255, 168, 36)
LAMP = (255, 246, 210)
RED = (200, 24, 20)
HALF_W = 1.275

boxes = []


def box(x0, x1, y0, y1, z0, z1, colour):
    boxes.append((min(x0, x1), max(x0, x1), min(y0, y1), max(y0, y1), min(z0, z1), max(z0, z1), colour))


def build():
    W = HALF_W
    # Interior first, so the tinted glass (drawn after it) lies over it: floor, seat rows, the
    # driver's console and wheel.
    box(-W + .08, W - .08, 1.28, 1.30, -5.9, 5.9, FLOOR)
    z = -5.2
    while z < 3.9:
        for side in (-1, 1):
            box(side * .35, side * 1.15, 1.30, 1.72, z - .22, z + .22, SEAT)
            box(side * .35, side * 1.15, 1.72, 2.05, z - .26, z - .16, SEAT)
        z += 1.05
    box(.2, 1.2, 1.30, 1.95, 5.35, 5.9, DARK)                      # dashboard (driver on the left, +X)
    box(.6, .9, 1.75, 2.05, 5.2, 5.28, DARK)                       # wheel
    # Body: green skirt, white stripe, glazed band, white upper body, roof and its air unit.
    box(-W, W, .32, 1.15, -6, 6, GREEN)
    box(-W, W, 1.15, 1.28, -6, 6, STRIPE)
    box(-W + .03, W - .03, 1.30, 2.45, -5.95, 5.95, GLASS)
    box(-W, W, 2.45, 3.02, -6, 6, WHITE)
    box(-W + .05, W - .05, 3.02, 3.12, -5.9, 5.9, GREY)
    box(-.8, .8, 3.12, 3.42, -1.4, 1.6, GREY)
    # Window pillars down both sides, leaving the doors clear on the kerb side.
    z = -5.5
    while z < 5.6:
        for side in (-1, 1):
            in_door = side < 0 and (4.45 < z < 5.85 or -0.35 < z < 1.15)
            if not in_door:
                box(side * (W - .02), side * (W + .01), 1.30, 2.45, z - .07, z + .07, GREEN)
        z += 1.35
    # Doors (kerb side, -X): glass leaves in a dark frame.
    for z0, z1 in ((4.55, 5.75), (-0.25, 1.05)):
        box(-W + .02, -W - .02, .34, 2.42, z0 - .06, z1 + .06, DARK)
        box(-W - .02, -W - .03, .45, 2.35, z0, (z0 + z1) / 2 - .02, SCREEN)
        box(-W - .02, -W - .03, .45, 2.35, (z0 + z1) / 2 + .02, z1, SCREEN)
    # Front: windscreen, destination board (lit), headlights, indicators, bumper, mirrors.
    box(-W + .1, W - .1, 1.0, 2.42, 5.96, 6.03, SCREEN)
    box(-1.05, 1.05, 2.5, 2.88, 5.98, 6.05, DARK)
    box(-.9, .9, 2.58, 2.8, 6.05, 6.07, AMBER)
    for side in (-1, 1):
        box(side * .78, side * 1.1, .55, .74, 6.0, 6.05, LAMP)
        box(side * 1.1, side * 1.22, .55, .74, 6.0, 6.05, AMBER)
        box(side * W, side * (W + .22), 1.95, 2.35, 5.72, 5.82, DARK)   # mirror arm and head
    box(-W, W, .30, .55, 6.0, 6.14, DARK)
    # Rear: window, tail lights, bumper, engine grille.
    box(-1.0, 1.0, 1.65, 2.4, -6.03, -5.96, GLASS)
    for side in (-1, 1):
        box(side * .85, side * 1.2, .6, 1.0, -6.04, -5.99, RED)
    box(-.7, .7, .6, 1.05, -6.04, -6.0, DARK)
    box(-W, W, .30, .55, -6.14, -6.0, DARK)
    # Wheel arches over the axles (front +3.5, rear -3.0).
    for z_axle in (3.5, -3.0):
        for side in (-1, 1):
            box(side * (W - .01), side * (W + .02), .32, 1.08, z_axle - .62, z_axle + .62, DARK)


def mesh():
    positions, normals, colours, indices = [], [], [], []
    faces = [  # normal, four corners as (x, y, z) selectors over (lo, hi)
        ((1, 0, 0), [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)]),
        ((-1, 0, 0), [(0, 0, 1), (0, 1, 1), (0, 1, 0), (0, 0, 0)]),
        ((0, 1, 0), [(0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0)]),
        ((0, -1, 0), [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)]),
        ((0, 0, 1), [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)]),
        ((0, 0, -1), [(1, 0, 0), (0, 0, 0), (0, 1, 0), (1, 1, 0)]),
    ]
    for x0, x1, y0, y1, z0, z1, colour in boxes:
        lo, hi = (x0, y0, z0), (x1, y1, z1)
        for normal, corners in faces:
            base = len(positions)
            for c in corners:
                positions.append(tuple(hi[i] if c[i] else lo[i] for i in range(3)))
                normals.append(normal)
                colours.append(colour if len(colour) == 4 else colour + (255,))
            # Counter-clockwise seen from outside: wind each triangle to agree with its normal.
            for a, b, c in ((0, 1, 2), (0, 2, 3)):
                pa, pb, pc = positions[base + a], positions[base + b], positions[base + c]
                u = [pb[i] - pa[i] for i in range(3)]
                v = [pc[i] - pa[i] for i in range(3)]
                n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
                if sum(n[i] * normal[i] for i in range(3)) >= 0:
                    indices += [base + a, base + b, base + c]
                else:
                    indices += [base + a, base + c, base + b]
    return positions, normals, colours, indices


def glb():
    build()
    positions, normals, colours, indices = mesh()
    blob = bytearray()

    def view(data, target):
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        blob.extend(data)
        return {"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target}

    pos = b"".join(struct.pack("<3f", *p) for p in positions)
    nor = b"".join(struct.pack("<3f", *n) for n in normals)
    col = b"".join(struct.pack("<4B", *c) for c in colours)
    idx = b"".join(struct.pack("<I", i) for i in indices)
    views = [view(pos, 34962), view(nor, 34962), view(col, 34962), view(idx, 34963)]
    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    document = {
        "asset": {"version": "2.0", "generator": "OUTLAND tools/generate_bus_model.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": "verda_bus"}],
        "meshes": [{"name": "verda_bus", "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2}, "indices": 3, "material": 0}]}],
        "materials": [{"name": "bus_paint", "alphaMode": "BLEND", "pbrMetallicRoughness": {
            "baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": 1}}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5121, "normalized": True, "count": len(colours), "type": "VEC4"},
            {"bufferView": 3, "componentType": 5125, "count": len(indices), "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    text = json.dumps(document, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    blob += b"\0" * (-len(blob) % 4)
    out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(text) + 8 + len(blob))
    out += struct.pack("<II", len(text), 0x4E4F534A) + text
    out += struct.pack("<II", len(blob), 0x004E4942) + bytes(blob)
    return out


if __name__ == "__main__":
    data = glb()
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_bytes() != data:
            raise SystemExit(f"Stale bus model: run {Path(__file__).name}")
    else:
        OUT.write_bytes(data)
        print(f"wrote {OUT.relative_to(ROOT)}: {len(boxes)} parts, {len(data)} bytes")
