#!/usr/bin/env python3
"""Pull one house out of the residential scene (assets/verda/residential/houses/source/house.glb).

The source is a whole neighbourhood (33 houses, a furnished interior, trees, fences, lamps:
255k vertices) and its textures did not survive export, so it cannot be placed as is. This takes
the one house node, bakes it to ground level centred on its footprint (front porch toward -Z),
paints each material with a flat colour and merges everything into a single vertex-coloured
primitive. Three paint schemes are written as assets/verda/residential/houses/house_<scheme>.glb.

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
    for material, part_positions, part_normals, part_indices in parts:
        base = len(positions)
        colour = colours.get(material, TRIM)
        positions += [(x - centre[0], y - centre[1], z - centre[2]) for x, y, z in part_positions]
        normals += part_normals
        paint += [colour] * len(part_positions)
        indices += [base + i for i in part_indices]
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
