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
GAS_ISLANDS = (-3.5, 3.5)       # gas_station(): the pump islands' x
GAS_PUMPS_Z = (-5.0, -3.0)      # ...and each pump's z on them
MECH_BAYS = (-5.0, 1.0)         # mechanic_shop(): the bays' x (door centres)
MECH_DOOR = 4.6                 # ...and the door openings' width
CAR_LOT_ROWS = (-6.0, 3.0)      # car_lot(): the display rows' z
CAR_LOT_SPOTS_X = (-14.0, -8.0, -2.0, 4.0, 10.0)


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
STELA_DOOR = 3.0     # half the lobby doorway's width (inside the gold frame)


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
                        # The lobby: one wide doorway in the middle of the front and back (added
                        # below, inside the gold frame), shop windows elsewhere.
                        if not (abs(centre) < width * .8 and axis == 'x'):
                            openings.append((a0, a1, .9, 3.2))
                    elif floor == first and axis == 'x' and outward < 0 and abs(centre) < width * 1.2 and first > 0:
                        openings.append((a0, a1, base, base + 2.6))   # french doors onto the terrace
                    elif floor == STELA_FLOORS - 1:
                        openings.append((a0, a1, base + .45, base + 3.05))  # the penthouse: tall windows
                    else:
                        openings.append((a0, a1, base + .85, base + 2.85))
            if first == 0 and axis == 'x':
                openings.append((-STELA_DOOR, STELA_DOOR, 0, 3.2))
                openings.sort()
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
    # (broken at the doorways: walk straight in off the pavement)
    for lo, hi in (((-hw0 - .1, 0, -hd0 - .1), (-STELA_DOOR, .9, -hd0)), ((STELA_DOOR, 0, -hd0 - .1), (hw0 + .1, .9, -hd0)),
                   ((-hw0 - .1, 0, hd0), (-STELA_DOOR, .9, hd0 + .1)), ((STELA_DOOR, 0, hd0), (hw0 + .1, .9, hd0 + .1)),
                   ((-hw0 - .1, 0, -hd0), (-hw0, .9, hd0)), ((hw0, 0, -hd0), (hw0 + .1, .9, hd0))):
        b.solid(lo, hi, "granite")
    # A shallow granite step up to the lobby floor in each doorway.
    for face in (-1, 1):
        wall = face * hd0
        b.solid((-STELA_DOOR, 0, min(wall - face * .9, wall + face * .2)), (STELA_DOOR, slab * .5, max(wall - face * .9, wall + face * .2)), "granite")
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


CONTAINER_COLOURS = ((176, 52, 40), (40, 92, 150), (40, 120, 76), (210, 150, 40), (120, 124, 130), (150, 70, 40), (60, 60, 66))


def containers(b, x0, z0, rows, cols, tiers, seed, along_z=True, y0=0.0):
    """Stacked 40 ft boxes (12.2 x 2.44 x 2.6 m), corrugated, in mixed liveries: `rows` side by side
    (x), `cols` end to end (z, or x when not along_z), up to `tiers` high (some stacks lower)."""
    rng = random.Random(seed)
    for k, colour in enumerate(CONTAINER_COLOURS):
        b.material(f"box{k}", stripes(colour, shade(colour, -26), 4, 60 + k), 1.2, 1.2)
    L, W, H = 12.2, 2.44, 2.6
    for r in range(rows):
        for c in range(cols):
            high = tiers if rng.random() < .7 else rng.randint(1, tiers)
            for t in range(high):
                m = f"box{rng.randrange(len(CONTAINER_COLOURS))}"
                if along_z:
                    lo = (x0 + r * (W + .1), y0 + t * H, z0 + c * (L + .3))
                    hi = (lo[0] + W, lo[1] + H - .02, lo[2] + L)
                else:
                    lo = (x0 + c * (L + .3), y0 + t * H, z0 + r * (W + .1))
                    hi = (lo[0] + L, lo[1] + H - .02, lo[2] + W)
                b.solid(lo, hi, m)


def container_stack():
    """A block of 40 ft containers on the quay: four rows of three end to end, up to three high
    (24 x 37 m footprint centred on the origin, long side along Z)."""
    b = Builder()
    containers(b, -5.1, -18.6, 4, 3, 3, 7)
    return b


def quay_apron():
    """The cargo terminal's quay: a 60 x 112 m concrete apron (top at y = 0, the seaward face at
    +X) with lane markings, the crane rails along the edge, a yellow safety line, and the quay wall
    going down 14 m into the water with black rubber fenders and bollards along the top."""
    b = Builder()
    b.material("concrete", flat((170, 168, 160), 7), 4, 4)
    b.material("wall", flat((128, 126, 118), 8), 3, 3)
    b.material("yellow", flat((230, 190, 40), 2), 1, 1)
    b.material("white", flat((236, 236, 230), 2), 1, 1)
    b.material("rail", flat((90, 92, 96), 3), 1, 1)
    b.material("fender", flat((26, 26, 28), 3), 1, 1)
    hx, hz = 30.0, 56.0
    b.box((-hx, -.4, -hz), (hx, 0, hz), {'+y': "concrete", '-x': "wall", '+z': "wall", '-z': "wall"})
    b.box((hx - 1.5, -14, -hz), (hx, 0, hz), {'+x': "wall", '+z': "wall", '-z': "wall"})
    b.solid((hx - 2.2, 0, -hz), (hx - 1.9, .01, hz), "yellow")
    for x in (7.0, 23.0):                                 # crane rails
        b.solid((x - .15, 0, -hz), (x + .15, .03, hz), "rail")
    for z in range(-50, 51, 8):                          # the lane's dashes (x = 6 local)
        b.solid((5.85, 0, z), (6.15, .01, z + 4), "white")
    for z in range(-52, 53, 8):
        b.solid((hx, -6, z - .6), (hx + .5, -1, z + .6), "fender")
    return b


