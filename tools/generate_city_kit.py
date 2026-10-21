#!/usr/bin/env python3
"""Generate the Verda downtown city kit: textured GLB buildings and streetscape (standard library only).

Modelled on mid-sized American downtowns: a grid of ~80 m blocks with 12 m roadways and 4 m
sidewalks, a core of glass and stone towers around a courthouse square, and blocks of attached
2-3 storey brick "Main Street" buildings with storefronts, cornices and awnings.

  towers/*.glb      exterior shells (solid cover): office towers and mid-rises, 5 facade styles
  main_street/*.glb enterable: storefront and window openings (no glass), floors, stairs, roof
  ground/*.glb      68 m sidewalk block, courthouse-square park, parking lot, crosswalk intersection

Every texture is drawn procedurally here; walls tile with REPEAT so windows line up with floors.
Units: metres, Y up, front of a building faces -Z, origin at the ground centre.
Also writes include/outland/creator/CityAssetCatalog.hpp so the Creator catalog lists every piece.

Run: python3 tools/generate_city_kit.py [--check]
"""
import argparse
import json
import math
from pathlib import Path
import random
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "verda" / "city"
CATALOG = ROOT / "include" / "outland" / "creator" / "CityAssetCatalog.hpp"
FLOOR = 3.6          # storey height
BAY = 3.0            # facade bay width
TEX = 64


# ---------------------------------------------------------------- textures
def png(width, height, pixels):
    raw = b"".join(b"\x00" + bytes(c for px in pixels[y * width:(y + 1) * width] for c in px) for y in range(height))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def clamp(c):
    return max(0, min(255, int(c)))


def shade(color, amount):
    return tuple(clamp(c + amount) for c in color)


class Canvas:
    def __init__(self, w=TEX, h=TEX, color=(128, 128, 128), seed=1):
        self.w, self.h, self.rng = w, h, random.Random(seed)
        self.px = [color] * (w * h)

    def rect(self, x0, y0, x1, y1, color, noise=0):
        for y in range(max(0, y0), min(self.h, y1)):
            for x in range(max(0, x0), min(self.w, x1)):
                self.px[y * self.w + x] = shade(color, self.rng.randint(-noise, noise)) if noise else color

    def noise(self, amount):
        self.px = [shade(c, self.rng.randint(-amount, amount)) for c in self.px]

    def encode(self):
        return png(self.w, self.h, self.px)


