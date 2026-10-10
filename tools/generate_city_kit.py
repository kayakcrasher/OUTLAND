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


def wall_with_openings(b, axis, fixed, span, height, thickness, openings, outer, inner, outward):
    """A wall along `axis` ('x' or 'z') at coordinate `fixed`, from span[0] to span[1] and 0..height,
    split into boxes around rectangular openings [(a0, a1, y0, y1)]. `outward` is +1/-1 (outer face)."""
    cuts = sorted({span[0], span[1], *[c for o in openings for c in (o[0], o[1])]})
    t0, t1 = (fixed, fixed + thickness) if outward < 0 else (fixed - thickness, fixed)
    for a0, a1 in zip(cuts, cuts[1:]):
        if a1 - a0 < 1e-4:
            continue
        holes = sorted((o[2], o[3]) for o in openings if o[0] <= a0 + 1e-4 and o[1] >= a1 - 1e-4)
        y = 0.0
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
    add("ground/sidewalk_block_68.glb", pad(68, "sidewalk"), "city_sidewalk_block_68", "Sidewalk Block 68 m", "BuildingPart", 68, 68, .15)
    add("ground/courthouse_square_68.glb", pad(68, "park"), "city_courthouse_square_68", "Courthouse Square Park 68 m", "BuildingPart", 68, 68, 4.6)
    add("ground/parking_lot_68.glb", pad(68, "parking"), "city_parking_lot_68", "Parking Lot 68 m", "BuildingPart", 68, 68, .15)
    add("ground/intersection_12.glb", intersection(12), "city_intersection_12", "Street Intersection 12 m (crosswalks)", "Road", 12, 12, .1)
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
