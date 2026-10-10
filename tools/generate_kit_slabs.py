#!/usr/bin/env python3
"""Generate the Verda building-kit floor and roof slabs (textured GLB, standard library only).

The modular Building Parts walls are 3 m wide and 3 m tall; these slabs are 3 x 3 m so floors,
ceilings and roofs tile exactly with them. Units: metres, Y up, origin at the slab's bottom centre.

  floor_slab_3x3.glb  0.10 m: wooden planks on top, plaster ceiling below, concrete edges
  roof_slab_3x3.glb   0.10 m: tar-and-gravel on top, plaster ceiling below, concrete edges

Run: python3 tools/generate_kit_slabs.py [--check]
"""
import argparse
import json
from pathlib import Path
import random
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "verda" / "kit"
SIZE, THICK, TEX = 3.0, 0.10, 64


def png(width, height, pixels):
    raw = b"".join(b"\x00" + bytes(c for px in pixels[y * width:(y + 1) * width] for c in px) for y in range(height))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def texture(top):
    """128x64 atlas: left half = top surface, right top quarter = plaster, right bottom = concrete."""
    rng = random.Random(7 if top == "wood" else 11)
    w, h = TEX * 2, TEX
    pixels = []
    for y in range(h):
        for x in range(w):
            n = rng.randint(-9, 9)
            if x < TEX:
                if top == "wood":
                    plank = (y // 8) % 2
                    grain = (x * 3 + y * 7) % 11 - 5
                    seam = 35 if y % 8 == 0 or (x + plank * 23) % 48 == 0 else 0
                    base = (142, 104, 66) if plank else (128, 92, 58)
                    pixels.append(tuple(max(0, min(255, c + n + grain - seam)) for c in base))
                else:
                    gravel = rng.choice((0, 0, 0, 22, -18))
                    pixels.append(tuple(max(0, min(255, c + n + gravel)) for c in (58, 56, 54)))
            elif y < h // 2:
                pixels.append(tuple(max(0, min(255, c + n // 3)) for c in (226, 220, 206)))
            else:
                pixels.append(tuple(max(0, min(255, c + n)) for c in (150, 148, 142)))
    return png(w, h, pixels)


def slab(top):
    """Box with per-face UVs into the atlas. Returns positions, normals, uvs, indices."""
    h = SIZE / 2
    tops = (0.02, 0.02, 0.48, 0.98)      # u0, v0, u1, v1 into the top-surface half
    plaster = (0.52, 0.02, 0.98, 0.48)
    concrete = (0.52, 0.52, 0.98, 0.98)
    faces = [  # (corners counter-clockwise seen from outside, normal, uv rect)
        ([(-h, THICK, -h), (-h, THICK, h), (h, THICK, h), (h, THICK, -h)], (0, 1, 0), tops),
        ([(-h, 0, -h), (h, 0, -h), (h, 0, h), (-h, 0, h)], (0, -1, 0), plaster),
        ([(-h, 0, h), (h, 0, h), (h, THICK, h), (-h, THICK, h)], (0, 0, 1), concrete),
        ([(h, 0, -h), (-h, 0, -h), (-h, THICK, -h), (h, THICK, -h)], (0, 0, -1), concrete),
        ([(h, 0, h), (h, 0, -h), (h, THICK, -h), (h, THICK, h)], (1, 0, 0), concrete),
        ([(-h, 0, -h), (-h, 0, h), (-h, THICK, h), (-h, THICK, -h)], (-1, 0, 0), concrete),
    ]
    positions, normals, uvs, indices = [], [], [], []
    for corners, normal, (u0, v0, u1, v1) in faces:
        base = len(positions)
        positions += corners
        normals += [normal] * 4
        uvs += [(u0, v1), (u1, v1), (u1, v0), (u0, v0)]
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]
    return positions, normals, uvs, indices


def glb(name, top):
    positions, normals, uvs, indices = slab(top)
    image = texture(top)
    blobs = [
        b"".join(struct.pack("<3f", *p) for p in positions),
        b"".join(struct.pack("<3f", *n) for n in normals),
        b"".join(struct.pack("<2f", *t) for t in uvs),
        b"".join(struct.pack("<H", i) for i in indices),
        image,
    ]
    views, offset, binary = [], 0, b""
    for blob in blobs:
        pad = (-len(binary)) % 4
        binary += b"\x00" * pad
        offset = len(binary)
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(blob)})
        binary += blob
    binary += b"\x00" * ((-len(binary)) % 4)
    for view, target in zip(views[:4], (34962, 34962, 34962, 34963)):
        view["target"] = target
    lo = [min(p[i] for p in positions) for i in range(3)]
    hi = [max(p[i] for p in positions) for i in range(3)]
    document = {
        "asset": {"version": "2.0", "generator": "OUTLAND generate_kit_slabs.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                                   "indices": 3, "material": 0}]}],
        "materials": [{"name": "VerdaKit_" + top, "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                                         "metallicFactor": 0, "roughnessFactor": 1}}],
        "textures": [{"sampler": 0, "source": 0}], "samplers": [{"magFilter": 9728, "minFilter": 9728}],
        "images": [{"bufferView": 4, "mimeType": "image/png"}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": len(positions), "type": "VEC3", "min": lo, "max": hi},
            {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": len(uvs), "type": "VEC2"},
            {"bufferView": 3, "componentType": 5123, "count": len(indices), "type": "SCALAR"},
        ],
        "bufferViews": views, "buffers": [{"byteLength": len(binary)}],
    }
    text = json.dumps(document, separators=(",", ":")).encode()
    text += b" " * ((-len(text)) % 4)
    return (struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(text) + 8 + len(binary))
            + struct.pack("<I4s", len(text), b"JSON") + text + struct.pack("<I4s", len(binary), b"BIN\x00") + binary)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if committed slabs are stale")
    args = parser.parse_args()
    outputs = {OUT / "floor_slab_3x3.glb": glb("floor_slab_3x3", "wood"), OUT / "roof_slab_3x3.glb": glb("roof_slab_3x3", "tar")}
    stale = [p for p, data in outputs.items() if not p.exists() or p.read_bytes() != data]
    if args.check:
        for p in stale:
            print(f"stale: {p.relative_to(ROOT)}", file=sys.stderr)
        return 1 if stale else 0
    OUT.mkdir(parents=True, exist_ok=True)
    for p, data in outputs.items():
        p.write_bytes(data)
        print(f"wrote {p.relative_to(ROOT)} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