def brick(base, mortar=(196, 188, 172), seed=3):
    c = Canvas(color=mortar, seed=seed)
    for row in range(0, TEX, 8):
        offset = 8 if (row // 8) % 2 else 0
        for col in range(-16, TEX, 16):
            tone = c.rng.randint(-14, 14)
            c.rect(col + offset + 1, row + 1, col + offset + 15, row + 7, shade(base, tone), 6)
    return c


def flat(color, noise=6, seed=5):
    c = Canvas(color=color, seed=seed)
    c.noise(noise)
    return c


def wood():
    c = Canvas(color=(132, 96, 60), seed=7)
    for y in range(TEX):
        plank = (y // 8) % 2
        for x in range(TEX):
            seam = 30 if y % 8 == 0 or (x + plank * 23) % 48 == 0 else 0
            c.px[y * TEX + x] = shade((140, 102, 64) if plank else (126, 90, 57), c.rng.randint(-8, 8) - seam)
    return c


def gravel_roof():
    c = Canvas(color=(84, 82, 78), seed=11)
    for i in range(len(c.px)):
        c.px[i] = shade(c.px[i], c.rng.choice((0, 0, 14, -12, 22)) + c.rng.randint(-6, 6))
    return c


def pavers():
    c = Canvas(color=(178, 174, 164), seed=13)
    c.noise(7)
    for i in range(0, TEX, 32):  # 1.5 m slabs on a 3 m tile
        c.rect(i, 0, i + 1, TEX, (138, 134, 126))
        c.rect(0, i, TEX, i + 1, (138, 134, 126))
    return c


def grass():
    c = Canvas(color=(92, 138, 62), seed=17)
    for i in range(len(c.px)):
        c.px[i] = shade(c.px[i], c.rng.randint(-16, 16))
    return c


def asphalt(lines=None):
    c = Canvas(color=(70, 72, 71), seed=19)
    c.noise(6)
    for (x0, y0, x1, y1, color) in lines or []:
        c.rect(x0, y0, x1, y1, color, 6)
    return c


def window_tile(wall, frame, glass, sill_ratio=.25, head_ratio=.85, width_ratio=.56, seed=23):
    """One bay x one storey: punched window in a wall (image top = top of the storey)."""
    c = Canvas(color=wall, seed=seed)
    c.noise(5)
    x0, x1 = int(TEX * (1 - width_ratio) / 2), int(TEX * (1 + width_ratio) / 2)
    y0, y1 = int(TEX * (1 - head_ratio)), int(TEX * (1 - sill_ratio))
    c.rect(x0 - 2, y0 - 2, x1 + 2, y1 + 3, frame)
    c.rect(x0, y0, x1, y1, glass, 4)
    c.rect(x0, y0 + (y1 - y0) // 3, x1, y0 + (y1 - y0) // 3 + 1, frame)  # transom
    c.rect((x0 + x1) // 2, y0, (x0 + x1) // 2 + 1, y1, frame)            # mullion
    c.rect(x0 - 4, y1 + 1, x1 + 4, y1 + 4, shade(wall, 30))               # stone sill
    return c


def curtain_wall(glass, mullion, spandrel, seed=29):
    """Glass curtain wall: full-height glazing, slim mullions, a spandrel band at each floor."""
    c = Canvas(color=glass, seed=seed)
    for y in range(TEX):
        for x in range(TEX):
            # Vertical sky reflection gradient across each pane.
            lift = int(26 * (1 - y / TEX)) + (8 if (x // 16) % 2 else 0)
            c.px[y * TEX + x] = shade(glass, lift + c.rng.randint(-3, 3))
    c.rect(0, TEX - 9, TEX, TEX, spandrel, 3)
    for x in (0, 31, 32, 63):
        c.rect(x, 0, x + 1, TEX, mullion)
    c.rect(0, 0, TEX, 1, mullion)
    return c


def ribbon(concrete, glass, seed=31):
    c = Canvas(color=concrete, seed=seed)
    c.noise(5)
    c.rect(0, 14, TEX, 44, glass, 3)
    for x in range(0, TEX, 16):
        c.rect(x, 14, x + 1, 44, shade(concrete, -40))
    return c


def storefront(frame, glass, sign, seed=37):
    """Ground-floor bay of a tower: sign band, tall glazing, kick plate."""
    c = Canvas(color=frame, seed=seed)
    c.rect(0, 0, TEX, 12, sign, 4)
    c.rect(3, 15, TEX - 3, 56, glass, 4)
    c.rect(TEX // 2, 15, TEX // 2 + 2, 56, frame)
    c.rect(0, 56, TEX, TEX, shade(frame, -20), 3)
    return c


def stripes(a, b, width=8, seed=41):
    c = Canvas(color=a, seed=seed)
    for x in range(0, TEX, width * 2):
        c.rect(x, 0, x + width, TEX, b, 4)
    c.noise(3)
    return c


def chevrons(base, line, seed=43):
    """Art deco spandrel: stacked chevrons in bronze, a fine border."""
    c = Canvas(color=base, seed=seed)
    c.noise(3)
    for y in range(TEX):
        for x in range(TEX):
            v = (y + abs(x - TEX // 2)) % 16
            if v < 3:
                c.px[y * TEX + x] = shade(line, c.rng.randint(-6, 6))
    c.rect(0, 0, TEX, 2, line)
    c.rect(0, TEX - 2, TEX, TEX, line)
    return c


def limestone(color, seed=47):
    """Dressed limestone: faint coursing every 0.6 m on a 1.2 m tile."""
    c = Canvas(color=color, seed=seed)
    c.noise(4)
    for y in (0, TEX // 2):
        c.rect(0, y, TEX, y + 1, shade(color, -22))
    for x, y0 in ((0, 0), (TEX // 2, TEX // 2)):
        c.rect(x, y0, x + 1, y0 + TEX // 2, shade(color, -18))
    return c


def checker(a, b, cells=4, seed=53):
    """Terrazzo / marble floor: a checkerboard of two stones with a brass strip between."""
    c = Canvas(color=a, seed=seed)
    size = TEX // cells
    for j in range(cells):
        for i in range(cells):
            if (i + j) % 2:
                c.rect(i * size, j * size, (i + 1) * size, (j + 1) * size, b, 5)
    c.noise(3)
    for k in range(0, TEX, size):
        c.rect(k, 0, k + 1, TEX, (176, 140, 70))
        c.rect(0, k, TEX, k + 1, (176, 140, 70))
    return c


# ---------------------------------------------------------------- mesh builder
class Builder:
    """Boxes with per-face materials. UVs are in tile units (REPEAT), so textures stay 1:1 with metres."""
    def __init__(self):
        self.materials = {}   # name -> (canvas, tile_u, tile_v)
        self.prims = {}       # name -> (positions, normals, uvs, indices)

    def material(self, name, canvas, tile_u=BAY, tile_v=FLOOR):
        self.materials.setdefault(name, (canvas, tile_u, tile_v))
        return name

    def quad(self, material, corners, normal, uv_axes):
        canvas, tu, tv = self.materials[material]
        positions, normals, uvs, indices = self.prims.setdefault(material, ([], [], [], []))
        base = len(positions)
        (ua, uo), (va, vo) = uv_axes  # (axis index, origin) for u and v; v grows downward
        for p in corners:
            positions.append(p)
            normals.append(normal)
            u = (p[ua] - uo) / tu if ua >= 0 else 0
            v = (vo - p[va]) / tv if va == 1 else (p[va] - vo) / tv
            uvs.append((u, v))
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]

    def box(self, lo, hi, faces, v_origin=None):
        """faces: dict side->material for sides '+x','-x','+y','-y','+z','-z' (missing sides skipped).
        Vertical faces use v measured down from v_origin (default: box top) so storeys line up."""
        x0, y0, z0 = lo
        x1, y1, z1 = hi
        top = hi[1] if v_origin is None else v_origin
        if '+y' in faces:
            self.quad(faces['+y'], [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], (0, 1, 0), ((0, 0), (2, 0)))
        if '-y' in faces:
            self.quad(faces['-y'], [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0), ((0, 0), (2, 0)))
        if '-z' in faces:
            self.quad(faces['-z'], [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1), ((0, x1 * 0), (1, top)))
        if '+z' in faces:
            self.quad(faces['+z'], [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1), ((0, 0), (1, top)))
        if '+x' in faces:
            self.quad(faces['+x'], [(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)], (1, 0, 0), ((2, 0), (1, top)))
        if '-x' in faces:
            self.quad(faces['-x'], [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)], (-1, 0, 0), ((2, 0), (1, top)))

    def solid(self, lo, hi, material, v_origin=None):
        self.box(lo, hi, {s: material for s in ('+x', '-x', '+y', '-y', '+z', '-z')}, v_origin)

    def raw(self, material, corners, normal, uvs):
        """One quad with explicit UVs (curved surfaces: columns, drums, domes, pediments)."""
        positions, normals, out_uvs, indices = self.prims.setdefault(material, ([], [], [], []))
        base = len(positions)
        positions += corners
        normals += [normal] * 4
        out_uvs += uvs
        indices += [base, base + 1, base + 2, base, base + 2, base + 3]

    def ring(self, material, cx, cz, r0, r1, y0, y1, sides=16, top_cap=False):
        """A vertical band of a round thing: a column or drum (r0 == r1), or a cone/dome course."""
        tu, tv = self.materials[material][1], self.materials[material][2]
        circumference = 2 * math.pi * max(r0, r1, .01)
        for k in range(sides):
            a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
            p = [(cx + r0 * math.sin(a0), y0, cz + r0 * math.cos(a0)), (cx + r0 * math.sin(a1), y0, cz + r0 * math.cos(a1)),
                 (cx + r1 * math.sin(a1), y1, cz + r1 * math.cos(a1)), (cx + r1 * math.sin(a0), y1, cz + r1 * math.cos(a0))]
            mid = (a0 + a1) / 2
            slope = (r0 - r1) / max(.001, abs(y1 - y0)) if y1 != y0 else 0
            n = (math.sin(mid), slope, math.cos(mid))
            length = math.sqrt(sum(c * c for c in n))
            n = tuple(c / length for c in n)
            u0, u1 = circumference * k / sides / tu, circumference * (k + 1) / sides / tu
            self.raw(material, p, n, [(u0, y0 / tv), (u1, y0 / tv), (u1, y1 / tv), (u0, y1 / tv)])
        if top_cap:
            for k in range(sides):
                a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
                p = [(cx, y1, cz), (cx, y1, cz), (cx + r1 * math.sin(a1), y1, cz + r1 * math.cos(a1)), (cx + r1 * math.sin(a0), y1, cz + r1 * math.cos(a0))]
                self.raw(material, p, (0, 1, 0), [(0, 0), (0, 0), (r1 / tu, 0), (0, r1 / tv)])

    def dome(self, material, cx, cz, r, y0, courses=7, sides=24, rise=1.0):
        """A hemispherical dome (rise scales its height) in stacked courses."""
        for c in range(courses):
            t0, t1 = math.pi / 2 * c / courses, math.pi / 2 * (c + 1) / courses
            self.ring(material, cx, cz, r * math.cos(t0), max(.001, r * math.cos(t1)),
                      y0 + rise * r * math.sin(t0), y0 + rise * r * math.sin(t1), sides)

    def pediment(self, material, x0, x1, y0, height, z, depth):
        """A triangular gable over a portico: front and back triangles and two roof slopes."""
        xm = (x0 + x1) / 2
        tu, tv = self.materials[material][1], self.materials[material][2]
        for zz, nz in ((z, -1), (z + depth, 1)):
            tri = [(x0, y0, zz), (x1, y0, zz), (xm, y0 + height, zz), (xm, y0 + height, zz)]
            if nz > 0:
                tri = [tri[1], tri[0], tri[2], tri[3]]
            self.raw(material, tri, (0, 0, nz), [(p[0] / tu, p[1] / tv) for p in tri])
        run = (x1 - x0) / 2
        length = math.hypot(run, height)
        for side in (-1, 1):
            edge = x0 if side < 0 else x1
            n = (side * height / length, run / length, 0)
            quad = [(edge, y0, z), (xm, y0 + height, z), (xm, y0 + height, z + depth), (edge, y0, z + depth)]
            if side > 0:
                quad = [quad[1], quad[0], quad[3], quad[2]]
            self.raw(material, quad, n, [(0, 0), (length / tu, 0), (length / tu, depth / tv), (0, depth / tv)])

    def glb(self, name):
        names = [m for m in self.materials if m in self.prims]
        blobs, views, accessors, primitives, images, textures, materials = [], [], [], [], [], [], []
        def add(blob, target=None):
            views.append({"buffer": 0, "byteLength": len(blob), **({"target": target} if target else {})})
            blobs.append(blob)
            return len(views) - 1
        for index, m in enumerate(names):
            positions, normals, uvs, indices = self.prims[m]
            assert len(positions) < 65536, (name, m)
            lo = [min(p[i] for p in positions) for i in range(3)]
            hi = [max(p[i] for p in positions) for i in range(3)]
            a = len(accessors)
            accessors += [
                {"bufferView": add(b"".join(struct.pack("<3f", *p) for p in positions), 34962), "componentType": 5126,
                 "count": len(positions), "type": "VEC3", "min": lo, "max": hi},
                {"bufferView": add(b"".join(struct.pack("<3f", *n) for n in normals), 34962), "componentType": 5126,
                 "count": len(normals), "type": "VEC3"},
                {"bufferView": add(b"".join(struct.pack("<2f", *t) for t in uvs), 34962), "componentType": 5126,
                 "count": len(uvs), "type": "VEC2"},
                {"bufferView": add(b"".join(struct.pack("<H", i) for i in indices), 34963), "componentType": 5123,
                 "count": len(indices), "type": "SCALAR"},
            ]
            images.append({"bufferView": add(self.materials[m][0].encode()), "mimeType": "image/png"})
            textures.append({"sampler": 0, "source": index})
            materials.append({"name": m, "pbrMetallicRoughness": {"baseColorTexture": {"index": index},
                                                                   "metallicFactor": 0, "roughnessFactor": 1}})
            primitives.append({"attributes": {"POSITION": a, "NORMAL": a + 1, "TEXCOORD_0": a + 2},
                               "indices": a + 3, "material": index})
        binary = b""
        for view, blob in zip(views, blobs):
            binary += b"\x00" * ((-len(binary)) % 4)
            view["byteOffset"] = len(binary)
            binary += blob
        binary += b"\x00" * ((-len(binary)) % 4)
        document = {
            "asset": {"version": "2.0", "generator": "OUTLAND generate_city_kit.py"},
            "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": name, "mesh": 0}],
            "meshes": [{"name": name, "primitives": primitives}], "materials": materials, "textures": textures,
            "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
            "images": images, "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(binary)}],
        }
        text = json.dumps(document, separators=(",", ":")).encode()
        text += b" " * ((-len(text)) % 4)
        return (struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(text) + 8 + len(binary))
                + struct.pack("<I4s", len(text), b"JSON") + text + struct.pack("<I4s", len(binary), b"BIN\x00") + binary)


ALL_SIDES = ('+x', '-x', '+y', '-y', '+z', '-z')


def wall_with_openings(b, axis, fixed, span, height, thickness, openings, outer, inner, outward, base=0.0):
    """A wall along `axis` ('x' or 'z') at coordinate `fixed`, from span[0] to span[1] and base..height,
    split into boxes around rectangular openings [(a0, a1, y0, y1)]. `outward` is +1/-1 (outer face)."""
    cuts = sorted({span[0], span[1], *[c for o in openings for c in (o[0], o[1])]})
    t0, t1 = (fixed, fixed + thickness) if outward < 0 else (fixed - thickness, fixed)
    for a0, a1 in zip(cuts, cuts[1:]):
        if a1 - a0 < 1e-4:
            continue
        holes = sorted((o[2], o[3]) for o in openings if o[0] <= a0 + 1e-4 and o[1] >= a1 - 1e-4)
        y = base
        for h0, h1 in holes + [(height, height)]:
            if h0 - y > 1e-4:
                lo = (a0, y, t0) if axis == 'x' else (t0, y, a0)
                hi = (a1, h0, t1) if axis == 'x' else (t1, h0, a1)
                out_face = ('-z' if outward < 0 else '+z') if axis == 'x' else ('-x' if outward < 0 else '+x')
                in_face = ('+z' if outward < 0 else '-z') if axis == 'x' else ('+x' if outward < 0 else '-x')
                faces = {s: outer for s in ALL_SIDES}
                faces[in_face] = inner
                b.box(lo, hi, faces, v_origin=height)
            y = max(y, h1)


# ---------------------------------------------------------------- buildings
def tower(style, width, depth, floors, setback=0, setback_at=0, crown=True):
    styles = {
        "glass_blue": (curtain_wall((70, 116, 148), (46, 52, 58), (38, 44, 52)), (52, 58, 66)),
        "glass_dark": (curtain_wall((44, 52, 60), (24, 26, 28), (30, 32, 34), seed=43), (34, 36, 38)),
        "stone": (window_tile((196, 182, 150), (120, 104, 82), (60, 78, 96), width_ratio=.5), (210, 198, 168)),
        "brick": (window_tile((150, 68, 50), (232, 226, 210), (54, 70, 84)), (226, 218, 198)),
        "concrete": (ribbon((168, 166, 160), (58, 74, 90)), (186, 184, 178)),
    }
    facade, trim_color = styles[style]
    b = Builder()
    b.material("facade", facade)
    b.material("storefront", storefront((40, 42, 44), (70, 92, 104), shade(trim_color, -30)), BAY, 4.5)
    b.material("trim", flat(trim_color, 5), BAY, 1)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("mech", flat((150, 150, 146), 4), 3, 3)
    w, d = width / 2, depth / 2
    ground = 4.5
    top = ground + floors * FLOOR
    b.box((-w, 0, -d), (w, ground, d), {s: "storefront" for s in ('+x', '-x', '+z', '-z')}, v_origin=ground)
    b.solid((-w - .25, ground, -d - .25), (w + .25, ground + .5, d + .25), "trim")  # podium cornice
    if setback and setback_at:
        mid = ground + setback_at * FLOOR
        b.box((-w, ground + .5, -d), (w, mid, d), {s: "facade" for s in ('+x', '-x', '+z', '-z')}, v_origin=mid)
        b.box((-w, mid, -d), (w, mid + .4, d), {'+y': "roof", '+x': "trim", '-x': "trim", '+z': "trim", '-z': "trim"})
        w2, d2 = w - setback, d - setback
        b.box((-w2, mid + .4, -d2), (w2, top, d2), {s: "facade" for s in ('+x', '-x', '+z', '-z')}, v_origin=top)
        w, d = w2, d2
    else:
        b.box((-w, ground + .5, -d), (w, top, d), {s: "facade" for s in ('+x', '-x', '+z', '-z')}, v_origin=top)
    # Parapet, roof and rooftop mechanical penthouse.
    b.box((-w, top, -d), (w, top + .05, d), {'+y': "roof"})
    for lo, hi in (((-w - .2, top, -d - .2), (w + .2, top + 1.0, -d + .2)), ((-w - .2, top, d - .2), (w + .2, top + 1.0, d + .2)),
                   ((-w - .2, top, -d), (-w + .2, top + 1.0, d)), ((w - .2, top, -d), (w + .2, top + 1.0, d))):
        b.solid(lo, hi, "trim")
    if crown:
        b.solid((-w * .45, top, -d * .4), (w * .35, top + 3.2, d * .3), "mech")
    return b


def main_street(width, depth, storeys, brick_color, trim_color, awning, seed):
    """Enterable attached commercial building. Front (storefront) faces -Z; party walls on +-X."""
    b = Builder()
    rng = random.Random(seed)
    b.material("brick", brick(brick_color, seed=seed), 1.2, 1.2)
    # Interiors are kept dim so openings read as windows from the street.
    b.material("plaster", flat((128, 120, 108), 4), 3, 3)
    b.material("trim", flat(trim_color, 5), 3, 1)
    b.material("floor", wood(), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("frame", flat((44, 46, 48), 3), 3, 3)
    if awning:
        b.material("awning", stripes(awning, (236, 230, 216)), 1.5, 1.5)
    w, d = width / 2, depth / 2
    t = .3
    slab = .2
    height = storeys * FLOOR + .9  # parapet
    bays = int(round(width / BAY))
    # Front: ground-floor storefront (doors walkable, display windows on a low sill), upper windows.
    front, back, lintels = [], [], []
    for i in range(bays):
        x0 = -w + i * width / bays
        x1 = x0 + width / bays
        door = i % 2 == (bays // 2) % 2
        front.append((x0 + .35, x1 - .35, slab, 3.0) if door else (x0 + .35, x1 - .35, .75, 3.0))
        for s in range(1, storeys):
            base = s * FLOOR
            front.append(((x0 + x1) / 2 - .65, (x0 + x1) / 2 + .65, base + .95, base + 2.75))
            lintels.append(((x0 + x1) / 2, base))
            if rng.random() < .7:
                back.append(((x0 + x1) / 2 - .6, (x0 + x1) / 2 + .6, base + .95, base + 2.6))
    back.append((-w + 1.2, -w + 2.6, slab, 2.6))  # rear door to the alley / parking
    wall_with_openings(b, 'x', -d, (-w, w), height, t, front, "brick", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), height, t, back, "brick", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), height, t, [], "brick", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), height, t, [], "brick", "plaster", +1)
    # Switchback stairs along the +X party wall: flight s climbs from floor s to s+1 in lane s % 2,
    # alternating direction, so every flight has open headroom (the next flight is in the other lane).
    # 0.2 m risers, 0.28 m treads; each floor has a hole over the flight that arrives at it.
    steps = int(round(FLOOR / .2))
    run = steps * .28
    sz0 = -d + 2.5
    lane = lambda s: (w - t - 1.3 * (1 + s % 2), w - t - 1.3 * (s % 2))
    for s in range(storeys + 1):
        y = s * FLOOR
        faces = {'+y': "roof" if s == storeys else "floor", '-y': "plaster", '+x': "plaster", '-x': "plaster", '+z': "plaster", '-z': "plaster"}
        if s == 0:
            b.box((-w + t, 0, -d + t), (w - t, slab, d - t), {'+y': "floor", '-y': "plaster"})
            continue
        if s == storeys:
            b.box((-w + t, y, -d + t), (w - t, y + slab, d - t), {'+y': "roof", '-y': "plaster"})
            continue
        hx0, hx1 = lane(s - 1)
        hz0, hz1 = sz0, sz0 + run
        for lo, hi in (((-w + t, y, -d + t), (hx0, y + slab, d - t)), ((hx1, y, -d + t), (w - t, y + slab, d - t)),
                       ((hx0, y, -d + t), (hx1, y + slab, hz0)), ((hx0, y, hz1), (hx1, y + slab, d - t))):
            if hi[0] - lo[0] > 1e-3 and hi[2] - lo[2] > 1e-3:
                b.box(lo, hi, faces)
    for s in range(storeys - 1):
        base = s * FLOOR + slab
        x0, x1 = lane(s)
        for k in range(steps):
            z0 = sz0 + k * .28 if s % 2 == 0 else sz0 + run - (k + 1) * .28
            b.solid((x0, base, z0), (x1, base + (k + 1) * .2 - (slab if k == steps - 1 else 0), z0 + .28), "frame")
    # Stone lintels and sills frame every upper window.
    for cx, base in lintels:
        b.solid((cx - .8, base + 2.75, -d - .08), (cx + .8, base + 2.98, -d), "trim")
        b.solid((cx - .75, base + .83, -d - .1), (cx + .75, base + .95, -d), "trim")
    # Facade dressing: storefront frame band, sign band, cornice, awning over the doors.
    b.solid((-w, 3.0, -d - .12), (w, 3.55, -d), "trim")
    b.solid((-w - .1, height - .55, -d - .45), (w + .1, height, -d), "trim")
    b.solid((-w, storeys * FLOOR - .2, -d - .2), (w, storeys * FLOOR, -d), "trim")
    b.solid((-w + .2, 0, -d - .1), (-w + .45, 3.0, -d), "frame")
    b.solid((w - .45, 0, -d - .1), (w - .2, 3.0, -d), "frame")
    if awning:
        b.solid((-w + .5, 3.0, -d - 1.6), (w - .5, 3.12, -d), "awning")
    return b


# ---------------------------------------------------------------- Stela Turo
STELA_FLOORS = 17
STELA_TIERS = [  # (first floor, end floor, half width, half depth): the art deco setbacks
    (0, 11, 15.0, 12.0),
    (11, 15, 12.5, 10.0),
    (15, 17, 10.5, 8.5),
]
STELA_STAIR_LANE = 1.3          # each stair core is two 1.3 m lanes (switchback flights)
STELA_STAIR_Z0 = -6.5           # flights run along Z from here (18 risers of 0.2 m, 0.28 m treads)
STELA_SLAB = .25
STELA_WALL = .35


def stela_tier(floor):
    for first, end, hw, hd in STELA_TIERS:
        if first <= floor < end:
            return first, end, hw, hd
    return STELA_TIERS[-1]


def slab_with_holes(b, x0, x1, z0, z1, y, thick, holes, top, bottom):
    """A floor slab over the rectangle, cut round rectangular holes [(x0, x1, z0, z1)]."""
    xs = sorted({x0, x1, *[c for h in holes for c in (h[0], h[1]) if x0 < c < x1]})
    zs = sorted({z0, z1, *[c for h in holes for c in (h[2], h[3]) if z0 < c < z1]})
    for a0, a1 in zip(xs, xs[1:]):
        for c0, c1 in zip(zs, zs[1:]):
            mx, mz = (a0 + a1) / 2, (c0 + c1) / 2
            if any(h[0] < mx < h[1] and h[2] < mz < h[3] for h in holes):
                continue
            b.box((a0, y, c0), (a1, y + thick, c1), {'+y': top, '-y': bottom, '+x': bottom, '-x': bottom, '+z': bottom, '-z': bottom})


def stela_lane(core, flight):
    """X span of the lane flight `flight` climbs in, in stair core 'L' or 'R' (inside the top tier)."""
    inner = STELA_TIERS[-1][2] - STELA_WALL
    k = flight % 2
    if core == 'R':
        return inner - STELA_STAIR_LANE * (1 + k), inner - STELA_STAIR_LANE * k
    return -inner + STELA_STAIR_LANE * k, -inner + STELA_STAIR_LANE * (1 + k)


def stela_flights(core):
    """Flights in a core: flight s climbs from floor s to floor s + 1. The right core goes on up to
    the roof from the penthouse; the left one stops at the penthouse."""
    return range(STELA_FLOORS if core == 'R' else STELA_FLOORS - 1)


def stela_turo():
    """Stela Turo, Verda City's art deco skyscraper (front faces -Z). Seventeen storeys of dressed
    limestone in three setbacks (30 x 24 m to the 11th floor, 25 x 20 m to the 15th, 21 x 17 m to the
    top), piers running up between the windows, bronze chevron spandrels, gold trim, a stepped crown
    and a gold spire (top 86 m). Every window is open (no glass). Every floor is open plan round a
    few columns, with two switchback stair cores, one at each side; the left core climbs to the
    penthouse (the 17th floor: a furnished suite on a marble floor), the right one on up to a stair
    house on the roof. The setback roofs are terraces, reached through doors on the 12th and 16th
    floors."""
    b = Builder()
    stone = limestone((228, 216, 186))
    for tier in ("a", "b", "c", "crown"):
        b.material("stone_" + tier, stone, 1.2, 1.2)
    b.material("granite", flat((52, 50, 54), 4), 3, 3)
    b.material("bronze", chevrons((84, 66, 44), (196, 156, 72)), 2.5, 1.5)
    b.material("gold", flat((214, 172, 74), 3), 1, 1)
    b.material("sill", flat((150, 118, 62), 4), 1, 1)
    # Interiors kept dimmer than the stone so the open windows read as windows from the street.
    b.material("plaster", flat((132, 124, 110), 3), 3, 3)
    b.material("partition", flat((214, 206, 188), 3), 3, 3)
    b.material("terrazzo", checker((196, 190, 176), (150, 144, 132)), 2.4, 2.4)
    b.material("marble", checker((236, 232, 224), (34, 34, 38), 4), 1.6, 1.6)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("frame", flat((44, 46, 48), 3), 3, 3)
    b.material("wood", wood(), 3, 3)
    b.material("velvet", flat((26, 96, 92), 6), 1, 1)
    b.material("linen", flat((232, 228, 216), 3), 1, 1)
    b.material("black", flat((22, 22, 24), 2), 1, 1)
    b.material("white", flat((242, 242, 238), 2), 1, 1)
    b.material("rug", stripes((120, 30, 40), (196, 156, 72), 4), 1, 1)
    t, slab = STELA_WALL, STELA_SLAB
    roof_y = STELA_FLOORS * FLOOR
    bay = 2.5
    # ---- exterior walls, tier by tier, with every window open
    for index, (first, end, hw, hd) in enumerate(STELA_TIERS):
        mat = "stone_" + "abc"[index]
        y0, y1 = first * FLOOR, end * FLOOR
        for axis, fixed, half, outward in (('x', -hd, hw, -1), ('x', hd, hw, 1), ('z', -hw, hd, -1), ('z', hw, hd, 1)):
            span = (-half, half) if axis == 'x' else (-half + t, half - t)
            bays = max(1, int(round((span[1] - span[0]) / bay)))
            width = (span[1] - span[0]) / bays
            openings = []
            for i in range(bays):
                a0 = span[0] + i * width + .55
                a1 = span[0] + (i + 1) * width - .55
                centre = (a0 + a1) / 2
                for floor in range(first, end):
                    base = floor * FLOOR + slab
                    if floor == 0:
                        # The lobby: tall doors in the middle of the front and back, shop windows elsewhere.
                        door = abs(centre) < width * .8 and axis == 'x'
                        openings.append((a0, a1, slab, 3.2) if door else (a0, a1, .9, 3.2))
                    elif floor == first and axis == 'x' and outward < 0 and abs(centre) < width * 1.2 and first > 0:
                        openings.append((a0, a1, base, base + 2.6))   # french doors onto the terrace
                    elif floor == STELA_FLOORS - 1:
                        openings.append((a0, a1, base + .45, base + 3.05))  # the penthouse: tall windows
                    else:
                        openings.append((a0, a1, base + .85, base + 2.85))
            wall_with_openings(b, axis, fixed, span, y1, t, openings, mat, "plaster", outward, base=y0)
            # A gold-edged bronze sill under every window, so the openings read from the street.
            for a0, a1, o0, o1 in openings:
                if o0 < .5:
                    continue
                if axis == 'x':
                    z0, z1 = (fixed - .14, fixed + .02) if outward < 0 else (fixed - .02, fixed + .14)
                    b.solid((a0 - .1, o0 - .14, z0), (a1 + .1, o0, z1), "sill")
                else:
                    x0, x1 = (fixed - .14, fixed + .02) if outward < 0 else (fixed - .02, fixed + .14)
                    b.solid((x0, o0 - .14, a0 - .1), (x1, o0, a1 + .1), "sill")
            # Bronze chevron spandrels between the windows of one floor and the next; piers between bays.
            for i in range(bays):
                a0 = span[0] + i * width + .55
                a1 = span[0] + (i + 1) * width - .55
                for floor in range(max(first, 1), end - 1):
                    lo_y, hi_y = floor * FLOOR + slab + 2.95, (floor + 1) * FLOOR + slab + .75
                    if axis == 'x':
                        z = fixed + outward * .02
                        b.box((a0 + .1, lo_y, min(z, fixed)), (a1 - .1, hi_y, max(z, fixed)), {'+z' if outward > 0 else '-z': "bronze"})
                    else:
                        x = fixed + outward * .02
                        b.box((min(x, fixed), lo_y, a0 + .1), (max(x, fixed), hi_y, a1 - .1), {'+x' if outward > 0 else '-x': "bronze"})
            for i in range(bays + 1):
                a = span[0] + i * width
                top_extra = 1.6 if i % 2 == 0 else .9     # piers rise past the parapet, alternate heights
                bottom = max(y0, 4.0)
                if axis == 'x':
                    lo = (a - .3, bottom, fixed - .22 if outward < 0 else fixed)
                    hi = (a + .3, y1 + top_extra, fixed if outward < 0 else fixed + .22)
                else:
                    lo = (fixed - .22 if outward < 0 else fixed, bottom, a - .3)
                    hi = (fixed if outward < 0 else fixed + .22, y1 + top_extra, a + .3)
                b.solid(lo, hi, mat)
                cap_lo, cap_hi = list(lo), list(hi)
                cap_lo[1], cap_hi[1] = hi[1] - .25, hi[1]
                b.solid(tuple(cap_lo), tuple(cap_hi), "gold")
        # Parapet round the roof of this tier (a terrace, or the roof itself on the top tier).
        py = y1
        for lo, hi in (((-hw, py, -hd), (hw, py + 1.1, -hd + .25)), ((-hw, py, hd - .25), (hw, py + 1.1, hd)),
                       ((-hw, py, -hd), (-hw + .25, py + 1.1, hd)), ((hw - .25, py, -hd), (hw, py + 1.1, hd))):
            b.solid(lo, hi, mat)
        b.solid((-hw - .15, py + 1.1, -hd - .15), (hw + .15, py + 1.25, -hd + .25), "gold")
    # ---- the base: a dark granite plinth and a gold entrance with a sunburst
    hw0, hd0 = STELA_TIERS[0][2], STELA_TIERS[0][3]
    for lo, hi in (((-hw0 - .1, 0, -hd0 - .1), (hw0 + .1, .9, -hd0)), ((-hw0 - .1, 0, hd0), (hw0 + .1, .9, hd0 + .1)),
                   ((-hw0 - .1, 0, -hd0), (-hw0, .9, hd0)), ((hw0, 0, -hd0), (hw0 + .1, .9, hd0))):
        b.solid(lo, hi, "granite")
    # The entrances, front and back alike (the model stays symmetric, so its origin is its centre).
    for face in (-1, 1):
        wall = face * hd0
        out = face * .35
        b.solid((-3.4, 3.2, min(wall, wall + out)), (3.4, 3.6, max(wall, wall + out)), "gold")          # lintel
        for side in (-1, 1):
            b.solid((side * 3.4 - .25, 0, min(wall, wall + out)), (side * 3.4 + .25, 3.6, max(wall, wall + out)), "gold")
        z = wall + face * .36
        for k in range(9):                                                        # sunburst rays
            a0, a1 = math.pi * (k + .15) / 9, math.pi * (k + .85) / 9
            p = [(0, 3.6, z), (0, 3.6, z), (-math.cos(a1) * 3.0, 3.6 + math.sin(a1) * 2.2, z), (-math.cos(a0) * 3.0, 3.6 + math.sin(a0) * 2.2, z)]
            if face > 0:
                p = [p[1], p[0], p[3], p[2]]
            b.raw("gold", p, (0, 0, face), [(0, 0), (0, 0), (1, 1), (0, 1)])
        b.solid((-4.2, 3.9, min(wall, wall + face * 2.6)), (4.2, 4.15, max(wall, wall + face * 2.6)), "bronze")  # marquee canopy
    # ---- floors with the stair holes; columns
    for level in range(STELA_FLOORS + 1):
        y = level * FLOOR
        _, _, hw, hd = stela_tier(level - 1 if level > 0 else 0)
        holes = []
        for core in ('L', 'R'):
            if level >= 1 and level - 1 in stela_flights(core):
                x0, x1 = stela_lane(core, level - 1)
                holes.append((x0, x1, STELA_STAIR_Z0, STELA_STAIR_Z0 + 18 * .28))
        top = "roof" if level == STELA_FLOORS else "marble" if level == STELA_FLOORS - 1 else "terrazzo"
        inset = 0 if level in {first for first, *_ in STELA_TIERS[1:]} or level == STELA_FLOORS else t
        slab_with_holes(b, -hw + inset, hw - inset, -hd + inset, hd - inset, y, slab, holes, top, "plaster")
    for floor in range(STELA_FLOORS - 1):
        for x in (-4.5, 4.5):
            for z in (-3.5, 3.5):
                b.solid((x - .35, floor * FLOOR + slab, z - .35), (x + .35, (floor + 1) * FLOOR, z + .35), "plaster")
    # ---- the stairs
    steps, tread = 18, .28
    for core in ('L', 'R'):
        for s in stela_flights(core):
            base = s * FLOOR + slab
            x0, x1 = stela_lane(core, s)
            for k in range(steps):
                z0 = STELA_STAIR_Z0 + k * tread if s % 2 == 0 else STELA_STAIR_Z0 + (steps - k - 1) * tread
                top_y = base + (k + 1) * .2 - (slab if k == steps - 1 else 0)
                b.solid((x0, top_y - .2 if k else base - .01, z0), (x1, top_y, z0 + tread), "frame")
    # Bronze guard rails round each floor's stair hole: along the core's inner side, and across the
    # end of the hole you do not step off at.
    for level in range(1, STELA_FLOORS + 1):
        y = level * FLOOR + slab
        for core in ('L', 'R'):
            if level - 1 not in stela_flights(core):
                continue
            x0, x1 = stela_lane(core, level - 1)
            lanes = stela_lane(core, level - 1), stela_lane(core, level)
            inner_x = min(l[0] for l in lanes) if core == 'R' else max(l[1] for l in lanes)
            zs0, zs1 = STELA_STAIR_Z0, STELA_STAIR_Z0 + steps * tread
            rx = (inner_x - .06, inner_x) if core == 'R' else (inner_x, inner_x + .06)
            b.solid((rx[0], y, zs0), (rx[1], y + 1.0, zs1), "bronze")
            end = zs0 if (level - 1) % 2 == 0 else zs1      # an even flight arrives at the +Z end
            b.solid((x0, y, end - .03), (x1, y + 1.0, end + .03), "bronze")
    # ---- the roof: stair house over the right core, a stepped crown and the spire
    rx0, rx1 = stela_lane('R', STELA_FLOORS - 1)
    zs0, zs1 = STELA_STAIR_Z0, STELA_STAIR_Z0 + steps * tread
    house = [((rx0 - .3, roof_y, zs0 - .3), (rx0, roof_y + 2.8, zs1 + .3)),       # inner wall
             ((rx1, roof_y, zs0 - .3), (rx1 + .3, roof_y + 2.8, zs1 + .3)),       # outer wall
             ((rx0 - .3, roof_y, zs0 - .3), (rx1 + .3, roof_y + 2.8, zs0))]       # end wall; the door faces +Z
    for lo, hi in house:
        b.solid(lo, hi, "stone_crown")
    b.solid((rx0 - .4, roof_y + 2.8, zs0 - .4), (rx1 + .4, roof_y + 3.1, zs1 + .4), "gold")
    crown = [((-6.5, 0.5, 7.6), 6.0), ((-4.5, 1.5, 6.6), 5.0), ((-2.8, 2.4, 5.6), 4.5)]
    y = roof_y
    for (hx, z0, z1), height in crown:
        b.box((hx, y, z0), (-hx, y + height, z1), {s: "stone_crown" for s in ALL_SIDES})
        for i in range(5):                         # gold fins up each step's face
            x = hx + (-2 * hx) * (i + .5) / 5
            b.solid((x - .12, y + .4, z0 - .25), (x + .12, y + height + .4, z0), "gold")
        b.solid((hx - .1, y + height - .3, z0 - .1), (-hx + .1, y + height, z1 + .1), "gold")
        y += height
    cz = (crown[-1][0][1] + crown[-1][0][2]) / 2
    b.ring("gold", 0, cz, .9, .08, y, y + 9.5, 8)                              # the spire
    b.ring("gold", 0, cz, 1.4, 1.4, y, y + .6, 12, top_cap=True)
    # ---- the penthouse (17th floor): a suite on a marble floor
    py = (STELA_FLOORS - 1) * FLOOR + slab
    hwc, hdc = STELA_TIERS[-1][2] - t, STELA_TIERS[-1][3] - t
    # Bedroom and bath at the back, behind a wall with two doorways; the living room in front.
    wall_with_openings(b, 'x', 1.6, (-7.4, 7.4), py + 3.1, .15, [(-5.8, -4.6, py, py + 2.3), (2.6, 3.8, py, py + 2.3)],
                       "partition", "partition", -1, base=py)
    wall_with_openings(b, 'z', .6, (1.6, hdc), py + 3.1, .15, [], "partition", "partition", 1, base=py)
    b.solid((-6.6, py, 4.2), (-2.4, py + .55, 6.9), "linen")                    # bed
    b.solid((-6.8, py, 6.9), (-2.2, py + 1.5, 7.2), "gold")                     # headboard
    b.solid((-7.2, py, 5.0), (-6.7, py + .55, 6.0), "wood")                     # night tables
    b.solid((-2.3, py, 5.0), (-1.8, py + .55, 6.0), "wood")
    b.solid((2.0, py, 6.4), (5.6, py + .6, 7.9), "white")                       # bathtub
    b.solid((5.0, py, 2.2), (6.8, py + .9, 3.0), "white")                       # basin counter
    b.solid((-6.0, py, -6.0), (-1.0, py + .02, -1.5), "rug")                    # living room: rug,
    b.solid((-6.0, py, -6.6), (-1.0, py + .8, -5.8), "velvet")                  # sofa,
    b.solid((-6.6, py, -6.6), (-5.8, py + .8, -2.0), "velvet")
    b.solid((-4.2, py, -4.4), (-2.6, py + .42, -3.2), "black")                  # coffee table,
    b.solid((1.5, py, -4.8), (4.7, py + .75, -3.4), "wood")                     # dining table
    for x in (2.0, 3.0, 4.0):
        for z in (-5.4, -2.8):
            b.solid((x - .22, py, z - .22), (x + .22, py + .45, z + .22), "velvet")
    b.solid((-1.0, py, .3), (3.8, py + .95, 1.0), "granite")                    # kitchen bar
    b.solid((4.6, py, -1.6), (6.4, py + .95, -.2), "black")                     # grand piano
    b.solid((4.6, py + .95, -1.6), (6.4, py + 1.0, .5), "black")
    return b


# ---------------------------------------------------------------- civic buildings
def police_station():
    """Modular Policio station (one storey, enterable): white render over a navy base, a glass
    entrance with a blue lamp, a public counter and two barred cells inside, a radio mast, and a
    carport on the +X side for the patrol car. Front faces -Z; 14 x 12 m plus the 6 m carport."""
    b = Builder()
    b.material("render", flat((232, 232, 228), 4), 3, 3)
    b.material("navy", flat((26, 40, 92), 4), 3, 1)
    b.material("plaster", flat((150, 154, 160), 4), 3, 3)
    b.material("floor", flat((120, 124, 128), 6), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("frame", flat((44, 46, 48), 3), 3, 3)
    b.material("steel", flat((120, 124, 130), 3), 3, 3)
    b.material("lamp", flat((70, 130, 255), 0), 1, 1)
    w, d, height, t, slab = 7.0, 6.0, 4.2, .3, .2
    # Front: double doors in the middle, a window either side; side and back windows.
    front = [(-1.1, 1.1, slab, 2.6), (-5.6, -3.2, 1.0, 2.6), (3.2, 5.6, 1.0, 2.6)]
    back = [(-5.5, -3.9, 1.6, 2.6), (3.9, 5.5, 1.6, 2.6), (-.5, .5, slab, 2.4)]
    sides = [(-3.5, -1.5, 1.0, 2.6), (1.5, 3.5, 1.0, 2.6)]
    wall_with_openings(b, 'x', -d, (-w, w), height, t, front, "render", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), height, t, back, "render", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), height, t, sides, "render", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), height, t, sides, "render", "plaster", +1)
    b.box((-w + t, 0, -d + t), (w - t, slab, d - t), {'+y': "floor", '-y': "plaster"})
    b.box((-w, height - .2, -d), (w, height, d), {'+y': "roof", '-y': "plaster", '+x': "navy", '-x': "navy", '+z': "navy", '-z': "navy"})
    # Navy plinth and a navy fascia band (the sign goes on it), parapet.
    b.solid((-w - .05, 0, -d - .05), (w + .05, .6, d + .05), "navy")
    b.solid((-w - .1, height - .9, -d - .15), (w + .1, height - .2, -d), "navy")
    for lo, hi in (((-w, height, -d), (w, height + .5, -d + .2)), ((-w, height, d - .2), (w, height + .5, d)),
                   ((-w, height, -d), (-w + .2, height + .5, d)), ((w - .2, height, -d), (w, height + .5, d))):
        b.solid(lo, hi, "render")
    # The blue lamp over the door, a canopy, and the radio mast.
    b.solid((-1.6, 2.75, -d - 1.2), (1.6, 2.9, -d), "navy")
    b.solid((-.18, 2.95, -d - .5), (.18, 3.3, -d - .14), "lamp")
    b.solid((-w + 1, height, d - 2), (-w + 1.2, height + 6, d - 1.8), "steel")
    b.solid((-w + .7, height + 5, d - 2.1), (-w + 1.5, height + 5.1, d - 1.7), "steel")
    # Inside: the public counter, a partition, two barred cells at the back.
    b.solid((-3, slab, -2.8), (3, 1.1, -2.4), "navy")
    b.solid((-w + t, slab, 0), (-1, 2.6, .15), "plaster")
    b.solid((1, slab, 0), (w - t, 2.6, .15), "plaster")
    for x0 in (-w + t, 1.2):
        for k in range(9):
            x = x0 + .3 + k * .6
            b.solid((x, slab, .15), (x + .06, 2.4, .21), "steel")
        b.solid((x0, 2.4, .15), (x0 + 5.6, 2.5, .21), "steel")
    # The carport on +X: posts and a flat roof over a parking bay.
    for z in (-d + .2, d - .4):
        b.solid((w + 5.6, 0, z), (w + 5.8, 3.0, z + .2), "steel")
    b.solid((w, 3.0, -d), (w + 6, 3.15, d), "navy")
    return b


def patrol_outpost():
    """A Ŝosea Patrolo outpost (modular, one storey, enterable): a small block in tan render over a
    brown plinth, an orange fascia band for the star sign, a glass door and front window, a counter
    and a desk inside, and a carport on the +X side for the cruiser. Front faces -Z; 8 x 6 m plus
    the 6 m carport."""
    b = Builder()
    b.material("render", flat((206, 178, 128), 4), 3, 3)
    b.material("brown", flat((104, 72, 40), 4), 3, 1)
    b.material("orange", flat((232, 118, 28), 2), 3, 1)
    b.material("plaster", flat((214, 200, 170), 3), 3, 3)
    b.material("floor", flat((128, 120, 108), 6), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("steel", flat((120, 124, 130), 3), 3, 3)
    b.material("wood", wood(), 2, 2)
    w, d, height, t, slab = 4.0, 3.0, 3.4, .25, .2
    front = [(-3.4, -2.2, slab, 2.4), (-1.6, 3.2, .9, 2.4)]          # the door, the long front window
    back = [(-1.0, 1.0, 1.2, 2.2)]
    sides = [(-1.0, 1.0, 1.0, 2.2)]
    wall_with_openings(b, 'x', -d, (-w, w), height, t, front, "render", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), height, t, back, "render", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), height, t, sides, "render", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), height, t, [(-1.2, 1.2, slab, 2.3)], "render", "plaster", +1)  # door to the carport
    b.box((-w + t, 0, -d + t), (w - t, slab, d - t), {'+y': "floor", '-y': "plaster"})
    b.box((-w, height - .2, -d), (w, height, d), {'+y': "roof", '-y': "plaster", '+x': "brown", '-x': "brown", '+z': "brown", '-z': "brown"})
    b.solid((-w - .05, 0, -d - .05), (w + .05, .45, d + .05), "brown")
    b.solid((-w - .1, height - .75, -d - .15), (w + .1, height - .2, -d), "orange")       # the fascia (sign band)
    for lo, hi in (((-w, height, -d), (w, height + .35, -d + .15)), ((-w, height, d - .15), (w, height + .35, d)),
                   ((-w, height, -d), (-w + .15, height + .35, d)), ((w - .15, height, -d), (w, height + .35, d))):
        b.solid(lo, hi, "render")
    # A canopy over the door, the counter by the window, a desk at the back, a radio mast.
    b.solid((-3.7, 2.55, -d - .9), (-1.9, 2.68, -d), "brown")
    b.solid((-1.4, slab, -d + .5), (3.0, 1.05, -d + .9), "wood")
    b.solid((-.8, slab, d - 1.1), (1.6, .8, d - .4), "wood")
    b.solid((-w + .6, height, d - .8), (-w + .75, height + 4, d - .65), "steel")
    # The carport on +X: two posts and a flat brown roof with an orange edge.
    for z in (-d + .2, d - .4):
        b.solid((w + 5.6, 0, z), (w + 5.8, 2.8, z + .2), "steel")
    b.solid((w, 2.8, -d), (w + 6, 2.95, d), "brown")
    b.solid((w, 2.95, -d - .02), (w + 6, 3.05, -d + .1), "orange")
    return b


def state_prison():
    """Ŝtata Malliberejo, Verda's only state prison: an 84 x 64 m walled compound on its own pad.
    A 6 m concrete wall with razor wire and a guard tower at each corner, a gatehouse with an
    8 m vehicle gate in the front (-Z) wall and the intake bay inside it, a two-storey cell block
    (barred windows) across the back, the admin block by the gate, and an exercise yard with a
    basketball hoop, benches and a weights bench. Symmetric outside, so it stands on its centre."""
    b = Builder()
    b.material("wall", flat((168, 166, 158), 6), 3, 3)
    b.material("inner", flat((150, 148, 140), 6), 3, 3)
    b.material("slab", flat((128, 126, 120), 8), 4, 4)
    b.material("yard", flat((110, 112, 108), 10), 4, 4)
    b.material("cells", window_tile((176, 168, 150), (60, 62, 66), (34, 38, 44), .3, .75, .3), 3, FLOOR)
    b.material("admin", window_tile((196, 190, 176), (80, 84, 90), (70, 96, 116)), BAY, FLOOR)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("steel", flat((96, 100, 106), 3), 3, 3)
    b.material("wire", flat((150, 154, 160), 2), 1, 1)
    b.material("orange", flat((222, 112, 30), 2), 1, 1)
    b.material("tower", flat((140, 138, 132), 5), 3, 3)
    b.material("glass", flat((60, 80, 96), 2), 1, 1)
    b.material("wood", wood(), 2, 2)
    W, D, H, T = 42.0, 32.0, 6.0, .6
    gate = 5.0
    b.box((-W, 0, -D - .65), (W, .12, D + .65), {'+y': "yard", '-y': "slab", '+x': "slab", '-x': "slab", '+z': "slab", '-z': "slab"})
    # The perimeter wall, the gate in the front.
    wall_with_openings(b, 'x', -D, (-W, W), H, T, [(-gate, gate, 0, 5.2)], "wall", "inner", -1)
    wall_with_openings(b, 'x', D, (-W, W), H, T, [], "wall", "inner", +1)
    wall_with_openings(b, 'z', -W, (-D + T, D - T), H, T, [], "wall", "inner", -1)
    wall_with_openings(b, 'z', W, (-D + T, D - T), H, T, [], "wall", "inner", +1)
    # Razor wire along the top: a rail and coils.
    for lo, hi in (((-W, H, -D), (W, H + .5, -D + .08)), ((-W, H, D - .08), (W, H + .5, D)),
                   ((-W, H, -D), (-W + .08, H + .5, D)), ((W - .08, H, -D), (W, H + .5, D))):
        b.solid(lo, hi, "wire")
    # The gatehouse: piers either side of the gate, a lintel with an orange band (the sign).
    for x in (-gate - 2.2, gate):
        b.solid((x, 0, -D - .6), (x + 2.2, H + 1.6, -D + 1.6), "tower")
    b.solid((-gate - 2.2, 5.2, -D - .6), (gate + 2.2, H + 1.6, -D + 1.6), "tower")
    b.solid((-gate, 5.6, -D - .65), (gate, 6.9, -D - .6), "orange")
    # Guard towers at the corners: four legs, a glazed cabin, a roof.
    for sx in (-1, 1):
        for sz in (-1, 1):
            cx, cz = sx * (W - 1.6), sz * (D - 1.6)
            for lx in (-1.2, 1.0):
                for lz in (-1.2, 1.0):
                    b.solid((cx + lx, 0, cz + lz), (cx + lx + .25, 9, cz + lz + .25), "steel")
            b.solid((cx - 1.6, 9, cz - 1.6), (cx + 1.6, 9.2, cz + 1.6), "tower")
            b.solid((cx - 1.6, 9.2, cz - 1.6), (cx + 1.6, 10.2, cz + 1.6), "tower")
            b.solid((cx - 1.5, 10.2, cz - 1.5), (cx + 1.5, 11.2, cz + 1.5), "glass")
            b.solid((cx - 1.9, 11.2, cz - 1.9), (cx + 1.9, 11.5, cz + 1.9), "roof")
    # The cell block across the back: two storeys, barred windows all round, a flat roof.
    cw, c0, c1, ch = 30.0, 8.0, 26.0, 2 * FLOOR + .6
    b.box((-cw, 0, c0), (cw, ch, c1), {'+y': "roof", '-y': "slab", '+x': "cells", '-x': "cells", '+z': "cells", '-z': "cells"}, v_origin=0)
    b.solid((-3, 0, c0 - 1.8), (3, 3.2, c0), "inner")                     # the yard door's porch
    b.solid((-1.2, .12, c0 - 1.85), (1.2, 2.6, c0 - 1.8), "steel")         # the door
    # The admin block by the gate (left as you come in), and the intake bay right of it.
    b.box((-W + T, 0, -D + T), (-14, 4.2, -D + 12), {'+y': "roof", '-y': "slab", '+x': "admin", '-x': "admin", '+z': "admin", '-z': "admin"}, v_origin=0)
    b.solid((10, 3.6, -D + T), (24, 3.8, -D + 9), "steel")                 # the intake canopy
    for x in (10.2, 23.6):
        b.solid((x, 0, -D + 8.6), (x + .2, 3.6, -D + 8.8), "steel")
    # The exercise yard: a basketball hoop, benches, a weights bench, painted court lines.
    b.solid((-24, .12, -6), (-6, .14, -5.9), "orange")
    b.solid((-24, .12, 4), (-6, .14, 4.1), "orange")
    b.solid((-15.1, 0, 3.6), (-14.9, 3.4, 3.8), "steel")
    b.solid((-15.9, 3.0, 3.2), (-14.1, 4.1, 3.3), "wire")
    for x in (2.0, 6.0):
        b.solid((x, 0, 2), (x + 2.6, .45, 2.5), "wood")
    b.solid((14, 0, -2), (15, .5, .2), "steel")
    b.solid((13.6, .9, -1.2), (15.4, .95, -1.1), "steel")
    return b


def highway_gantry():
    """An overhead sign gantry spanning the highway (25 m between the posts, over the 18.8 m paved
    width and the verges): two galvanised lattice posts on concrete footings, a box truss across at
    6.4 m, and a maintenance walkway with a rail. The game hangs the green direction boards on it
    (runtime signs) at y 7.7 on both faces. Across is X; traffic passes along Z."""
    b = Builder()
    b.material("steel", flat((150, 156, 160), 3), 2, 2)
    b.material("dark", flat((90, 96, 100), 3), 2, 2)
    b.material("concrete", flat((176, 172, 164), 5), 2, 2)
    half = 12.5
    for x in (-half, half):
        b.solid((x - .9, 0, -.9), (x + .9, .6, .9), "concrete")
        for dx in (-.35, .35):
            for dz in (-.35, .35):
                b.solid((x + dx - .08, .6, dz - .08), (x + dx + .08, 7.6, dz + .08), "steel")
        for y in (1.8, 3.4, 5.0):
            b.solid((x - .43, y, -.43), (x + .43, y + .08, .43), "dark")
    for y in (6.4, 7.5):
        for z in (-.5, .5):
            b.solid((-half - .43, y, z - .07), (half + .43, y + .14, z + .07), "steel")
    for k in range(26):
        x = -half + k * (2 * half / 25)
        b.solid((x - .05, 6.4, -.5), (x + .05, 7.64, .5), "dark")
    b.solid((-half, 6.3, -1.2), (half, 6.38, -.5), "dark")          # walkway
    b.solid((-half, 6.38, -1.2), (half, 7.3, -1.14), "steel")       # rail
    return b


def smugglers_jetty():
    """A timber jetty on the open shore - where contraband comes ashore. A 6 m wide deck on piles
    44 m out from the beach (its land end at -Z), the piles standing from y = 0 (the sea bed) so
    the deck is 10 m up and stands 2.2 m over the water when placed. A small coaster lies along
    the +X side (dark hull, white wheelhouse, a mast and boom), a tender with crates on the -X
    side, lamp posts, mooring bollards, and stacked crates and barrels at the land end."""
    b = Builder()
    b.material("deck", wood(), 2, 2)
    b.material("pile", flat((84, 66, 48), 6), 1, 3)
    b.material("hull", flat((40, 52, 64), 4), 3, 3)
    b.material("hull_red", flat((150, 40, 32), 3), 3, 1)
    b.material("white", flat((228, 228, 222), 2), 3, 3)
    b.material("glass", flat((50, 70, 88), 2), 1, 1)
    b.material("crate", flat((156, 118, 70), 8), 1, 1)
    b.material("tarp", flat((52, 92, 60), 4), 1, 1)
    b.material("steel", flat((96, 100, 106), 3), 1, 1)
    b.material("barrel", flat((40, 70, 120), 4), 1, 1)
    top, z0, z1, w = 10.0, -22.0, 22.0, 3.0
    water = top - 2.2
    b.box((-w, top - .25, z0), (w, top, z1), {s: "deck" for s in ('+y', '-y', '+x', '-x', '+z', '-z')})
    for z in range(int(z0) + 1, int(z1), 4):
        for x in (-w + .3, w - .6):
            b.solid((x, 0, z), (x + .3, top - .25, z + .3), "pile")
    for z in (z0 + 4, 0.0, z1 - 4):
        for x in (-w - .1, w - .2):
            b.solid((x, top, z), (x + .3, top + .5, z + .3), "steel")       # bollards
    for z in (z0 + 2, z1 - 2):
        b.solid((-w + .2, top, z), (-w + .35, top + 4, z + .15), "steel")   # lamp posts
        b.solid((-w + .2, top + 3.8, z - .2), (-w + .8, top + 4, z + .35), "white")
    # The coaster along the +X side.
    hx0, hx1, hz0, hz1 = w + .4, w + 5.4, -14.0, 16.0
    b.solid((hx0, water - 1.2, hz0), (hx1, water - .2, hz1), "hull_red")
    b.solid((hx0, water - .2, hz0), (hx1, water + 1.8, hz1), "hull")
    b.solid((hx0 + .6, water + 1.8, hz1 - 1.5), (hx1 - .6, water + 2.6, hz1 + 2.2), "hull")   # the bow
    b.solid((hx0 + .8, water + 1.8, hz0 + 1.0), (hx1 - .8, water + 4.4, hz0 + 6.0), "white")  # wheelhouse
    b.solid((hx0 + .9, water + 3.2, hz0 + .95), (hx1 - .9, water + 4.0, hz0 + 6.05), "glass")
    b.solid((hx0 + 2.3, water + 1.8, hz0 + 9.5), (hx0 + 2.6, water + 9.0, hz0 + 9.8), "steel")  # mast
    b.solid((hx0 + 2.4, water + 6.0, hz0 + 9.6), (hx0 + 2.5, water + 6.1, hz1 - 2.0), "steel")  # boom
    for k in range(3):
        z = hz0 + 9.0 + k * 4.0
        b.solid((hx0 + .8, water + 1.8, z), (hx1 - .8, water + 3.0, z + 3.0), "tarp")      # cargo under tarps
    # The tender on the -X side, with crates.
    b.solid((-w - 5.4, water - .5, -6.0), (-w - 2.2, water + .6, 6.0), "white")
    for z in (-4.0, -1.5, 1.0):
        b.solid((-w - 4.8, water + .6, z), (-w - 2.8, water + 1.6, z + 2.0), "crate")
    # Crates and barrels stacked at the land end.
    for i, (x, z, h) in enumerate(((-2.4, z0 + 1.0, 1.0), (-1.2, z0 + 1.0, 2.0), (1.2, z0 + 1.2, 1.0), (-2.4, z0 + 2.4, 1.0))):
        b.solid((x, top, z), (x + 1.1, top + h, z + 1.1), "crate")
    for x, z in ((2.0, z0 + 3.0), (2.0, z0 + 4.2), (1.0, z0 + 3.6)):
        b.solid((x, top, z), (x + .6, top + .9, z + .6), "barrel")
    return b


def hospital():
    """Hospitalo de Verda: a three-storey hospital (36 x 20 m) in white render with blue ribbon
    windows, an enterable ground-floor hall (glass entrance, reception, beds), sealed wards
    above, red crosses on the front and roof, and a drive-under ambulance bay with a canopy on
    the -X side (room for three ambulances). Front faces -Z."""
    b = Builder()
    b.material("render", ribbon((236, 236, 232), (88, 128, 168)), BAY, FLOOR)
    b.material("white", flat((238, 238, 234), 3), 3, 3)
    b.material("ground", storefront((210, 212, 214), (96, 130, 156), (200, 30, 36)), BAY, 4.5)
    b.material("plaster", flat((210, 214, 218), 3), 3, 3)
    b.material("floor", flat((196, 200, 202), 4), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("red", flat((206, 28, 34), 0), 1, 1)
    b.material("green", flat((0, 140, 70), 2), 1, 1)
    b.material("steel", flat((150, 154, 160), 3), 3, 3)
    b.material("bed", flat((240, 240, 236), 2), 1, 1)
    w, d, t, slab = 18.0, 10.0, .3, .2
    ground_h = 4.5
    top = ground_h + 2 * FLOOR
    # Ground floor: an open, enterable hall. Doors at the front (middle) and to the bay (-X side).
    front = [(-2, 2, slab, 3.0)] + [(x, x + 2.4, .8, 3.4) for x in (-15, -11, -7, 4.6, 8.6, 12.6)]
    back = [(-1, 1, slab, 2.6)] + [(x, x + 2.4, 1.0, 3.2) for x in (-14, -8, 6, 12)]
    wall_with_openings(b, 'x', -d, (-w, w), ground_h, t, front, "white", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), ground_h, t, back, "white", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), ground_h, t, [(-6, -2, slab, 3.2)], "white", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), ground_h, t, [(-2, 2, 1.0, 3.2)], "white", "plaster", +1)
    b.box((-w + t, 0, -d + t), (w - t, slab, d - t), {'+y': "floor", '-y': "plaster"})
    b.box((-w, ground_h - .25, -d), (w, ground_h, d), {'-y': "plaster", '+x': "white", '-x': "white", '+z': "white", '-z': "white"})
    # The wards: sealed upper storeys with ribbon windows, a roof with a parapet.
    b.box((-w, ground_h, -d), (w, top, d), {s: "render" for s in ('+x', '-x', '+z', '-z')}, v_origin=top)
    b.box((-w, top, -d), (w, top + .05, d), {'+y': "roof"})
    for lo, hi in (((-w - .2, top, -d - .2), (w + .2, top + 1.0, -d + .2)), ((-w - .2, top, d - .2), (w + .2, top + 1.0, d + .2)),
                   ((-w - .2, top, -d), (-w + .2, top + 1.0, d)), ((w - .2, top, -d), (w + .2, top + 1.0, d))):
        b.solid(lo, hi, "white")
    # Green band over the hall, red crosses on the front and on the roof.
    b.solid((-w - .1, ground_h - .5, -d - .15), (w + .1, ground_h, -d), "green")
    for cx in (-12.0, 12.0):
        b.solid((cx - 1.5, top - 2.3, -d - .12), (cx + 1.5, top - 1.3, -d - .02), "red")
        b.solid((cx - .5, top - 3.3, -d - .12), (cx + .5, top - .3, -d - .02), "red")
    b.solid((-5, top + .05, -1.5), (5, top + .1, 1.5), "red")
    b.solid((-1.5, top + .05, -5), (1.5, top + .1, 5), "red")
    # Entrance canopy.
    b.solid((-4, 3.3, -d - 3), (4, 3.5, -d), "white")
    for x in (-3.8, 3.6):
        b.solid((x, 0, -d - 3), (x + .2, 3.3, -d - 2.8), "steel")
    # The ambulance bay on -X: a drive-under canopy 10 m out and 18 m deep, posts at the outer edge.
    b.solid((-w - 10, 4.2, -d - 1), (-w, 4.45, d - 1), "white")
    b.solid((-w - 10, 4.0, -d - 1), (-w, 4.2, -d - .8), "red")
    for z in (-d - 1, -1.0, d - 1.4):
        b.solid((-w - 10, 0, z), (-w - 9.6, 4.2, z + .4), "steel")
    # Inside the hall: reception desk, rows of beds with curtains, a partition.
    b.solid((-3, slab, -6.5), (3, 1.1, -5.8), "green")
    for row, z in enumerate((2.0, 6.0)):
        for k in range(7):
            x = -15 + k * 4.3
            if -3 < x < 3:
                continue
            b.solid((x, slab, z), (x + 1.0, .75, z + 2.1), "bed")
            b.solid((x - .4, slab, z - .2), (x - .35, 2.2, z + 2.3), "steel")
    b.solid((-w + t, slab, -.2), (-6, 2.8, .0), "plaster")
    b.solid((6, slab, -.2), (w - t, 2.8, .0), "plaster")
    return b


def flag(b, x, z, height, cloth, pole="steel"):
    """A flagpole with its flag flying toward +X."""
    b.solid((x - .07, 0, z - .07), (x + .07, height, z + .07), pole)
    b.solid((x + .07, height - 1.6, z - .03), (x + 2.6, height - .1, z + .03), cloth)


def parliament():
    """Registaro de Verda: the island's parliament on the bay in Verda City. A neoclassical stone
    building (48 x 22 m, front faces -Z): a tall centre block with a six-column portico under a
    pediment and broad steps, lower wings either side, and over the centre a drum with a green
    copper dome and a lantern (top at 25.5 m). The raised ground floor (1.5 m) is one great
    enterable hall under the dome: the chamber's desk and benches, galleries of windows. Flags of
    Verda fly on two poles at the foot of the steps."""
    b = Builder()
    b.material("stone", window_tile((226, 218, 196), (180, 170, 146), (70, 92, 104), .3, .82, .42), BAY, FLOOR)
    b.material("ashlar", flat((222, 214, 192), 5), 3, 3)
    b.material("white", flat((240, 238, 230), 3), 3, 3)
    b.material("steps", flat((196, 192, 182), 5), 3, 3)
    b.material("floor", pavers(), 3, 3)
    b.material("plaster", flat((232, 226, 210), 3), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("copper", flat((88, 150, 128), 6), 3, 3)
    b.material("gold", flat((212, 172, 70), 2), 1, 1)
    b.material("wood", wood(), 3, 3)
    b.material("bench", flat((120, 40, 40), 4), 1, 1)
    b.material("green", flat((16, 128, 70), 2), 1, 1)
    b.material("steel", flat((150, 154, 160), 3), 3, 3)
    t, floor = .4, 1.5
    cw, cd, ch = 12.0, 11.0, 13.0     # centre block half-width, half-depth, height
    ww, wd, wh = 24.0, 9.0, 10.0      # wings out to x = +-24, half-depth 9
    # Plinth under everything (the raised floor), then the floor of the hall.
    b.solid((-ww, 0, -cd), (ww, floor, cd), "ashlar")
    b.box((-cw + t, floor, -cd + t), (cw - t, floor + .05, cd - t), {'+y': "floor"})
    # Centre block: front wall with the great doors and two tall windows, back and sides.
    front = [(-1.8, 1.8, floor, floor + 4.2), (-8.5, -6.0, floor + 1.2, floor + 6.5), (6.0, 8.5, floor + 1.2, floor + 6.5)]
    back = [(x, x + 2.4, floor + 1.2, floor + 6.5) for x in (-9.0, -4.0, 1.6, 6.6)]
    wall_with_openings(b, 'x', -cd, (-cw, cw), ch, t, front, "stone", "plaster", -1)
    wall_with_openings(b, 'x', cd, (-cw, cw), ch, t, back, "stone", "plaster", +1)
    # Inner side walls open into the wings through wide arches.
    for x, out in ((-cw, -1), (cw, 1)):
        wall_with_openings(b, 'z', x, (-cd + t, cd - t), ch, t, [(-4, 4, floor, floor + 5)], "stone", "plaster", out)
    b.box((-cw, ch - .3, -cd), (cw, ch, cd), {'+y': "roof", '-y': "plaster"})
    b.solid((-cw - .3, ch - 1.2, -cd - .3), (cw + .3, ch - .3, cd + .3), "white")     # cornice
    # Wings: lower, sealed beyond the arches (offices), windowed all round.
    for sgn in (-1, 1):
        x0, x1 = sorted((sgn * cw, sgn * ww))
        b.box((x0, floor, -wd), (x1, wh, wd), {s: "stone" for s in ('+z', '-z', '+x' if sgn > 0 else '-x')}, v_origin=wh)
        b.box((x0, wh - .2, -wd), (x1, wh, wd), {'+y': "roof"})
        b.solid((x0 - (.3 if sgn < 0 else 0), wh - .9, -wd - .3), (x1 + (.3 if sgn > 0 else 0), wh - .2, wd + .3), "white")
    # The portico: steps, six columns, entablature and pediment.
    for k in range(5):
        z0 = -cd - 4.2 - 1.2 * k
        b.solid((-cw + .4 - .3 * k, 0, z0), (cw - .4 + .3 * k, floor - .3 * k, -cd), "steps")
    b.box((-cw + .4, floor - .05, -cd - 4.2), (cw - .4, floor, -cd), {'+y': "floor"})
    for x in (-10.0, -6.0, -2.0, 2.0, 6.0, 10.0):
        b.solid((x - .8, floor, -cd - 3.8), (x + .8, floor + .5, -cd - 2.2), "white")
        b.ring("white", x, -cd - 3.0, .6, .52, floor + .5, ch - 1.4, 12)
        b.solid((x - .8, ch - 1.4, -cd - 3.8), (x + .8, ch - 1.0, -cd - 2.2), "white")
    b.solid((-cw + .2, ch - 1.0, -cd - 4.2), (cw - .2, ch + .2, -cd), "white")
    b.pediment("white", -cw + .2, cw - .2, ch + .2, 3.4, -cd - 4.2, 4.2)
    b.solid((-1.4, ch + .9, -cd - 4.25), (1.4, ch + 1.7, -cd - 4.2), "gold")      # the arms of Verda
    # The drum, the dome and the lantern over the hall.
    b.ring("stone", 0, 0, 7.4, 7.4, ch, ch + 4.0, 24)
    b.ring("white", 0, 0, 7.8, 7.8, ch + 4.0, ch + 4.6, 24, top_cap=False)
    b.dome("copper", 0, 0, 7.6, ch + 4.6, 7, 24, 1.0)
    b.ring("white", 0, 0, 1.3, 1.3, ch + 12.0, ch + 14.2, 8)
    b.dome("copper", 0, 0, 1.5, ch + 14.2, 3, 8, 1.1)
    b.solid((-.08, ch + 15.6, -.08), (.08, ch + 16.6, .08), "gold")
    # Inside the hall: the president's desk on a dais, curved rows of red benches, a gallery rail.
    b.solid((-3.5, floor, 6.0), (3.5, floor + .4, 9.5), "wood")
    b.solid((-2.4, floor + .4, 7.0), (2.4, floor + 1.4, 7.8), "wood")
    for row, z in enumerate((-1.0, 1.4, 3.6)):
        span = 4.0 + row * 1.6
        b.solid((-span - 2.6, floor, z), (-1.4, floor + .5 + .2 * row, z + .7), "bench")
        b.solid((1.4, floor, z), (span + 2.6, floor + .5 + .2 * row, z + .7), "bench")
    # Flags at the foot of the steps.
    for x in (-cw - 1.5, cw + 1.5):
        flag(b, x, -cd - 9.5, 11.0, "green")
    return b


def town_hall():
    """Urbodomo: a county town's hall (20 x 14 m, two storeys, front faces -Z). Cream render with
    stone quoins, a four-column porch under a small pediment up three steps, a clock tower with a
    green cupola over the middle (top at 17 m) and a flag on the roof. The raised ground floor
    (0.6 m) is an enterable hall with the clerks' counter and benches for the public."""
    b = Builder()
    b.material("render", window_tile((232, 222, 196), (176, 160, 128), (70, 92, 104), .28, .84, .46), BAY, FLOOR)
    b.material("stone", flat((200, 192, 172), 5), 3, 3)
    b.material("white", flat((240, 238, 230), 3), 3, 3)
    b.material("floor", pavers(), 3, 3)
    b.material("plaster", flat((228, 222, 206), 3), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("copper", flat((88, 150, 128), 6), 3, 3)
    b.material("clock", flat((246, 244, 236), 1), 1, 1)
    b.material("hands", flat((30, 30, 32), 1), 1, 1)
    b.material("wood", wood(), 3, 3)
    b.material("green", flat((16, 128, 70), 2), 1, 1)
    b.material("steel", flat((150, 154, 160), 3), 3, 3)
    w, d, height, t, floor = 10.0, 7.0, 8.0, .3, .6
    b.solid((-w, 0, -d), (w, floor, d), "stone")
    b.box((-w + t, floor, -d + t), (w - t, floor + .05, d - t), {'+y': "floor"})
    front = [(-1.4, 1.4, floor, floor + 3.0), (-7.6, -5.4, floor + 1.0, floor + 2.8), (5.4, 7.6, floor + 1.0, floor + 2.8)]
    back = [(x, x + 2.0, floor + 1.0, floor + 2.8) for x in (-7.0, -3.0, 1.0, 5.0)]
    sides = [(-4.5, -2.5, floor + 1.0, floor + 2.8), (2.5, 4.5, floor + 1.0, floor + 2.8)]
    wall_with_openings(b, 'x', -d, (-w, w), height, t, front, "render", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), height, t, back, "render", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), height, t, sides, "render", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), height, t, sides, "render", "plaster", +1)
    b.box((-w, height - .25, -d), (w, height, d), {'+y': "roof", '-y': "plaster"})
    b.solid((-w - .25, height - .8, -d - .25), (w + .25, height - .25, d + .25), "white")
    for x in (-w, w - .5):                                   # quoins at the corners
        for z in (-d - .05, d - .45):
            b.solid((x - .05, floor, z), (x + .55, height - .8, z + .5), "stone")
    # Porch: three steps, four columns, a small pediment.
    for k in range(3):
        b.solid((-5.2 - .3 * k, 0, -d - 3.0 - .9 * k), (5.2 + .3 * k, floor - .2 * k, -d), "stone")
    b.box((-5.0, floor - .05, -d - 3.0), (5.0, floor, -d), {'+y': "floor"})
    for x in (-4.2, -1.4, 1.4, 4.2):
        b.ring("white", x, -d - 2.4, .38, .33, floor, floor + 4.0, 10)
    b.solid((-5.0, floor + 4.0, -d - 3.0), (5.0, floor + 4.6, -d), "white")
    b.pediment("white", -5.0, 5.0, floor + 4.6, 1.6, -d - 3.0, 3.0)
    # The clock tower and its cupola, and a flag beside it.
    b.solid((-2.0, height, -2.0), (2.0, height + 5.0, 2.0), "render")
    b.solid((-2.2, height + 5.0, -2.2), (2.2, height + 5.4, 2.2), "white")
    for (nx, nz) in ((0, -1), (0, 1), (-1, 0), (1, 0)):
        cx, cz = nx * 2.02, nz * 2.02
        if nz:
            b.solid((cx - .9, height + 2.6, cz - .02), (cx + .9, height + 4.4, cz + .02), "clock")
            b.solid((cx - .05, height + 3.5, cz - .04 * nz), (cx + .05, height + 4.2, cz + .04 * nz), "hands")
            b.solid((cx, height + 3.45, cz - .04 * nz), (cx + .55, height + 3.55, cz + .04 * nz), "hands")
        else:
            b.solid((cx - .02, height + 2.6, cz - .9), (cx + .02, height + 4.4, cz + .9), "clock")
            b.solid((cx - .04 * nx, height + 3.5, cz - .05), (cx + .04 * nx, height + 4.2, cz + .05), "hands")
            b.solid((cx - .04 * nx, height + 3.45, cz), (cx + .04 * nx, height + 3.55, cz + .55), "hands")
    b.ring("white", 0, 0, 1.6, 1.6, height + 5.4, height + 6.4, 12)
    b.dome("copper", 0, 0, 1.8, height + 6.4, 4, 12, 1.1)
    b.solid((-.06, height + 8.3, -.06), (.06, height + 9.0, .06), "steel")
    flag(b, w - 1.5, -d + 1.5, height + 5.0, "green")
    # Inside: the clerks' counter, a partition, benches for the public.
    b.solid((-4.0, floor, 2.2), (4.0, floor + 1.1, 2.8), "wood")
    b.solid((-w + t, floor, 3.2), (-4.0, floor + 2.6, 3.4), "plaster")
    b.solid((4.0, floor, 3.2), (w - t, floor + 2.6, 3.4), "plaster")
    for x in (-6.5, -2.0, 2.0, 6.5):
        b.solid((x - 1.4, floor, -3.6), (x + 1.4, floor + .45, -3.1), "wood")
    return b


