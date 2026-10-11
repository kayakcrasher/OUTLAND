#!/usr/bin/env python3
"""Pull one house out of the residential scene (assets/verda/residential/houses/source/house.glb).

The source is a whole neighbourhood (33 houses, a furnished interior, trees, fences, lamps:
255k vertices) and its textures did not survive export, so it cannot be placed as is. This takes
the one house node, bakes it to ground level centred on its footprint (front porch toward -Z),
paints each material with a flat colour and merges everything into a single vertex-coloured
primitive. The paint schemes are written as assets/verda/residential/houses/house_<scheme>.glb.

The house is made enterable: the front door panel (beside the garage) is left out so its opening
in the wall is a doorway, the inside faces of the walls are painted plaster, and the hollow shell
gets a floor, a ceiling at first-storey height, a garage partition with a doorway, and furniture -
a living room, dining table, kitchen along the back extension and a garage workbench - and a
flight of stairs up to a carpeted upper floor with a master bedroom over the garage, a front
and a back bedroom, and a bathroom in the back extension. Collision
is built from the same triangles, so the doorway, walls and furniture are real.

    python3 tools/extract_house.py [--check]
"""
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FOLDER = ROOT / "assets/verda/residential/houses"
SOURCE = FOLDER / "source/house.glb"
NODE = "House"  # mesh 679, identity rotation: front porch on -Z

TRIM = (238, 236, 228, 255)
COMMON = {
    "ConcreteBare": (150, 148, 142, 255),
    "WoodFine_01": TRIM,
    "Wood": (132, 100, 72, 255),
    "WoodPlanks_01": (142, 108, 78, 255),
    "Wood_06": TRIM,
    "Wood_03": (150, 116, 84, 255),
    "Garage_Door": (230, 228, 220, 255),
    "Windows": (70, 92, 108, 255),
    "Metal": (64, 66, 70, 255),
}
SCHEMES = {
    "white": {"WoodPlanks": (226, 224, 214, 255), "RooftilesSlate": (66, 72, 80, 255), "Door": (128, 34, 38, 255)},
    "blue": {"WoodPlanks": (150, 176, 196, 255), "RooftilesSlate": (58, 60, 64, 255), "Door": (36, 52, 80, 255)},
    "sage": {"WoodPlanks": (168, 182, 150, 255), "RooftilesSlate": (96, 74, 62, 255), "Door": (42, 64, 48, 255)},
    "cream": {"WoodPlanks": (232, 214, 172, 255), "RooftilesSlate": (82, 86, 92, 255), "Door": (24, 26, 30, 255)},
}

# Interior, in the model's local metres (origin at the footprint centre on the ground, porch -Z).
# Main room x -6.58..6.73, z -5.6..6.15; the back extension x 0.35..6.73, z 6.15..8.44.
PLASTER = (232, 226, 212, 255)
GARAGE_WALL = (176, 174, 168, 255)
FLOOR_WOOD = (156, 116, 80, 255)
CARPET = (176, 164, 146, 255)
CONCRETE = (128, 128, 124, 255)
CEILING = (240, 238, 232, 255)
WOOD_DARK = (110, 80, 56, 255)
WOOD_MID = (100, 72, 48, 255)
LINEN = (236, 234, 226, 255)
FRONT_DOOR_Z = -5.6   # door panels in front of this are left out (the doorway)
FLOOR_TOP, CEILING_Y = .06, 3.2
UPPER = CEILING_Y + .1  # upstairs floor top
WALL_TOP = UPPER + 2.1  # upstairs partitions stop under the roof
GARAGE_X = -3.45      # partition between the garage (x below) and the hall
# Stairs climb +Z along the garage partition, from the hall up to a landing at the back.
STAIR_X0, STAIR_X1, STAIR_Z0, STEPS, TREAD = -3.4, -2.3, -2.0, 17, .28
STAIR_Z1 = STAIR_Z0 + STEPS * TREAD   # 2.76: top step edge, upstairs floor beyond
RISE = UPPER / STEPS

