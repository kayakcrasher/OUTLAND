#!/usr/bin/env python3
"""Generate Verda's pickup truck (assets/verda/vehicles/runtime/truck_body.glb, truck_wheel.glb).

An original, light (about 1,500 vertices) full-size pickup built from boxes with vertex colours:
a long bonnet, a two-door cab with tinted see-through glass, seats, dashboard and wheel inside,
an open bed with a tailgate, wheel arches over 0.8 m tyres, chrome bumpers, head and tail lights
and mirrors. +Z is the front, Y is up, metres, ground at Y = 0; the driver sits on the left (+X).
The wheel is a 1 m tyre and hub (scaled to the manifest's radius by the game) with its tread on
Y = 0 and the axle along X.

    python3 tools/generate_truck_model.py            # write the models
    python3 tools/generate_truck_model.py --check    # fail if the committed models are stale
"""
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_bus_model as kit  # noqa: E402  (box() and mesh() from the bus generator)

ROOT = Path(__file__).resolve().parents[1]
BODY = ROOT / "assets/verda/vehicles/runtime/truck_body.glb"
WHEEL = ROOT / "assets/verda/vehicles/runtime/truck_wheel.glb"

PAINT = (150, 34, 30)
PAINT_DARK = (112, 24, 22)
CHROME = (196, 198, 200)
DARK = (30, 30, 32)
BED = (58, 56, 54)
GLASS = (36, 52, 62, 110)
SEAT = (64, 60, 56)
LAMP = (255, 246, 210)
AMBER = (255, 168, 36)
RED = (200, 24, 20)

HALF_W = .975
FRONT, REAR = 2.65, -2.65
FRONT_AXLE, REAR_AXLE = 1.65, -1.55
ARCH = .5     # half length of a wheel arch
SILL = .5     # body bottom between the wheels
ARCH_TOP = .95


def body_side(z0, z1, top, colour):
    """Body panel from z0 to z1 up to `top`, cut away over the wheels."""
    cuts = sorted([(FRONT_AXLE - ARCH, FRONT_AXLE + ARCH), (REAR_AXLE - ARCH, REAR_AXLE + ARCH)])
    z = z0
    for c0, c1 in cuts:
        if c1 <= z or c0 >= z1:
            continue
        if c0 > z:
            kit.box(-HALF_W, HALF_W, SILL, top, z, c0, colour)
        kit.box(-HALF_W, HALF_W, ARCH_TOP, top, max(z, c0), min(z1, c1), colour)
        for side in (-1, 1):  # dark arch liner
            kit.box(side * (HALF_W - .02), side * (HALF_W + .01), ARCH_TOP - .06, ARCH_TOP, max(z, c0), min(z1, c1), DARK)
        z = min(z1, c1)
    if z < z1:
        kit.box(-HALF_W, HALF_W, SILL, top, z, z1, colour)