# ---------------------------------------------------------------- ground pieces
def pad(size, kind):
    """Block surface 0.15 m above the street: sidewalk (pavers), park (grass + paths) or parking."""
    b = Builder()
    b.material("pavers", pavers(), 3, 3)
    b.material("curb", flat((150, 148, 142), 4), 3, 1)
    h = size / 2
    top = .15
    sides = {'+x': "curb", '-x': "curb", '+z': "curb", '-z': "curb", '-y': "curb"}
    if kind == "sidewalk":
        b.box((-h, 0, -h), (h, top, h), {**sides, '+y': "pavers"})
    elif kind == "park":
        b.material("grass", grass(), 3, 3)
        b.box((-h, 0, -h), (h, top, h), {**sides, '+y': "grass"})
        ring = 4.0
        for lo, hi in (((-h, top, -h), (h, top + .02, -h + ring)), ((-h, top, h - ring), (h, top + .02, h)),
                       ((-h, top, -h + ring), (-h + ring, top + .02, h - ring)), ((h - ring, top, -h + ring), (h, top + .02, h - ring)),
                       ((-2, top, -h + ring), (2, top + .02, h - ring)), ((-h + ring, top, -2), (h - ring, top + .02, 2))):
            b.box(lo, hi, {'+y': "pavers"})
        # Centre plinth for a monument / fountain.
        b.material("stone", flat((198, 192, 178), 5), 3, 1)
        b.solid((-3.5, top, -3.5), (3.5, top + .6, 3.5), "stone")
        b.solid((-1, top + .6, -1), (1, top + 4.5, 1), "stone")
    else:  # parking
        b.material("asphalt", asphalt([(0, 0, 2, TEX, (232, 230, 220))]), 2.7, 5.5)
        ring = 4.0
        b.box((-h, 0, -h), (h, top, h), sides)
        b.box((-h + ring, top - .02, -h + ring), (h - ring, top, h - ring), {'+y': "asphalt"})
        for lo, hi in (((-h, 0, -h), (h, top, -h + ring)), ((-h, 0, h - ring), (h, top, h)),
                       ((-h, 0, -h + ring), (-h + ring, top, h - ring)), ((h - ring, 0, -h + ring), (h, top, h - ring))):
            b.box(lo, hi, {'+y': "pavers"})
    return b