INTERIOR = [  # (x0, x1, y0, y1, z0, z1, colour)
    # Ground floor: floors.
    (-3.4, 6.73, 0, FLOOR_TOP, -5.6, 6.15, FLOOR_WOOD),
    (.35, 6.73, 0, FLOOR_TOP, 6.15, 8.44, (210, 206, 196, 255)),        # kitchen tiles
    (-6.58, GARAGE_X - .15, 0, FLOOR_TOP, -5.6, 6.15, CONCRETE),
    # Upstairs floor (ground-floor ceiling) with the stairwell open.
    (-6.58, STAIR_X0, CEILING_Y, UPPER, -5.6, 6.15, CARPET),
    (STAIR_X1, 6.73, CEILING_Y, UPPER, -5.6, 6.15, CARPET),
    (STAIR_X0, STAIR_X1, CEILING_Y, UPPER, -5.6, STAIR_Z0, CARPET),
    (STAIR_X0, STAIR_X1, CEILING_Y, UPPER, STAIR_Z1, 6.15, CARPET),
    (.35, 6.73, CEILING_Y, UPPER, 6.15, 8.44, (214, 216, 218, 255)),    # bathroom tiles
    # Garage partition: doorway z -4.6..-3.6 (just inside the front door), 2.2 m high.
    (GARAGE_X - .15, GARAGE_X, 0, CEILING_Y, -5.6, -4.6, PLASTER),
    (GARAGE_X - .15, GARAGE_X, 0, CEILING_Y, -3.6, 6.15, PLASTER),
    (GARAGE_X - .15, GARAGE_X, 2.2, CEILING_Y, -4.6, -3.6, PLASTER),
    # Living room: rug, sofa facing the TV on the right wall, coffee table, armchair.
    (1.6, 5.4, FLOOR_TOP, FLOOR_TOP + .02, -4.4, -.6, (138, 54, 50, 255)),
    (1.8, 2.7, FLOOR_TOP, .45, -4.0, -1.0, (70, 86, 110, 255)),
    (1.8, 2.05, .45, .95, -4.0, -1.0, (70, 86, 110, 255)),
    (1.8, 2.7, .45, .7, -4.0, -3.8, (70, 86, 110, 255)),
    (1.8, 2.7, .45, .7, -1.2, -1.0, (70, 86, 110, 255)),
    (3.5, 4.5, FLOOR_TOP, .45, -3.0, -2.0, WOOD_DARK),
    (6.2, 6.73, FLOOR_TOP, .55, -3.4, -1.6, (60, 50, 44, 255)),           # TV stand
    (6.5, 6.6, .55, 1.35, -3.2, -1.8, (24, 24, 26, 255)),                 # TV
    (3.6, 4.5, FLOOR_TOP, .45, -5.3, -4.5, (150, 120, 84, 255)),          # armchair
    (3.6, 4.5, .45, .9, -5.3, -5.1, (150, 120, 84, 255)),
    # Dining table and four chairs.
    (2.4, 4.4, .72, .78, 1.9, 3.1, (120, 86, 58, 255)),
    (2.5, 2.6, FLOOR_TOP, .72, 2.0, 2.1, WOOD_MID), (4.2, 4.3, FLOOR_TOP, .72, 2.0, 2.1, WOOD_MID),
    (2.5, 2.6, FLOOR_TOP, .72, 2.9, 3.0, WOOD_MID), (4.2, 4.3, FLOOR_TOP, .72, 2.9, 3.0, WOOD_MID),
    (2.8, 3.25, FLOOR_TOP, .46, 1.3, 1.75, WOOD_MID), (3.55, 4.0, FLOOR_TOP, .46, 1.3, 1.75, WOOD_MID),
    (2.8, 3.25, FLOOR_TOP, .46, 3.25, 3.7, WOOD_MID), (3.55, 4.0, FLOOR_TOP, .46, 3.25, 3.7, WOOD_MID),
    # Kitchen along the extension's back wall, fridge by its door, a cabinet run on the right.
    (.6, 6.5, FLOOR_TOP, .88, 7.8, 8.44, (236, 234, 228, 255)),
    (.6, 6.5, .88, .94, 7.75, 8.44, (54, 56, 60, 255)),
    (6.1, 6.73, FLOOR_TOP, .88, 6.4, 7.75, (236, 234, 228, 255)),
    (6.05, 6.73, .88, .94, 6.4, 7.75, (54, 56, 60, 255)),
    (.6, 1.4, FLOOR_TOP, 1.9, 6.5, 7.2, (196, 198, 200, 255)),
    (.6, 6.5, 1.5, 2.2, 8.1, 8.44, (226, 222, 212, 255)),                 # wall cupboards
    # Garage: workbench, shelves and boxes.
    (-6.58, -5.9, 0, .9, 3.2, 5.9, (120, 96, 70, 255)),
    (-6.58, -6.1, 0, 2.0, -.5, 2.6, (90, 92, 96, 255)),
    (-5.6, -5.0, 0, .5, 5.2, 5.8, (168, 140, 100, 255)), (-4.8, -4.3, 0, .4, 5.3, 5.9, (168, 140, 100, 255)),

    # Upstairs. A hall runs front to back beside the stairwell (x -3.6..-0.6) with a rail along
    # the well; the master bedroom is over the garage, two bedrooms on the right, the bathroom
    # in the back extension. Partitions are 2.1 m and stop where the roof comes down.
    (-3.75, -3.6, UPPER, WALL_TOP, -4.2, 2.9, PLASTER),                  # master bedroom wall,
    (-3.75, -3.6, UPPER, WALL_TOP, 3.9, 4.0, PLASTER),                   #   door z 2.9..3.9
    (-.75, -.6, UPPER, WALL_TOP, -5.6, -3.4, PLASTER),                   # right-hand rooms' wall,
    (-.75, -.6, UPPER, WALL_TOP, -2.4, .4, PLASTER),                     #   doors z -3.4..-2.4
    (-.75, -.6, UPPER, WALL_TOP, 1.4, 4.0, PLASTER),                     #   and z 0.4..1.4
    (-.6, 6.73, UPPER, WALL_TOP, -.75, -.6, PLASTER),                    # front / back bedroom
    (-6.58, 3.0, UPPER, WALL_TOP, 4.0, 4.15, PLASTER),                   # back wall, bathroom
    (4.0, 6.73, UPPER, WALL_TOP, 4.0, 4.15, PLASTER),                    #   door x 3.0..4.0
    (STAIR_X1, STAIR_X1 + .08, UPPER, UPPER + .95, STAIR_Z0, STAIR_Z1, WOOD_DARK),   # rail
    (STAIR_X0, STAIR_X1, UPPER, UPPER + .95, STAIR_Z0 - .08, STAIR_Z0, WOOD_DARK),
    # Master bedroom: double bed with its head to the back wall, a nightstand, wardrobe, rug.
    (-6.4, -4.9, UPPER, UPPER + .5, 1.9, 3.88, LINEN),
    (-6.45, -4.85, UPPER, UPPER + 1.1, 3.88, 4.0, WOOD_DARK),
    (-6.35, -4.95, UPPER + .5, UPPER + .58, 1.95, 3.3, (120, 140, 168, 255)),  # duvet
    (-6.58, -6.45, UPPER, UPPER + .55, 3.4, 3.88, WOOD_MID),
    (-6.58, -6.0, UPPER, UPPER + 2.0, -2.2, -.8, (200, 190, 170, 255)),     # wardrobe
    (-6.0, -4.4, UPPER, UPPER + .02, -.4, 1.6, (120, 96, 120, 255)),
    # Front bedroom: double bed with its head to the partition, dresser on the right wall.
    (1.8, 3.8, UPPER, UPPER + .5, -2.7, -.75, LINEN),
    (1.7, 3.9, UPPER, UPPER + 1.1, -.75, -.6, WOOD_DARK),
    (1.85, 3.75, UPPER + .5, UPPER + .58, -2.6, -1.1, (168, 92, 82, 255)),
    (6.2, 6.73, UPPER, UPPER + 1.0, -4.6, -3.0, WOOD_MID),
    (4.2, 4.7, UPPER, UPPER + .55, -1.4, -.75, WOOD_MID),
    # Back bedroom: two single beds on the right wall, a desk and chair by the back wall.
    (4.8, 6.7, UPPER, UPPER + .45, 0, .95, LINEN), (5.2, 6.7, UPPER + .45, UPPER + .52, .05, .9, (92, 138, 112, 255)),
    (4.8, 6.7, UPPER, UPPER + .45, 2.3, 3.25, LINEN), (5.2, 6.7, UPPER + .45, UPPER + .52, 2.35, 3.2, (196, 150, 70, 255)),
    (6.62, 6.73, UPPER, UPPER + .9, -.1, 3.4, WOOD_DARK),
    (1.0, 2.4, UPPER + .72, UPPER + .78, 3.4, 4.0, WOOD_MID),
    (1.0, 1.1, UPPER, UPPER + .72, 3.4, 3.5, WOOD_MID), (2.3, 2.4, UPPER, UPPER + .72, 3.4, 3.5, WOOD_MID),
    (1.45, 1.9, UPPER, UPPER + .46, 2.8, 3.25, (60, 60, 64, 255)),
    # Bathroom: bath, toilet and basin along the extension's back wall.
    (4.8, 6.6, UPPER, UPPER + .55, 7.6, 8.44, (244, 244, 240, 255)),
    (1.0, 1.5, UPPER, UPPER + .42, 7.75, 8.3, (244, 244, 240, 255)), (1.0, 1.5, UPPER + .42, UPPER + .8, 8.2, 8.44, (244, 244, 240, 255)),
    (2.4, 3.4, UPPER, UPPER + .85, 7.95, 8.44, (226, 222, 212, 255)),
]
# The flight: one solid block per step.
for _step in range(STEPS):
    INTERIOR.append((STAIR_X0, STAIR_X1, 0, (_step + 1) * RISE, STAIR_Z0 + _step * TREAD, STAIR_Z0 + (_step + 1) * TREAD, WOOD_DARK))