def build():
    kit.boxes.clear()
    W = HALF_W
    # Interior first, under the glass: floor, bench seat, dashboard, wheel.
    kit.box(-W + .06, W - .06, .9, .95, -.7, 1.0, DARK)
    kit.box(-W + .1, W - .1, .95, 1.4, -.55, -.1, SEAT)
    kit.box(-W + .1, W - .1, 1.4, 1.85, -.62, -.45, SEAT)
    kit.box(-W + .08, W - .08, 1.15, 1.42, .8, 1.02, DARK)
    kit.box(.3, .62, 1.35, 1.62, .68, .74, DARK)
    # Frame, bonnet, cab and bed.
    kit.box(-.62, .62, .32, .5, -2.45, 2.45, DARK)
    body_side(1.05, FRONT - .05, 1.22, PAINT)                    # front wings and bonnet
    kit.box(-W + .05, W - .05, 1.22, 1.25, 1.1, FRONT - .1, PAINT_DARK)   # bonnet shut line
    body_side(-.72, 1.05, 1.22, PAINT)                           # cab doors
    kit.box(-W, W, 1.22, 1.3, -.72, 1.05, PAINT)                 # belt line
    kit.box(-W + .04, W - .04, 1.3, 1.9, -.68, .82, GLASS)        # the greenhouse
    kit.box(-W + .02, W - .02, 1.9, 1.96, -.72, .86, PAINT)      # roof
    for z in (-.7, .84):                                         # pillars
        for side in (-1, 1):
            kit.box(side * (W - .06), side * W, 1.3, 1.92, z - .05, z + .05, PAINT)
    for side in (-1, 1):                                         # door gaps and handles
        kit.box(side * W, side * (W + .005), .55, 1.3, .2, .23, PAINT_DARK)
        kit.box(side * W, side * (W + .02), 1.05, 1.1, -.1, .1, CHROME)
    # Bed: a floor above the rear wheels, walls 7 cm thick, a front wall and a tailgate - open above.
    kit.box(-W + .07, W - .07, .82, .88, REAR + .07, -.8, BED)
    kit.box(-W, W, .5, 1.3, -.8, -.72, PAINT)
    kit.box(-W + .07, W - .07, .5, .82, REAR + .07, REAR + .3, PAINT_DARK)   # rear cross-member
    kit.box(-W + .07, W - .07, .6, 1.25, REAR, REAR + .07, PAINT)            # tailgate
    for side in (-1, 1):
        for z0, z1 in ((REAR, REAR_AXLE - ARCH), (REAR_AXLE + ARCH, -.72)):
            kit.box(side * (W - .07), side * W, SILL, 1.25, z0, z1, PAINT)
        kit.box(side * (W - .07), side * W, ARCH_TOP, 1.25, REAR_AXLE - ARCH, REAR_AXLE + ARCH, PAINT)
        kit.box(side * (W - .07), side * W, 1.25, 1.3, REAR, -.72, PAINT)   # rail cap
    # Front: grille, headlights, indicators, bumper. Rear: tail lights, bumper, plate.
    kit.box(-.62, .62, .62, 1.12, FRONT - .05, FRONT, DARK)
    for side in (-1, 1):
        kit.box(side * .64, side * .9, .85, 1.08, FRONT - .05, FRONT, LAMP)
        kit.box(side * .64, side * .9, .74, .83, FRONT - .05, FRONT, AMBER)
        kit.box(side * .72, side * .95, .9, 1.22, REAR - .02, REAR + .03, RED)
        kit.box(side * W, side * (W + .2), 1.42, 1.6, .9, 1.0, DARK)  # mirror
    kit.box(-W - .02, W + .02, .38, .62, FRONT, FRONT + .1, CHROME)
    kit.box(-W - .02, W + .02, .38, .58, REAR - .1, REAR, CHROME)
    kit.box(-.25, .25, .62, .78, REAR - .11, REAR - .1, (226, 224, 214))


def wheel():
    """A 1 m tyre (x -0.25..0.25) with a hub, tread at y = 0, axle along X through y = 0.5."""
    positions, normals, colours, indices = [], [], [], []
    segments = 14

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

    tyre = (26, 26, 28)
    hub = (170, 172, 176)
    outer_l, outer_r = ring(.5, -.25, tyre, True), ring(.5, .25, tyre, True)
    strip(outer_l, outer_r)
    strip(outer_r, outer_l)  # both windings: the tread shows from any side
    for x in (-.25, .25):
        rim = ring(.5, x, tyre, False)
        hub_ring = ring(.3, x * 1.02, hub, False)
        strip(rim, hub_ring)
        strip(hub_ring, rim)
        centre = len(positions)
        positions.append((x * 1.04, .5, 0))
        normals.append((1 if x > 0 else -1, 0, 0))
        colours.append(hub + (255,))
        for i in range(segments):
            j = (i + 1) % segments
            indices.extend([centre, hub_ring + i, hub_ring + j, centre, hub_ring + j, hub_ring + i])
    return positions, normals, colours, indices


def glb(mesh_data, name, blend):
    positions, normals, colours, indices = mesh_data
    blob = bytearray()

    def view(data, target):
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        blob.extend(data)
        return {"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": target}

    views = [view(b"".join(struct.pack("<3f", *p) for p in positions), 34962),
             view(b"".join(struct.pack("<3f", *n) for n in normals), 34962),
             view(b"".join(struct.pack("<4B", *c) for c in colours), 34962),
             view(b"".join(struct.pack("<H", i) for i in indices), 34963)]
    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    material = {"name": name + "_paint", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": 1}}
    if blend:
        material["alphaMode"] = "BLEND"
    document = {
        "asset": {"version": "2.0", "generator": "OUTLAND tools/generate_truck_model.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2}, "indices": 3, "material": 0}]}],
        "materials": [material],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5121, "normalized": True, "count": len(colours), "type": "VEC4"},
            {"bufferView": 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
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
    build()
    outputs = {BODY: glb(kit.mesh(), "verda_pickup", True), WHEEL: glb(wheel(), "verda_pickup_wheel", False)}
    for path, data in outputs.items():
        if "--check" in sys.argv:
            if not path.exists() or path.read_bytes() != data:
                raise SystemExit(f"Stale truck model {path.name}: run {Path(__file__).name}")
        else:
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {len(data)} bytes")