def cargo_ship():
    """Stella Maris, a feeder container ship (96 x 16 m, along Z, bow at -Z). Keel at y = 0, the
    waterline at 5 m and the main deck at 11 m (placed with the keel 5 m under the sea). A red
    bottom and a dark blue hull with a white sheer line, a raked bow, containers in four bays on
    deck, the white accommodation block aft with the bridge and its wings, a funnel in the line's
    green with a white band, masts, and a gangway hatch on the quay (-X) side."""
    b = Builder()
    b.material("bottom", flat((150, 44, 36), 4), 3, 3)
    b.material("hull", flat((28, 40, 64), 4), 3, 3)
    b.material("sheer", flat((236, 236, 230), 2), 3, 1)
    b.material("deck", flat((96, 110, 96), 6), 3, 3)
    b.material("white", window_tile((236, 236, 230), (200, 200, 196), (40, 60, 80), .45, .8, .5), 2.4, 2.8)
    b.material("plain", flat((236, 236, 230), 2), 3, 3)
    b.material("glass", flat((40, 60, 80), 2), 1, 1)
    b.material("funnel", flat((16, 128, 70), 2), 1, 1)
    b.material("black", flat((30, 30, 32), 2), 1, 1)
    b.material("steel", flat((150, 154, 160), 3), 2, 2)
    b.material("hatch", flat((140, 130, 60), 5), 3, 3)
    hw, z0, z1, water, deck = 8.0, -48.0, 48.0, 5.0, 11.0
    # The hull: a parallel body, then the bow narrowing in steps to the stem, and a square stern.
    body0 = z0 + 16
    b.box((-hw, 0, body0), (hw, water, z1), {'+x': "bottom", '-x': "bottom", '+z': "bottom", '-y': "bottom", '-z': "bottom"})
    b.box((-hw, water, body0), (hw, deck, z1), {'+x': "hull", '-x': "hull", '+z': "hull", '+y': "deck", '-z': "hull"})
    b.solid((-hw - .02, deck - .5, body0), (hw + .02, deck - .2, z1 + .02), "sheer")
    steps = 8
    for k in range(steps):
        t0, t1 = k / steps, (k + 1) / steps
        za, zb = body0 - t0 * 16, body0 - t1 * 16
        w = hw * (1 - (t1 ** 1.6) * .85)
        lift = t1 * 2.2       # the sheer rises toward the bow
        b.box((-w, t1 * 3, zb), (w, water, za), {'+x': "bottom", '-x': "bottom", '-y': "bottom", '-z': "bottom"})
        b.box((-w, water, zb), (w, deck + lift, za), {'+x': "hull", '-x': "hull", '+y': "deck", '-z': "hull"})
    b.solid((-1.4, deck + 2.2, z0 + .2), (1.4, deck + 3.2, z0 + 4), "hull")       # the forecastle
    b.solid((-.1, deck + 3.2, z0 + 1), (.1, deck + 10, z0 + 1.2), "steel")        # foremast
    # Hatch covers and four bays of containers forward of the house.
    for bay in range(4):
        zc = -24 + bay * 13.2
        b.solid((-hw + .8, deck, zc), (hw - .8, deck + .6, zc + 12.6), "hatch")
        containers(b, -6.1 + .02, zc + .2, 5, 1, 2 + (bay % 2), 40 + bay, True, deck + .6)
    # The accommodation block aft: five decks, the bridge on top with wings, the funnel behind.
    a0, a1 = 30.0, 44.0
    b.box((-6.5, deck, a0), (6.5, deck + 12.5, a1), {'+x': "white", '-x': "white", '-z': "white", '+z': "white", '+y': "plain"}, v_origin=deck + 12.5)
    b.solid((-hw, deck + 12.5, a0 + .5), (hw, deck + 15.0, a0 + 5.5), "plain")    # the bridge, full beam
    b.solid((-hw + .2, deck + 13.4, a0 + .45), (hw - .2, deck + 14.6, a0 + .5), "glass")
    b.solid((-4.0, deck + 15.0, a0 + 1.5), (4.0, deck + 15.2, a0 + 4.5), "steel")
    b.solid((-.1, deck + 15.2, a0 + 3), (.1, deck + 21, a0 + 3.2), "steel")      # the mast and radar
    b.solid((-1.6, deck + 19.5, a0 + 2.9), (1.6, deck + 19.7, a0 + 3.3), "steel")
    b.solid((-2.2, deck + 12.5, a1 - 3.5), (2.2, deck + 19.0, a1 - .5), "funnel")
    b.solid((-2.25, deck + 16.4, a1 - 3.55), (2.25, deck + 17.2, a1 - .45), "plain")
    b.solid((-2.25, deck + 19.0, a1 - 3.55), (2.25, deck + 19.4, a1 - .45), "black")
    # Bulwark rails, and the hatch on the quay side where the gangway comes aboard (aft).
    for x in (-hw + .1, hw - .3):
        b.solid((x, deck, body0), (x + .2, deck + 1.1, a0), "steel")
    b.solid((-hw - .05, deck - 2.8, 20), (-hw + .1, deck - .2, 26), "black")
    return b


def harbour_crane():
    """A ship-to-shore container crane on rails along the quay: two portal frames 16 m apart
    (across the quay, X) and 12 m between them along it (Z), a machinery house, and the boom at
    32 m - out 40 m over the water (+X) and back 18 m over the quay - with its trolley and
    spreader. Red and white."""
    b = Builder()
    b.material("red", flat((196, 50, 36), 3), 2, 2)
    b.material("white", flat((232, 232, 226), 3), 2, 2)
    b.material("dark", flat((70, 74, 80), 3), 2, 2)
    b.material("glass", flat((50, 70, 88), 2), 1, 1)
    g, s, top = 8.0, 6.0, 30.0
    for x in (-g, g):
        for z in (-s, s):
            b.solid((x - .7, 0, z - .7), (x + .7, top, z + .7), "red")
        b.solid((x - .9, 0, -s - 1.5), (x + .9, 1.2, s + 1.5), "dark")            # the bogies
        b.solid((x - .6, 12, -s), (x + .6, 13.2, s), "red")                         # portal ties
    for z in (-s, s):
        b.solid((-g - .7, top - 1.4, z - .7), (g + .7, top, z + .7), "red")
    b.solid((-g - 2, top, -s - 1), (-g + 6, top + 4, s + 1), "white")            # machinery house
    for z in (-1.4, 1.4):
        b.solid((-18, top + 1.2, z - .5), (40, top + 2.6, z + .5), "white")       # the boom
    b.solid((-18, top + 2.6, -1.9), (-14, top + 3.4, 1.9), "dark")
    b.solid((-3, top + 2.6, -.3), (-2.4, top + 12, .3), "red")                    # the A-frame
    for zz in (-1.4, 1.4):
        b.solid((-2.7, top + 11.4, zz - .2), (34, top + 11.7, zz + .2), "dark")   # stays (simplified)
    b.solid((20, top - 1.2, -2.2), (24, top + 1.2, 2.2), "white")                 # the trolley and cab
    b.solid((20.2, top - 1.0, -2.25), (23.8, top - .2, 2.25), "glass")
    for zz in (-1.6, 1.6):
        b.solid((21.9, top - 14, zz - .05), (22.1, top - 1.2, zz + .05), "dark")  # hoist ropes
    b.solid((19, top - 15, -6), (25, top - 14, 6), "dark")                        # the spreader
    return b