def intersection(size):
    """Asphalt plate with zebra crosswalks on all four approaches (sits 2 cm over the road ribbons)."""
    b = Builder()
    b.material("asphalt", asphalt(), 3, 3)
    b.material("zebra", stripes((228, 226, 216), (70, 72, 71), width=8), 1.0, 1.0)
    h = size / 2
    y0, y1 = .07, .09
    b.box((-h, y0, -h), (h, y1, h), {'+y': "asphalt"})
    walk = 3.0
    for lo, hi in (((-h + .5, y1, -h), (h - .5, y1 + .005, -h + walk)), ((-h + .5, y1, h - walk), (h - .5, y1 + .005, h))):
        b.box(lo, hi, {'+y': "zebra"})
    # East/west crossings: stripes must run the other way, so swap UV axes by building them as +x faces? Keep
    # it simple: rotate by using a second material tiled along z.
    b.material("zebra_ns", stripes((228, 226, 216), (70, 72, 71), width=8, seed=43), 1.0, 1.0)
    for lo, hi in (((-h, y1, -h + walk), (-h + walk, y1 + .005, h - walk)), ((h - walk, y1, -h + walk), (h, y1 + .005, h - walk))):
        positions_before = len(b.prims.get("zebra_ns", ([], [], [], []))[0])
        b.box(lo, hi, {'+y': "zebra_ns"})
        # Re-map the new quad's UVs so stripes run across the walking direction.
        prim = b.prims["zebra_ns"]
        for i in range(positions_before, len(prim[0])):
            p = prim[0][i]
            prim[2][i] = (p[2] / 1.0, p[0] / 1.0)
    return b