def box(x0, x1, y0, y1, z0, z1):
    """Six outward quads: (positions, normals, indices) for a box."""
    faces = [((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
             ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
             ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
             ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
             ((0, 0, 1), [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)]),
             ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)])]
    positions, normals, indices = [], [], []
    for normal, quad in faces:
        base = len(positions)
        positions += quad
        normals += [normal] * 4
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    return positions, normals, indices


def indoors(x, z):
    return (-6.58 < x < 6.73 and -5.6 < z < 6.15) or (.35 < x < 6.73 and 6.15 <= z < 8.44)


def faces_inside(shell):
    """For each shell triangle: does it face into the house? A ray from its centre along its
    normal hits another part of the house (within the footprint) when it faces in,
    and escapes to the open air when it faces out."""
    import numpy as np
    tris = np.array([corners for _, corners, _, _ in shell], dtype=np.float64)
    normals = np.array([normal for _, _, normal, _ in shell], dtype=np.float64)
    centres = tris.mean(axis=1)
    enclosing = np.ones(len(shell), dtype=bool)  # walls, roof, windows and trim all close the house
    footprint = (np.abs(centres[:, 0]) <= 7.3) & (centres[:, 2] >= -6.2) & (centres[:, 2] <= 8.8)
    targets = tris[enclosing & footprint]
    a, edge1, edge2 = targets[:, 0], targets[:, 1] - targets[:, 0], targets[:, 2] - targets[:, 0]
    result = []
    for i in range(len(shell)):
        if not footprint[i]:
            result.append(False)
            continue
        direction = normals[i] / (np.linalg.norm(normals[i]) or 1)
        origin = centres[i] + direction * .03
        p = np.cross(direction, edge2)
        det = np.einsum("ij,ij->i", edge1, p)
        ok = np.abs(det) > 1e-9
        inv = np.where(ok, 1 / np.where(ok, det, 1), 0)
        tvec = origin - a
        u = np.einsum("ij,ij->i", tvec, p) * inv
        q = np.cross(tvec, edge1)
        v = (q @ direction) * inv
        dist = np.einsum("ij,ij->i", edge2, q) * inv
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (dist > .02) & (dist < 20)
        # The shell has no floor: inside the rooms, a face looking down looks at the floor.
        result.append(bool(hit.any()) or (direction[1] < -.2 and indoors(centres[i][0], centres[i][2])))
    return result