def cattle_gangway():
    """The cattle gangway up to a livestock ship's side: an 18 m ramp along +X rising 2.4 m, 2.4 m
    wide with high slatted sides, on two trestles; the foot at x = 0, the top at x = 18."""
    b = Builder()
    b.material("steel", flat((120, 126, 130), 3), 2, 2)
    b.material("deck", flat((96, 90, 80), 8), 1, 1)
    run, rise, w = 18.0, 2.4, 1.2
    n = 18
    for k in range(n):
        x0, x1 = run * k / n, run * (k + 1) / n
        y = rise * (k + .5) / n
        b.solid((x0, y, -w), (x1, y + .15, w), "deck")
        for z in (-w - .1, w):
            for h in (.6, 1.1, 1.6):
                b.solid((x0, y + h, z), (x1, y + h + .1, z + .1), "steel")
            b.solid((x0, y, z), (x0 + .1, y + 1.7, z + .1), "steel")
    for x in (6.0, 13.0):
        b.solid((x - .2, 0, -w), (x + .2, rise * x / run, w), "steel")
    return b


def truck_depot():
    """Verda Transporto's depot: a steel-clad truck shed (36 x 22 m, 9 m to the eaves) with three
    roller doors in the front (-Z), and the two-storey office across the middle of the front with
    its door, windows and a canopy. The game hangs the company's sign over the office."""
    b = Builder()
    b.material("clad", stripes((176, 184, 190), (150, 158, 166), 6, 71), 1.0, 1.0)
    b.material("office", window_tile((214, 208, 196), (110, 114, 120), (60, 86, 110)), BAY, FLOOR)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("door", stripes((120, 130, 140), (100, 108, 118), 3, 72), 1.0, 1.0)
    b.material("slab", flat((150, 148, 142), 6), 3, 3)
    b.material("green", flat((16, 128, 70), 2), 1, 1)
    b.material("steel", flat((90, 96, 100), 3), 2, 2)
    b.material("glass", flat((60, 86, 110), 2), 1, 1)
    b.material("yellow", flat((230, 190, 40), 2), 1, 1)
    w, d, h = 18.0, 11.0, 9.0
    b.box((-w, 0, -d), (w, h, d), {'+x': "clad", '-x': "clad", '+z': "clad", '-z': "clad", '+y': "roof"}, v_origin=h)
    b.solid((-w - .1, h - .6, -d - .1), (w + .1, h, d + .1), "green")             # the eaves band
    for x in (-15.0, 9.0):
        b.solid((x, 0, -d - .06), (x + 6, 5.5, -d), "door")                        # roller doors
        b.solid((x - .2, 0, -d - .3), (x, 1.0, -d), "yellow")                      # bollards
        b.solid((x + 6, 0, -d - .3), (x + 6.2, 1.0, -d), "yellow")
    # The office: two storeys across the middle of the front.
    b.box((-6, 0, -d - 5), (6, 7.0, -d), {'+x': "office", '-x': "office", '-z': "office", '+y': "roof"}, v_origin=7.0)
    b.solid((-1.2, 0, -d - 5.06), (1.2, 2.4, -d - 5.0), "glass")                  # the door
    b.solid((-3, 2.9, -d - 7), (3, 3.1, -d - 5), "steel")                          # canopy
    for x in (-2.9, 2.7):
        b.solid((x, 0, -d - 6.9), (x + .2, 2.9, -d - 6.7), "steel")
    b.solid((-6.1, 6.6, -d - 5.1), (6.1, 7.0, -d), "green")
    b.box((-w - 1, 0, -d - 9), (w + 1, .1, -d), {'+y': "slab"})                     # the apron
    return b


RADIO_STYLES = {
    # walls, accent, mast height, porch (timber, country), dishes on the roof
    "popolko": dict(walls=("render", (236, 228, 206)), accent=(16, 128, 70), mast=30.0, porch=False, dishes=1),
    "kantrio": dict(walls=("wood", None), accent=(122, 74, 36), mast=24.0, porch=True, dishes=0),
    "roko": dict(walls=("render", (54, 54, 58)), accent=(176, 30, 34), mast=36.0, porch=False, dishes=2),
}


def lattice_mast(b, x, z, height, width=1.4):
    """A square lattice transmitter mast in red and white bands, a whip on top and its warning lamp."""
    h = width / 2
    section = 4.0
    y = 0.0
    k = 0
    while y < height - 1e-3:
        top = min(height, y + section)
        paint = "mast_red" if k % 2 == 0 else "mast_white"
        for sx in (-1, 1):
            for sz in (-1, 1):
                b.solid((x + sx * h - .06, y, z + sz * h - .06), (x + sx * h + .06, top, z + sz * h + .06), paint)
        rung = y + 1.0
        while rung < top - .1:
            b.solid((x - h, rung, z - h - .03), (x + h, rung + .06, z - h + .03), paint)
            b.solid((x - h, rung, z + h - .03), (x + h, rung + .06, z + h + .03), paint)
            b.solid((x - h - .03, rung, z - h), (x - h + .03, rung + .06, z + h), paint)
            b.solid((x + h - .03, rung, z - h), (x + h + .03, rung + .06, z + h), paint)
            rung += 1.33
        y = top
        k += 1
    b.solid((x - .08, height, z - .08), (x + .08, height + 4, z + .08), "steel")
    b.solid((x - .2, height + 4, z - .2), (x + .2, height + 4.35, z + .2), "lamp_red")
    for side in (-1, 1):   # the feeder dishes half way up
        b.solid((x + side * (h + .1) - .05, height * .55, z - .5), (x + side * (h + .1) + .05, height * .55 + 1, z + .5), "steel")
    b.solid((x - h - .4, 0, z - h - .4), (x + h + .4, .4, z + h + .4), "concrete")