# ---------------------------------------------------------------- the kit
def kit():
    pieces = []  # (relative path, builder, id, name, category, footprint w, d, h)
    def add(path, builder, ident, name, category, w, d, h):
        pieces.append((path, builder, ident, name, category, w, d, h))
    towers = [
        ("glass_dark_tower", "glass_dark", 24, 24, 20, 3, 14, "Bank Tower (dark glass, 20 floors)"),
        ("glass_blue_tower", "glass_blue", 22, 26, 16, 0, 0, "Office Tower (blue glass, 16 floors)"),
        ("stone_deco_tower", "stone", 26, 26, 14, 3, 9, "Art Deco Tower (stone, 14 floors)"),
        ("concrete_office", "concrete", 30, 22, 8, 0, 0, "Concrete Office (8 floors)"),
        ("brick_midrise", "brick", 24, 20, 7, 0, 0, "Brick Mid-rise (7 floors)"),
        ("stone_midrise", "stone", 20, 24, 6, 0, 0, "Stone Mid-rise (6 floors)"),
        ("glass_blue_midrise", "glass_blue", 28, 20, 9, 0, 0, "Glass Mid-rise (9 floors)"),
    ]
    for ident, style, w, d, floors, setback, at, name in towers:
        add(f"towers/{ident}.glb", tower(style, w, d, floors, setback, at), "city_" + ident, name, "Building", w, d, 4.5 + floors * FLOOR + 1)
    schemes = [
        ("red", (152, 64, 48), (226, 218, 198), (156, 36, 40)),
        ("brown", (112, 74, 54), (214, 204, 178), (40, 84, 64)),
        ("tan", (186, 158, 118), (120, 96, 72), (44, 64, 120)),
        ("cream", (220, 206, 170), (96, 104, 84), (164, 92, 40)),
        ("dark", (86, 58, 50), (196, 186, 160), (120, 30, 36)),
    ]
    seed = 100
    for color, brick_color, trim, awning in schemes:
        for width, storeys in ((12, 2), (18, 3), (12, 3)):
            if (color, width, storeys) in {("dark", 12, 3), ("cream", 12, 3)}:
                continue
            ident = f"main_street_{color}_{width}m_{storeys}st"
            seed += 1
            add(f"main_street/{ident}.glb", main_street(width, 20, storeys, brick_color, trim, awning if seed % 3 else None, seed),
                "city_" + ident, f"Main Street {color.title()} Brick {width} m, {storeys} storeys (enterable)", "Building", width, 20, storeys * FLOOR + .9)
    add("civic/police_station.glb", police_station(), "city_police_station", "Police Station (modular, enterable, carport)", "Building", 20, 12, 4.7)
    add("civic/patrol_outpost.glb", patrol_outpost(), "city_patrol_outpost", "Ŝosea Patrolo outpost (modular, enterable, carport)", "Building", 14, 6, 3.8)
    add("civic/state_prison.glb", state_prison(), "city_state_prison", "Ŝtata Malliberejo - walled state prison, towers, cell block, yard", "Building", 84, 64, 11.5)
    add("civic/highway_gantry.glb", highway_gantry(), "city_highway_gantry", "Highway sign gantry (spans the highway)", "BuildingPart", 27, 2.4, 7.64)
    add("civic/smugglers_jetty.glb", smugglers_jetty(), "city_smugglers_jetty", "Jetty with a moored coaster and contraband crates", "BuildingPart", 18, 44, 19)
    add("civic/hospital.glb", hospital(), "city_hospital", "Hospitalo - hospital with ambulance bay", "Building", 46, 23, 12.7)
    add("civic/parliament.glb", parliament(), "city_parliament", "Registaro de Verda - domed parliament (enterable hall)", "Building", 48, 33, 29.6)
    add("civic/town_hall.glb", town_hall(), "city_town_hall", "Urbodomo - county town hall with clock tower (enterable)", "Building", 23, 17, 17.0)
    add("towers/stela_turo.glb", stela_turo(), "city_stela_turo", "Stela Turo - art deco skyscraper, 17 floors, penthouse (enterable)", "Building", 30.6, 29.2, 86.2)
    add("ground/sidewalk_block_68.glb", pad(68, "sidewalk"), "city_sidewalk_block_68", "Sidewalk Block 68 m", "BuildingPart", 68, 68, .15)
    add("ground/courthouse_square_68.glb", pad(68, "park"), "city_courthouse_square_68", "Courthouse Square Park 68 m", "BuildingPart", 68, 68, 4.6)
    add("ground/parking_lot_68.glb", pad(68, "parking"), "city_parking_lot_68", "Parking Lot 68 m", "BuildingPart", 68, 68, .15)
    add("ground/intersection_12.glb", intersection(12), "city_intersection_12", "Street Intersection 12 m (crosswalks)", "Road", 12, 12, .1)
    add("ground/intersection_16.glb", intersection(16), "city_intersection_16", "Street Intersection 16 m (crosswalks)", "Road", 16, 16, .1)
    return pieces