def load(path):
    data = path.read_bytes()
    length = struct.unpack_from("<I", data, 12)[0]
    document = json.loads(data[20:20 + length])
    offset = 20 + length
    blob = data[offset + 8:offset + 8 + struct.unpack_from("<I", data, offset)[0]]
    return document, blob


def read(document, blob, index):
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[accessor["type"]]
    fmt = {5126: "f", 5125: "I", 5123: "H", 5121: "B"}[accessor["componentType"]]
    size = struct.calcsize(fmt)
    stride = view.get("byteStride", size * width)
    out = []
    for i in range(accessor["count"]):
        values = struct.unpack_from("<%d%s" % (width, fmt), blob, start + i * stride)
        out.append(values if width > 1 else values[0])
    return out


def extract():
    document, blob = load(SOURCE)
    node = next(n for n in document["nodes"] if n.get("name") == NODE)
    parts = []
    for primitive in document["meshes"][node["mesh"]]["primitives"]:
        material = document["materials"][primitive["material"]]["name"]
        positions = read(document, blob, primitive["attributes"]["POSITION"])
        normals = read(document, blob, primitive["attributes"]["NORMAL"])
        indices = read(document, blob, primitive["indices"])
        parts.append((material, positions, normals, indices))
    every = [p for _, positions, _, _ in parts for p in positions]
    lo = [min(p[i] for p in every) for i in range(3)]
    hi = [max(p[i] for p in every) for i in range(3)]
    centre = ((lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2)
    return parts, centre


def write(parts, centre, scheme, target):
    colours = dict(COMMON, **SCHEMES[scheme])
    positions, normals, paint, indices = [], [], [], []
    shell = []  # (material, three local corners, normal, colour) for every kept triangle
    for material, part_positions, part_normals, part_indices in parts:
        colour = colours.get(material, TRIM)
        local = [(x - centre[0], y - centre[1], z - centre[2]) for x, y, z in part_positions]
        for t in range(0, len(part_indices), 3):
            corners = [local[i] for i in part_indices[t:t + 3]]
            if material == "Door" and sum(c[2] for c in corners) / 3 < FRONT_DOOR_Z:
                continue  # the front door: an open doorway
            shell.append((material, corners, part_normals[part_indices[t]], colour))
    inside = faces_inside(shell)
    shared = {}  # (position, normal, colour) -> vertex index
    for (material, corners, normal, colour), looks_in in zip(shell, inside):
        cy = sum(c[1] for c in corners) / 3
        cx = sum(c[0] for c in corners) / 3
        wall = GARAGE_WALL if cx < GARAGE_X and cy < CEILING_Y else PLASTER
        front = colour
        if material == "WoodPlanks" and looks_in:
            front = wall
        elif material in ("RooftilesSlate", "WoodPlanks") and normal[1] < -.2 and looks_in:
            front = CEILING
        # The shell was modelled to be seen from outside: give every face a back face too, painted
        # for the inside (plaster walls, a ceiling under the roof), so no wall is see-through.
        back = CEILING if material == "RooftilesSlate" else wall if material == "WoodPlanks" and not looks_in else front
        # Ground-floor walls are modelled with both faces; only the upper storey and roof need backs.
        sides = [(corners, normal, front)]
        if cy > CEILING_Y - .5 and abs(cx) < 7.4 and -6.3 < sum(c[2] for c in corners) / 3 < 10.2:
            sides.append((corners[::-1], tuple(-n for n in normal), back))
        for points, facing, paint_colour in sides:
            for point in points:
                key = (point, facing, paint_colour)
                if key not in shared:
                    shared[key] = len(positions)
                    positions.append(point)
                    normals.append(facing)
                    paint.append(paint_colour)
                indices.append(shared[key])
    for x0, x1, y0, y1, z0, z1, colour in INTERIOR:
        base = len(positions)
        box_positions, box_normals, box_indices = box(x0, x1, y0, y1, z0, z1)
        positions += box_positions
        normals += box_normals
        paint += [colour] * len(box_positions)
        indices += [base + i for i in box_indices]
    blob = bytearray()

    def view(data, kind):
        while len(blob) % 4:
            blob.append(0)
        offset = len(blob)
        blob.extend(data)
        return {"buffer": 0, "byteOffset": offset, "byteLength": len(data), "target": kind}

    views = [view(b"".join(struct.pack("<3f", *p) for p in positions), 34962),
             view(b"".join(struct.pack("<3f", *n) for n in normals), 34962),
             view(b"".join(struct.pack("<4B", *c) for c in paint), 34962),
             view(b"".join(struct.pack("<H", i) for i in indices), 34963)]
    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    document = {
        "asset": {"version": "2.0", "generator": "OUTLAND tools/extract_house.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"mesh": 0, "name": "verda_house_" + scheme}],
        "meshes": [{"name": "verda_house_" + scheme, "primitives": [{
            "attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2}, "indices": 3, "material": 0}]}],
        "materials": [{"name": "house_paint", "doubleSided": True, "pbrMetallicRoughness": {
            "baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": 1}}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5121, "normalized": True, "count": len(paint), "type": "VEC4"},
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
    if target.exists() and target.read_bytes() == out:
        return False
    target.write_bytes(out)
    return True


def main():
    check = "--check" in sys.argv
    parts, centre = extract()
    stale = []
    for scheme in SCHEMES:
        target = FOLDER / ("house_%s.glb" % scheme)
        if check:
            before = target.read_bytes() if target.exists() else b""
            write(parts, centre, scheme, target)
            if target.read_bytes() != before:
                stale.append(target.name)
                target.write_bytes(before) if before else target.unlink()
        elif write(parts, centre, scheme, target):
            print("wrote", target.relative_to(ROOT))
    if stale:
        print("stale:", ", ".join(stale))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