def radio_station(style):
    """A radio station (modular, one storey, enterable): the studio block - a glass door, a long
    studio window and a red ON AIR lamp at the front (-Z), the sign band in the station's colour,
    the mixing desk and the studio wall inside - beside a lattice transmitter mast in red and
    white. Modules per station: walls and colour, the mast's height, a timber porch (the
    country station) and the satellite dishes on the roof. 12 x 9 m, the mast on the +X side."""
    st = RADIO_STYLES[style]
    b = Builder()
    kind, colour = st["walls"]
    if kind == "wood":
        b.material("walls", wood(), 2, 2)
    else:
        b.material("walls", flat(colour, 4), 3, 3)
    b.material("accent", flat(st["accent"], 3), 3, 1)
    b.material("plaster", flat((214, 210, 200), 3), 3, 3)
    b.material("floor", flat((96, 92, 88), 6), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("steel", flat((120, 124, 130), 3), 3, 3)
    b.material("concrete", flat((150, 148, 142), 6), 3, 3)
    b.material("desk", flat((34, 36, 40), 3), 1, 1)
    b.material("panel", checker((70, 74, 80), (46, 48, 52), 6, 61), 1, 1)
    b.material("lamp_red", flat((235, 40, 40), 0), 1, 1)
    b.material("mast_red", flat((206, 52, 40), 2), 1, 1)
    b.material("mast_white", flat((236, 236, 232), 2), 1, 1)
    b.material("dish", flat((226, 228, 230), 2), 1, 1)
    b.material("timber", wood(), 2, 2)
    w, d, height, t, slab = 6.0, 4.5, 4.0, .25, .2
    front = [(-4.6, -3.2, slab, 2.5), (-2.2, 4.8, 1.0, 2.6)]           # the door, the long studio window
    back = [(-4.0, -2.0, 1.4, 2.5), (2.0, 4.0, 1.4, 2.5)]
    sides = [(-1.5, 1.5, 1.2, 2.5)]
    wall_with_openings(b, 'x', -d, (-w, w), height, t, front, "walls", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), height, t, back, "walls", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), height, t, sides, "walls", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), height, t, sides, "walls", "plaster", +1)
    b.box((-w + t, 0, -d + t), (w - t, slab, d - t), {'+y': "floor", '-y': "plaster"})
    b.box((-w, height - .2, -d), (w, height, d), {'+y': "roof", '-y': "plaster", '+x': "accent", '-x': "accent", '+z': "accent", '-z': "accent"})
    b.solid((-w - .05, 0, -d - .05), (w + .05, .45, d + .05), "accent")
    b.solid((-w - .1, height - .85, -d - .15), (w + .1, height - .2, -d), "accent")     # the sign band
    for lo, hi in (((-w, height, -d), (w, height + .4, -d + .15)), ((-w, height, d - .15), (w, height + .4, d)),
                   ((-w, height, -d), (-w + .15, height + .4, d)), ((w - .15, height, -d), (w, height + .4, d))):
        b.solid(lo, hi, "walls")
    # ON AIR over the door.
    b.solid((-4.4, 2.75, -d - .22), (-3.4, 3.05, -d), "lamp_red")
    # Inside: the studio wall with its window, the mixing desk facing the window, speakers.
    b.solid((-w + t, slab, .8), (-1.0, 2.8, .95), "plaster")
    b.solid((1.0, slab, .8), (w - t, 1.0, .95), "plaster")
    b.solid((1.0, 2.4, .8), (w - t, 2.8, .95), "plaster")
    b.solid((-1.4, slab, -1.6), (3.6, .85, -.7), "desk")
    b.solid((-1.3, .85, -1.0), (3.5, .95, -.75), "panel")
    for x in (-1.2, 3.1):
        b.solid((x, .95, -1.0), (x + .4, 1.5, -.7), "desk")
    if st["porch"]:
        # A timber porch along the front, on posts, with a step.
        b.solid((-w - .3, 2.9, -d - 2.2), (w + .3, 3.05, -d), "timber")
        for x in (-w, -1.0, 3.0, w - .2):
            b.solid((x, slab, -d - 2.1), (x + .2, 2.9, -d - 1.9), "timber")
        b.solid((-w - .3, 0, -d - 2.2), (w + .3, slab, -d), "timber")
    for k in range(st["dishes"]):
        cx, cz = -3.6 + k * 2.6, 2.4
        b.solid((cx - .1, height, cz - .1), (cx + .1, height + .9, cz + .1), "steel")
        b.dome("dish", cx, cz, 1.0 if k == 0 else .7, height + .9, courses=4, sides=16, rise=.35)
    lattice_mast(b, w + 2.2, 1.5, st["mast"])
    # The guy-line anchors and the fence round the mast's foot.
    b.solid((w + .6, 0, -.1), (w + 3.8, .9, -.04), "steel")
    b.solid((w + .6, 0, 3.04), (w + 3.8, .9, 3.1), "steel")
    b.solid((w + 3.74, 0, -.1), (w + 3.8, .9, 3.1), "steel")
    return b