def catalog_header(pieces):
    lines = [
        "#pragma once",
        "// Generated by tools/generate_city_kit.py - do not edit.",
        "#include \"outland/creator/CreatorAssetRegistry.hpp\"",
        "namespace outland::creator {",
        "inline void register_city_assets(CreatorAssetRegistry& registry) {",
    ]
    for path, _, ident, name, category, w, d, h in pieces:
        lines.append(f"    (void)registry.add({{.id = \"{ident}\", .name = \"{name}\", .category = CreatorAssetCategory::{category},")
        lines.append(f"        .model_path = \"assets/verda/city/{path}\", .thumbnail_path = \"\", .footprint = {{{float(w):.2f}F, {float(d):.2f}F, {float(h):.2f}F}},")
        lines.append("        .placement = {}, .default_scale = 1, .tags = {\"verda\", \"city\", \"downtown\"}});")
    lines += ["}", "}  // namespace outland::creator", ""]
    return "\n".join(lines).encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if committed kit files are stale")
    args = parser.parse_args()
    pieces = kit()
    outputs = {OUT / path: builder.glb(Path(path).stem) for path, builder, *_ in pieces}
    outputs[CATALOG] = catalog_header(pieces)
    stale = [p for p, data in outputs.items() if not p.exists() or p.read_bytes() != data]
    if args.check:
        for p in stale:
            print(f"stale: {p.relative_to(ROOT)}", file=sys.stderr)
        return 1 if stale else 0
    for p, data in outputs.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    print(f"wrote {len(outputs) - 1} city kit models and {CATALOG.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