def gun_shop():
    """Armilejo, Verda's gun shop chain (modular, one storey, enterable): olive render over a
    dark plinth, a brass sign band, a glass door and two barred display windows at the front
    (-Z); inside, the long counter with its glass case, rifles racked on the back wall, shelves
    of ammunition along one side and a stand of vests. 12 x 9 m."""
    b = Builder()
    b.material("render", flat((92, 104, 78), 4), 3, 3)
    b.material("plinth", flat((40, 42, 38), 4), 3, 1)
    b.material("brass", flat((196, 160, 70), 3), 3, 1)
    b.material("plaster", flat((206, 202, 190), 3), 3, 3)
    b.material("floor", flat((70, 66, 60), 6), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("steel", flat((60, 62, 66), 3), 2, 2)
    b.material("glass", flat((70, 100, 120), 2), 1, 1)
    b.material("wood", wood(), 2, 2)
    b.material("gun", flat((30, 30, 32), 2), 1, 1)
    b.material("stock", flat((110, 70, 40), 3), 1, 1)
    b.material("ammo", stripes((150, 120, 50), (60, 90, 50), 4, 81), 1, 1)
    b.material("vest", flat((70, 80, 60), 4), 1, 1)
    w, d, height, t, slab = 6.0, 4.5, 4.0, .25, .2
    front = [(-1.0, 1.0, slab, 2.5), (-5.2, -2.0, .9, 2.5), (2.0, 5.2, .9, 2.5)]   # door, two display windows
    wall_with_openings(b, 'x', -d, (-w, w), height, t, front, "render", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), height, t, [], "render", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), height, t, [], "render", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), height, t, [(-.5, .5, slab, 2.3)], "render", "plaster", +1)   # the back door
    b.box((-w + t, 0, -d + t), (w - t, slab, d - t), {'+y': "floor", '-y': "plaster"})
    b.box((-w, height - .2, -d), (w, height, d), {'+y': "roof", '-y': "plaster", '+x': "plinth", '-x': "plinth", '+z': "plinth", '-z': "plinth"})
    b.solid((-w - .05, 0, -d - .05), (w + .05, .5, d + .05), "plinth")
    b.solid((-w - .1, height - .9, -d - .15), (w + .1, height - .2, -d), "brass")           # the sign band
    for lo, hi in (((-w, height, -d), (w, height + .4, -d + .15)), ((-w, height, d - .15), (w, height + .4, d)),
                   ((-w, height, -d), (-w + .15, height + .4, d)), ((w - .15, height, -d), (w, height + .4, d))):
        b.solid(lo, hi, "render")
    # Bars over the display windows, and a rifle or two on show behind each.
    for x0, x1 in ((-5.2, -2.0), (2.0, 5.2)):
        x = x0 + .2
        while x < x1:
            b.solid((x, .9, -d - .08), (x + .05, 2.5, -d - .03), "steel")
            x += .35
        b.solid((x0 + .3, 1.3, -d + .35), (x1 - .3, 1.36, -d + .45), "gun")
        b.solid((x0 + .3, 1.27, -d + .35), (x0 + 1.0, 1.4, -d + .45), "stock")
    # The counter with its glass case, the till.
    b.solid((-4.5, slab, .3), (3.5, 1.0, 1.0), "wood")
    b.solid((-4.4, 1.0, .35), (3.4, 1.05, .95), "glass")
    b.solid((2.6, 1.05, .5), (3.1, 1.3, .8), "steel")
    # Rifles racked on the back wall: barrels and stocks in rows.
    for row, y in enumerate((1.5, 2.0, 2.5)):
        for k in range(8):
            x = -4.8 + k * 1.2 + (row % 2) * .3
            b.solid((x, y, d - t - .12), (x + .9, y + .06, d - t - .05), "gun")
            b.solid((x, y - .04, d - t - .12), (x + .3, y + .1, d - t - .05), "stock")
    # Ammunition on shelves down the left wall; vests on a stand at the right.
    for y in (.6, 1.2, 1.8):
        b.solid((-w + t, y, -2.5), (-w + t + .45, y + .05, 2.5), "wood")
        for k in range(10):
            z = -2.3 + k * .48
            b.solid((-w + t + .05, y + .05, z), (-w + t + .4, y + .3, z + .3), "ammo")
    for k in range(3):
        z = -2.0 + k * 1.1
        b.solid((w - t - .5, slab, z), (w - t - .45, 1.7, z + .05), "steel")
        b.solid((w - t - .75, 1.0, z - .25), (w - t - .2, 1.6, z + .3), "vest")
    return b


# ---------------------------------------------------------------- the motor trade
# Marnafto (the island's oil company: its rigs out at sea, its stations on the roads), the
# Meĥanikejo workshops and Aŭtovendejo Verda's car lot. Keep the layout numbers in step with
# include/outland/world/MotorTrade.hpp (the game finds the pumps, bays and display spots by them).
MARNAFTO_BLUE, MARNAFTO_ORANGE = (18, 54, 112), (238, 122, 26)


def marnafto_logo(seed=211):
    """Marnafto's board: an orange drop on deep blue over a white rig on the sea line."""
    c = Canvas(color=MARNAFTO_BLUE, seed=seed)
    for y in range(TEX):
        for x in range(TEX):
            dx, dy = x - 32, y - 26
            # The drop: a circle with a point on top.
            if dx * dx + (dy - 4) * (dy - 4) < 15 * 15 or (dy < 0 and abs(dx) < (dy + 22) * .7 and dy > -22):
                c.px[y * TEX + x] = MARNAFTO_ORANGE
    c.rect(0, 52, TEX, 54, (240, 240, 236))             # the sea line
    c.rect(26, 40, 38, 44, (240, 240, 236))             # the rig: deck, legs, derrick
    for x in (27, 36):
        c.rect(x, 44, x + 2, 52, (240, 240, 236))
    c.rect(31, 32, 33, 40, (240, 240, 236))
    return c


def gas_station():
    """A Marnafto filling station (modular, front faces -Z): a concrete forecourt, a canopy on four
    columns over two pump islands (two pumps each), the kiosk shop at the back (glass front,
    counter, shelves; enterable), a price pylon with the drop-and-rig logo. 24 x 22 m. Cars drive
    under the canopy and pull up beside the pumps."""
    b = Builder()
    b.material("concrete", flat((176, 174, 166), 6), 3, 3)
    b.material("white", flat((236, 236, 232), 3), 3, 3)
    b.material("blue", flat(MARNAFTO_BLUE, 3), 3, 1)
    b.material("orange", flat(MARNAFTO_ORANGE, 3), 3, 1)
    b.material("logo", marnafto_logo(), 2.4, 2.4)
    b.material("steel", flat((150, 154, 160), 3), 2, 2)
    b.material("dark", flat((40, 42, 46), 3), 1, 1)
    b.material("glass", flat((70, 104, 124), 2), 1, 1)
    b.material("plaster", flat((214, 212, 204), 3), 3, 3)
    b.material("floor", checker((214, 214, 206), (180, 180, 172)), 2, 2)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("goods", stripes((200, 60, 40), (60, 120, 180), 6, 213), 1, 1)
    b.material("hose", flat((24, 24, 26), 2), 1, 1)
    # The forecourt.
    b.box((-12, 0, -11), (12, .06, 5), {'+y': "concrete", '-z': "concrete", '+x': "concrete", '-x': "concrete"})
    # The canopy: x -8..8, z -9..1, its underside 5 m up, a blue fascia with an orange stripe.
    b.box((-8, 5.0, -9), (8, 5.9, 1), {'-y': "white", '+y': "roof", '-z': "blue", '+z': "blue", '+x': "blue", '-x': "blue"})
    for z0, z1, side in ((-9.02, -9.0, '-z'), (1.0, 1.02, '+z')):
        b.box((-8, 5.3, z0), (8, 5.5, z1), {side: "orange"})
    for x in (-6.5, 6.5):
        for z in (-7.5, -.5):
            b.solid((x - .25, .06, z - .25), (x + .25, 5.0, z + .25), "white")
    # The pump islands (x = +-3.5, z -6..-2), two pumps each.
    for x in GAS_ISLANDS:
        b.solid((x - .6, .06, -6.2), (x + .6, .22, -1.8), "concrete")
        for z in GAS_PUMPS_Z:
            b.solid((x - .35, .22, z - .25), (x + .35, 1.75, z + .25), "white")
            b.box((x - .36, 1.2, z - .26), (x + .36, 1.7, z + .26), {'+x': "blue", '-x': "blue", '+z': "blue", '-z': "blue"})
            b.box((x - .37, 1.75, z - .27), (x + .37, 1.95, z + .27), {s: "orange" for s in ('+x', '-x', '+z', '-z', '+y')})
            for sx in (-1, 1):
                b.solid((x + sx * .36, .7, z - .05), (x + sx * .42, 1.3, z + .05), "hose")
        b.solid((x - .15, .22, -1.95), (x + .15, 1.0, -1.85), "steel")      # bollard
    # The kiosk shop: x -6..6, z 5..11, a glass front with a door in the middle.
    w0, w1, z0, z1, h, t, slab = -6.0, 6.0, 5.0, 11.0, 3.6, .25, .15
    front = [(-1.0, 1.0, slab, 2.4), (-5.4, -1.6, .9, 2.6), (1.6, 5.4, .9, 2.6)]
    wall_with_openings(b, 'x', z0, (w0, w1), h, t, front, "white", "plaster", -1)
    wall_with_openings(b, 'x', z1, (w0, w1), h, t, [], "white", "plaster", +1)
    wall_with_openings(b, 'z', w0, (z0 + t, z1 - t), h, t, [], "white", "plaster", -1)
    wall_with_openings(b, 'z', w1, (z0 + t, z1 - t), h, t, [], "white", "plaster", +1)
    b.box((w0 + t, 0, z0 + t), (w1 - t, slab, z1 - t), {'+y': "floor", '-y': "plaster"})
    b.box((w0, h - .2, z0), (w1, h, z1), {'+y': "roof", '-y': "plaster", '+x': "white", '-x': "white", '+z': "white", '-z': "white"})
    b.box((w0 - .05, h - .8, z0 - .12), (w1 + .05, h, z0), {'-z': "blue"})
    for x0, x1 in ((-5.4, -1.6), (1.6, 5.4)):
        b.solid((x0, .9, z0 + .1), (x1, 2.6, z0 + .13), "glass")
    b.solid((2.0, slab, 8.6), (5.4, 1.0, 9.4), "white")                     # the counter
    b.solid((2.0, 1.0, 8.6), (5.4, 1.05, 9.4), "dark")
    for x in (-4.6, -2.2):
        b.solid((x, slab, 6.4), (x + 1.4, 1.7, 9.6), "goods")                # shelves
    # The price pylon at the front corner: a pole and the board with the logo.
    px, pz = 10.6, -9.6
    b.solid((px - .2, .06, pz - .2), (px + .2, 3.2, pz + .2), "steel")
    b.box((px - 1.3, 3.2, pz - .25), (px + 1.3, 6.2, pz + .25), {'-z': "logo", '+z': "logo", '+x': "blue", '-x': "blue", '+y': "blue", '-y': "blue"})
    b.box((px - 1.3, 2.6, pz - .26), (px + 1.3, 3.2, pz + .26), {'-z': "orange", '+z': "orange"})
    return b


def mechanic_shop():
    """A Meĥanikejo (modular workshop, front faces -Z): two drive-in bays behind wide door
    openings (x -5 and +1), lifts, benches and tyre stacks, an office on the +X side with its own
    door, a red sign band. 18 x 12 m, 5 m high. Drive in and the mechanic comes over."""
    b = Builder()
    b.material("render", flat((198, 190, 172), 4), 3, 3)
    b.material("red", flat((172, 32, 30), 3), 3, 1)
    b.material("plaster", flat((190, 188, 182), 3), 3, 3)
    b.material("floor", flat((110, 110, 106), 8), 3, 3)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("steel", flat((126, 130, 136), 3), 2, 2)
    b.material("yellow", stripes((222, 182, 30), (30, 30, 30), 6, 217), 1, 1)
    b.material("tyre", flat((28, 28, 30), 2), 1, 1)
    b.material("wood", wood(), 2, 2)
    b.material("drum", flat((40, 90, 150), 3), 1, 1)
    w, d, h, t, slab = 9.0, 6.0, 5.0, .3, .1
    doors = [(c - MECH_DOOR / 2, c + MECH_DOOR / 2, 0, 4.0) for c in MECH_BAYS]
    front = doors + [(6.0, 7.2, slab, 2.4), (7.6, 8.6, 1.0, 2.4)]           # the office door and window
    wall_with_openings(b, 'x', -d, (-w, w), h, t, front, "render", "plaster", -1)
    wall_with_openings(b, 'x', d, (-w, w), h, t, [(-2.0, 0, 1.2, 2.6)], "render", "plaster", +1)
    wall_with_openings(b, 'z', -w, (-d + t, d - t), h, t, [(-1.0, 1.0, 1.4, 2.6)], "render", "plaster", -1)
    wall_with_openings(b, 'z', w, (-d + t, d - t), h, t, [], "render", "plaster", +1)
    # The office partition (x = 4.5) with a door into the workshop.
    wall_with_openings(b, 'z', 4.5, (-d + t, d - t), 3.2, .15, [(1.5, 2.5, slab, 2.3)], "plaster", "plaster", -1)
    b.box((-w + t, 0, -d + t), (w - t, slab, d - t), {'+y': "floor", '-y': "plaster"})
    b.box((-w, h - .25, -d), (w, h, d), {'+y': "roof", '-y': "plaster", '+x': "render", '-x': "render", '+z': "render", '-z': "render"})
    b.box((-w - .05, 4.1, -d - .15), (w + .05, 4.9, -d), {'-z': "red", '-y': "red", '+x': "red", '-x': "red"})
    # Yellow-and-black edges on the door jambs; a lift in each bay (two posts and arms).
    for lo, hi in [((c - MECH_DOOR / 2 - .12, 0, -d - .05), (c - MECH_DOOR / 2, 4.0, -d + t)) for c in MECH_BAYS] + \
                  [((c + MECH_DOOR / 2, 0, -d - .05), (c + MECH_DOOR / 2 + .12, 4.0, -d + t)) for c in MECH_BAYS]:
        b.solid(lo, hi, "yellow")
    for c in MECH_BAYS:
        for sx in (-1, 1):
            b.solid((c + sx * 2.1 - .15, slab, -.3), (c + sx * 2.1 + .15, 3.4, .3), "steel")
            b.solid((c + sx * 2.1 - .1, .5, -1.4), (c + sx * 1.3, .6, 1.4), "steel")
    # Benches and tool boards on the back wall, tyre stacks, oil drums.
    for x0 in (-8.4, -2.4):
        b.solid((x0, slab, d - t - .8), (x0 + 3.0, .95, d - t - .05), "wood")
        b.solid((x0, 1.4, d - t - .06), (x0 + 3.0, 2.4, d - t - .02), "steel")
    for k in range(4):
        x = -8.4 + (k % 2) * .9
        z = -4.6 + (k // 2) * .9
        for level in range(3):
            b.solid((x - .35, slab + level * .26, z - .35), (x + .35, slab + level * .26 + .24, z + .35), "tyre")
    for k in range(3):
        b.solid((3.2 - k * .7, slab, 4.6), (3.8 - k * .7, 1.0, 5.2), "drum")
    # The office: a desk and a counter.
    b.solid((6.2, slab, 2.5), (8.2, .8, 3.5), "wood")
    b.solid((5.0, slab, -3.0), (5.6, 1.0, 1.0), "wood")
    return b


def car_lot():
    """Aŭtovendejo Verda (front faces -Z): a paved lot (36 x 26 m) with white bay lines, two
    rows of display bays, a glass sales office at the back on +X (desk, chairs; enterable),
    bunting on poles round the edge, and a tall sign. The cars on show are the game's own."""
    b = Builder()
    b.material("asphalt", flat((62, 64, 66), 6), 3, 3)
    b.material("line", flat((236, 236, 232), 2), 1, 1)
    b.material("white", flat((236, 236, 232), 3), 3, 3)
    b.material("plaster", flat((212, 212, 206), 3), 3, 3)
    b.material("glass", flat((76, 112, 136), 2), 1, 1)
    b.material("floor", checker((220, 220, 214), (190, 190, 184)), 2, 2)
    b.material("roof", gravel_roof(), 3, 3)
    b.material("green", flat((22, 118, 64), 3), 3, 1)
    b.material("steel", flat((150, 154, 160), 3), 2, 2)
    b.material("flag_a", flat((220, 40, 40), 2), 1, 1)
    b.material("flag_b", flat((240, 200, 40), 2), 1, 1)
    b.material("flag_c", flat((30, 110, 200), 2), 1, 1)
    b.material("wood", wood(), 2, 2)
    hx, hz = 18.0, 13.0
    b.box((-hx, 0, -hz), (hx, .06, hz), {'+y': "asphalt", '-z': "asphalt", '+z': "asphalt", '+x': "asphalt", '-x': "asphalt"})
    # Bay lines between the display spots (rows at z = -6 and z = 3, cars 6 m apart).
    for z in CAR_LOT_ROWS:
        for x in [CAR_LOT_SPOTS_X[0] - 3] + [sx + 3 for sx in CAR_LOT_SPOTS_X]:
            b.solid((x - .06, .06, z - 2.6), (x + .06, .07, z + 2.6), "line")
    # The sales office: x 9..16, z 6..11.
    x0, x1, z0, z1, h, t, slab = 9.0, 16.0, 6.0, 11.0, 3.4, .2, .12
    wall_with_openings(b, 'x', z0, (x0, x1), h, t, [(9.6, 11.6, .8, 2.7), (12.2, 13.4, slab, 2.4), (14.0, 15.6, .8, 2.7)], "white", "plaster", -1)
    wall_with_openings(b, 'x', z1, (x0, x1), h, t, [], "white", "plaster", +1)
    wall_with_openings(b, 'z', x0, (z0 + t, z1 - t), h, t, [(7.0, 10.0, .8, 2.7)], "white", "plaster", -1)
    wall_with_openings(b, 'z', x1, (z0 + t, z1 - t), h, t, [], "white", "plaster", +1)
    b.box((x0 + t, 0, z0 + t), (x1 - t, slab, z1 - t), {'+y': "floor", '-y': "plaster"})
    b.box((x0 - .4, h - .2, z0 - .8), (x1 + .4, h, z1 + .4), {'+y': "roof", '-y': "plaster", '+x': "green", '-x': "green", '+z': "green", '-z': "green"})
    for xa, xb in ((9.6, 11.6), (14.0, 15.6)):
        b.solid((xa, .8, z0 + .08), (xb, 2.7, z0 + .1), "glass")
    b.solid((12.4, slab, 8.4), (14.8, .8, 9.2), "wood")                      # the salesman's desk
    # Bunting: poles round the front and sides, strings of pennants between them.
    poles = [(-hx + .4, -hz + .4), (-6.0, -hz + .4), (6.0, -hz + .4), (hx - .4, -hz + .4), (hx - .4, 0.0), (-hx + .4, 0.0), (-hx + .4, hz - .4)]
    for x, z in poles:
        b.solid((x - .08, .06, z - .08), (x + .08, 5.0, z + .08), "steel")
    for (xa, za), (xb, zb) in zip(poles, poles[1:]):
        n = max(2, int(math.hypot(xb - xa, zb - za) / .8))
        for k in range(n):
            u = (k + .5) / n
            x, z = xa + (xb - xa) * u, za + (zb - za) * u
            y = 4.8 - .9 * math.sin(math.pi * u)
            b.solid((x - .14, y - .4, z - .14), (x + .14, y, z + .14), ("flag_a", "flag_b", "flag_c")[k % 3])
    # The sign: two posts and a green board by the entrance.
    for x in (-17.2, -14.8):
        b.solid((x - .12, .06, -hz - .6), (x + .12, 6.0, -hz - .36), "steel")
    b.solid((-17.6, 4.2, -hz - .62), (-14.4, 6.4, -hz - .34), "green")
    return b


def oil_rig():
    """One of Marnafto's offshore rigs (Platformoj Marnafto): four legs out of the sea to a two-deck
    platform 30 x 30 m, a derrick, accommodation block, helideck, crane and flare boom, in the
    company's blue and orange. The model's y = 0 is the sea's surface; the legs go 14 m under."""
    b = Builder()
    b.material("steel", flat((120, 124, 130), 4), 2, 2)
    b.material("yellow", flat((222, 182, 30), 3), 2, 2)
    b.material("blue", flat(MARNAFTO_BLUE, 3), 3, 3)
    b.material("orange", flat(MARNAFTO_ORANGE, 3), 3, 3)
    b.material("white", flat((228, 228, 224), 3), 3, 3)
    b.material("deck", flat((70, 72, 74), 6), 3, 3)
    b.material("flame", flat((255, 170, 40), 0), 1, 1)
    for x in (-12, 12):
        for z in (-12, 12):
            b.ring("yellow", x, z, 1.6, 1.6, -14, 18, 12)
    for y in (18.0, 24.0):
        b.box((-15, y, -15), (15, y + 1.2, 15), {'+y': "deck", '-y': "steel", '+x': "steel", '-x': "steel", '+z': "steel", '-z': "steel"})
    for x in (-15, 15):
        for z in (-15, 15):
            b.solid((x - .3, 19.2, z - .3), (x + .3, 24, z + .3), "steel")
    # The derrick: a tapering lattice tower (four corner posts and rungs).
    for k in range(10):
        y0, y1 = 25.2 + k * 4, 25.2 + (k + 1) * 4
        r0, r1 = 3.2 - k * .26, 3.2 - (k + 1) * .26
        for sx in (-1, 1):
            for sz in (-1, 1):
                b.solid((4 + sx * r1 - .18, y0, -4 + sz * r1 - .18), (4 + sx * r1 + .18, y1, -4 + sz * r1 + .18), "white")
        b.solid((4 - r1, y1 - .2, -4 - r1), (4 + r1, y1, -4 - r1 + .3), "orange")
    # Accommodation block, helideck on top.
    b.box((-14, 25.2, 4), (-2, 33, 14), {s: "blue" for s in ('+x', '-x', '+z', '-z')} | {'+y': "deck"})
    b.box((-15, 33, 2), (-1, 33.4, 15), {'+y': "orange", '-y': "steel", '+x': "white", '-x': "white", '+z': "white", '-z': "white"})
    # The crane and the flare boom (with its flame) out over the sea.
    b.solid((10, 25.2, 8), (12, 32, 10), "yellow")
    b.solid((10.5, 31, 9), (24, 31.8, 9.8), "yellow")
    b.solid((-15, 26, -12), (-34, 27, -11), "steel")
    b.solid((-35.5, 25.5, -12.8), (-33, 30, -10.2), "flame")
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
    add("civic/cargo_ship.glb", cargo_ship(), "city_cargo_ship", "Stella Maris - feeder container ship (moored)", "BuildingPart", 16, 96, 32)
    add("civic/quay_apron.glb", quay_apron(), "city_quay_apron", "Concrete quay apron with quay wall and crane rails", "BuildingPart", 60.5, 112, 14)
    add("civic/harbour_crane.glb", harbour_crane(), "city_harbour_crane", "Ship-to-shore container crane on quay rails", "BuildingPart", 58, 15, 42)
    add("civic/container_stack.glb", container_stack(), "city_container_stack", "Stacked shipping containers (40 ft)", "BuildingPart", 10.5, 37.5, 7.8)
    add("civic/cattle_gangway.glb", cattle_gangway(), "city_cattle_gangway", "Cattle gangway up a ship's side", "BuildingPart", 18, 2.6, 4.2)
    add("civic/truck_depot.glb", truck_depot(), "city_truck_depot", "Verda Transporto - truck depot with office", "Building", 38, 31, 9)
    add("civic/gun_shop.glb", gun_shop(), "city_gun_shop", "Armilejo - gun shop (modular, enterable)", "Building", 12.2, 9.2, 4.4)
    for style, name in (("popolko", "Verda Popolko 98.1"), ("kantrio", "Radio Sovaĝejo 102.5"), ("roko", "Espera Roko 105.9")):
        mast = RADIO_STYLES[style]["mast"]
        add(f"civic/radio_station_{style}.glb", radio_station(style), f"city_radio_station_{style}",
            f"Radio station - {name} (modular studio, transmitter mast)", "Building", 18, 13.4 if RADIO_STYLES[style]["porch"] else 11.2, mast + 4.35)
    add("civic/gas_station.glb", gas_station(), "city_gas_station", "Marnafto - filling station (modular, drive-in canopy, kiosk shop)", "Building", 24, 22, 6.2)
    add("civic/mechanic_shop.glb", mechanic_shop(), "city_mechanic_shop", "Meĥanikejo - mechanic's workshop (modular, two drive-in bays, office)", "Building", 18.1, 12.15, 5.0)
    add("civic/car_lot.glb", car_lot(), "city_car_lot", "Aŭtovendejo Verda - car lot with sales office and bunting", "Building", 36, 26.62, 6.4)
    add("civic/oil_rig.glb", oil_rig(), "city_oil_rig", "Platformo Marnafto - offshore oil rig", "BuildingPart", 59.5, 30.6, 79.2)
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
