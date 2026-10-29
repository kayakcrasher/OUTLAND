#!/usr/bin/env python3
"""Generate Verda's furniture (assets/verda/furniture/*.glb), built like the city kit.

The pubs, some homes and Vestaĵejo Verda (the clothing store) are furnished with these at runtime
(game::interiors::Furnishing); each is also in the Creator catalog (FurnitureAssetCatalog.hpp).

  pub:     bar_counter (with the back bar), bar_stool, pub_table (with four chairs), pub_booth,
           pool_table, jukebox, dartboard
  home:    sofa_green, sofa_blue, coffee_table, rug, tv_stand, bed_double, kitchen_run,
           dining_set, wardrobe, bookshelf, floor_lamp, house_plant
  store:   clothes_rack, clothes_table, mannequin_plinth, fitting_rooms, shop_counter,
           hat_stand, store_mirror

Units: metres, Y up, origin on the floor at the centre of the piece's footprint, front (the side
you use it from) toward -Z; a piece that stands against a wall has its back at +Z.

    python3 tools/generate_furniture.py           # write the models and the catalog
    python3 tools/generate_furniture.py --check   # fail if the committed files are stale
"""
import argparse
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_city_kit import Builder, Canvas, flat, shade  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "verda" / "furniture"
CATALOG = ROOT / "include" / "outland" / "creator" / "FurnitureAssetCatalog.hpp"

DARK_WOOD = (92, 60, 36)
MID_WOOD = (138, 98, 60)
LIGHT_WOOD = (186, 150, 104)
BRASS = (196, 160, 70)
CHROME = (186, 190, 196)
BLACK = (30, 30, 32)
WHITE = (232, 230, 224)


def grain(color, seed):
    """Wood: streaks along u."""
    c = Canvas(color=color, seed=seed)
    for y in range(c.h):
        streak = c.rng.randint(-10, 10)
        for x in range(c.w):
            c.px[y * c.w + x] = shade(color, streak + c.rng.randint(-4, 4))
    return c


def fabric(color, seed):
    c = flat(color, 5, seed)
    for y in range(0, c.h, 4):
        c.rect(0, y, c.w, y + 1, shade(color, -12))
    return c


def bottles(seed):
    """A shelf of bottles seen from the front: coloured glass on a dark mirror."""
    c = Canvas(color=(38, 34, 30), seed=seed)
    x = 1
    while x < c.w - 3:
        w = c.rng.randint(2, 4)
        top = c.rng.randint(10, 30)
        colour = c.rng.choice([(60, 120, 50), (150, 90, 30), (200, 190, 150), (110, 30, 30), (40, 70, 110), (190, 150, 60)])
        c.rect(x, top, x + w, c.h, colour, 8)
        c.rect(x + w // 2, top - 6, x + w // 2 + 1, top, colour)
        x += w + c.rng.randint(1, 2)
    return c


def screen():
    c = Canvas(color=(24, 30, 40), seed=3)
    c.rect(4, 4, 60, 60, (44, 70, 96), 10)
    return c


def jukebox_face():
    c = Canvas(color=(150, 30, 40), seed=9)
    for i, colour in enumerate([(255, 200, 60), (255, 120, 40), (90, 200, 255), (255, 80, 160)]):
        c.rect(6 + i * 2, 4 + i * 6, 58 - i * 2, 8 + i * 6, colour)
    c.rect(14, 34, 50, 58, (230, 220, 190), 6)
    return c


def dart_face():
    c = Canvas(color=(20, 20, 20), seed=4)
    for y in range(c.h):
        for x in range(c.w):
            dx, dy = x - 31.5, y - 31.5
            r = math.hypot(dx, dy)
            sector = int((math.atan2(dy, dx) + math.pi) / (2 * math.pi) * 20)
            if r > 30:
                continue
            if r < 2.5:
                col = (200, 30, 30)
            elif r < 5:
                col = (30, 140, 50)
            elif 17 < r < 19 or 27 < r < 29:
                col = (200, 30, 30) if sector % 2 else (30, 140, 50)
            else:
                col = (20, 20, 20) if sector % 2 else (230, 220, 190)
            c.px[y * c.w + x] = col
    return c


def garments(seed):
    """Hanging clothes, front on: a row of coloured shirts and coats."""
    c = Canvas(color=(0, 0, 0), seed=seed)
    x = 0
    palette = [(200, 40, 40), (40, 80, 160), (230, 230, 220), (60, 60, 64), (200, 170, 60), (40, 130, 70),
               (150, 90, 160), (210, 120, 50), (90, 160, 200), (120, 80, 50)]
    while x < c.w:
        w = c.rng.randint(5, 8)
        colour = c.rng.choice(palette)
        c.rect(x, 0, x + w - 1, c.h, colour, 6)
        c.rect(x, 0, x + w - 1, 3, shade(colour, -30))
        x += w
    return c


def stacks(seed):
    """Folded clothes from the side: coloured layers."""
    c = Canvas(color=(0, 0, 0), seed=seed)
    y = 0
    while y < c.h:
        h = c.rng.randint(4, 7)
        colour = c.rng.choice([(200, 40, 40), (40, 80, 160), (230, 230, 220), (60, 60, 64), (200, 170, 60), (40, 130, 70)])
        c.rect(0, y, c.w, y + h - 1, colour, 5)
        c.rect(0, y + h - 1, c.w, y + h, shade(colour, -40))
        y += h
    return c


def mats(b, *names):
    table = {
        "dark_wood": lambda: grain(DARK_WOOD, 11), "mid_wood": lambda: grain(MID_WOOD, 12),
        "light_wood": lambda: grain(LIGHT_WOOD, 13), "brass": lambda: flat(BRASS, 4, 14),
        "chrome": lambda: flat(CHROME, 3, 15), "black": lambda: flat(BLACK, 3, 16), "white": lambda: flat(WHITE, 3, 17),
        "leather": lambda: flat((110, 40, 34), 6, 18), "felt": lambda: flat((30, 110, 60), 5, 19),
        "glass": lambda: flat((150, 190, 200), 3, 20), "steel": lambda: flat((132, 136, 140), 4, 21),
        "mirror": lambda: flat((190, 206, 214), 2, 22), "curtain": lambda: fabric((120, 30, 40), 23),
        "bottles": lambda: bottles(24), "screen": screen, "jukebox": jukebox_face, "dart": dart_face,
        "linen": lambda: fabric((236, 234, 226), 25), "duvet": lambda: fabric((70, 100, 140), 26),
        "rug": lambda: fabric((150, 60, 50), 27), "green_fabric": lambda: fabric((60, 110, 70), 28),
        "blue_fabric": lambda: fabric((60, 80, 130), 29), "cream": lambda: flat((226, 220, 200), 3, 30),
        "plant": lambda: flat((50, 120, 50), 18, 31), "pot": lambda: flat((170, 90, 60), 6, 32),
        "lamp": lambda: flat((250, 236, 190), 2, 33), "garments": lambda: garments(34), "garments2": lambda: garments(35),
        "stacks": lambda: stacks(36), "plinth": lambda: flat((236, 236, 232), 2, 37), "books": lambda: bottles(38),
        "tile": lambda: flat((214, 214, 208), 3, 39), "cash": lambda: fabric((118, 168, 112), 40),
    }
    for name in names:
        b.material(name, table[name](), 1, 1)


def disc(b, material, cx, cz, r, y, sides=16):
    """A flat round top at height y, seen from above and below."""
    tu = b.materials[material][1]
    for k in range(sides):
        a0, a1 = 2 * math.pi * k / sides, 2 * math.pi * (k + 1) / sides
        p0, p1 = (cx + r * math.sin(a0), y, cz + r * math.cos(a0)), (cx + r * math.sin(a1), y, cz + r * math.cos(a1))
        uv = [(.5, .5), (.5, .5), (.5 + r * math.sin(a1) / tu, .5 + r * math.cos(a1) / tu), (.5 + r * math.sin(a0) / tu, .5 + r * math.cos(a0) / tu)]
        b.raw(material, [(cx, y, cz), (cx, y, cz), p1, p0], (0, 1, 0), uv)
        b.raw(material, [(cx, y, cz), (cx, y, cz), p0, p1], (0, -1, 0), [uv[0], uv[1], uv[3], uv[2]])


def legs(b, x0, x1, z0, z1, height, thick, material):
    for x in (x0, x1 - thick):
        for z in (z0, z1 - thick):
            b.solid((x, 0, z), (x + thick, height, z + thick), material)


def chair(b, cx, cz, facing, seat="dark_wood"):
    """A plain wooden chair at (cx, cz); `facing` is the direction the sitter faces: '-z', '+z', '-x' or '+x'."""
    s = .21
    legs(b, cx - s, cx + s, cz - s, cz + s, .45, .04, "dark_wood")
    b.solid((cx - s - .01, .45, cz - s - .01), (cx + s + .01, .5, cz + s + .01), seat)
    back = {'-z': ((cx - s, cz + s - .04), (cx + s, cz + s)), '+z': ((cx - s, cz - s), (cx + s, cz - s + .04)),
            '-x': ((cx + s - .04, cz - s), (cx + s, cz + s)), '+x': ((cx - s, cz - s), (cx - s + .04, cz + s))}[facing]
    b.solid((back[0][0], .5, back[0][1]), (back[1][0], .95, back[1][1]), "dark_wood")


# ---------------------------------------------------------------- the pub
def bar_counter():
    """5 x 2.4 m: the counter (customers at -Z) with a brass foot rail and three beer taps, a gap
    for the barman, and the back bar against the wall - shelves of bottles under a mirror."""
    b = Builder()
    mats(b, "dark_wood", "mid_wood", "brass", "bottles", "mirror", "black", "chrome")
    w = 2.5
    b.solid((-w, 0, -1.2), (w, 1.02, -.6), "dark_wood")
    b.solid((-w - .05, 1.02, -1.28), (w + .05, 1.1, -.55), "mid_wood")
    b.solid((-w, .16, -1.36), (w, .2, -1.32), "brass")
    for x in (-w + .2, 0, w - .2):
        b.solid((x - .02, 0, -1.36), (x + .02, .2, -1.28), "brass")
    for x in (-.6, -.3, 0):
        b.solid((x - .03, 1.1, -.85), (x + .03, 1.48, -.79), "chrome")
        b.solid((x - .025, 1.34, -.95), (x + .025, 1.5, -.85), "black")
    b.solid((1.2, 1.1, -.95), (1.6, 1.36, -.7), "black")                     # the till
    # The back bar: cupboard, three shelves of bottles, the mirror.
    b.solid((-w, 0, .75), (w, .95, 1.2), "dark_wood")
    b.solid((-w - .03, .95, .72), (w + .03, 1.0, 1.2), "mid_wood")
    b.box((-w, 1.0, 1.15), (w, 2.5, 1.2), {'-z': "mirror"})
    for y in (1.05, 1.5, 1.95):
        b.solid((-w, y, .95), (w, y + .04, 1.15), "mid_wood")
        b.box((-w, y + .04, 1.0), (w, y + .38, 1.08), {'-z': "bottles", '+y': "black"})
    b.solid((-w - .05, 2.5, .9), (w + .05, 2.62, 1.2), "dark_wood")
    return b


def bar_stool():
    b = Builder()
    mats(b, "chrome", "leather")
    b.solid((-.03, 0, -.03), (.03, .72, .03), "chrome")
    b.ring("chrome", 0, 0, .2, .2, 0, .02, 12)
    disc(b, "chrome", 0, 0, .2, .02, 12)
    b.ring("chrome", 0, 0, .16, .16, .26, .28, 12)
    disc(b, "chrome", 0, 0, .16, .28, 12)
    b.ring("leather", 0, 0, .19, .19, .72, .78, 12)
    disc(b, "leather", 0, 0, .19, .78, 12)
    return b


def pub_table():
    """A square table with four chairs round it: 1.8 x 1.8 m."""
    b = Builder()
    mats(b, "dark_wood", "mid_wood", "brass", "leather")
    b.solid((-.05, 0, -.05), (.05, .72, .05), "dark_wood")
    b.solid((-.3, 0, -.3), (.3, .04, .3), "dark_wood")
    b.solid((-.45, .72, -.45), (.45, .76, .45), "mid_wood")
    for cx, cz, facing in ((0, -.7, '+z'), (0, .7, '-z'), (-.7, 0, '+x'), (.7, 0, '-x')):
        chair(b, cx, cz, facing, "leather")
    return b


def pub_booth():
    """A booth against the wall: two high-backed benches facing each other across a table; 2.2 x 1.6 m."""
    b = Builder()
    mats(b, "dark_wood", "mid_wood", "leather")
    for z0, z1, back in ((-1.1, -.55, -1.1), (.55, 1.1, 1.0)):
        b.solid((-.8, 0, z0), (.8, .45, z1), "dark_wood")
        b.solid((-.8, .45, z0), (.8, .52, z1), "leather")
        b.solid((-.8, .45, back), (.8, 1.25, back + .1), "leather")
        b.solid((-.82, 1.25, back - .02), (.82, 1.3, back + .12), "dark_wood")
    b.solid((-.1, 0, -.1), (.1, .72, .1), "dark_wood")
    b.solid((-.7, .72, -.4), (.7, .76, .4), "mid_wood")
    return b


def pool_table():
    """2.6 x 1.5 m: green felt, wooden rails, turned legs, six pockets."""
    b = Builder()
    mats(b, "dark_wood", "felt", "black")
    b.solid((-1.2, .55, -.65), (1.2, .78, .65), "dark_wood")
    b.box((-1.18, .78, -.63), (1.18, .8, .63), {'+y': "felt"})
    for x0, x1, z0, z1 in ((-1.3, 1.3, -.75, -.63), (-1.3, 1.3, .63, .75), (-1.3, -1.18, -.63, .63), (1.18, 1.3, -.63, .63)):
        b.solid((x0, .55, z0), (x1, .86, z1), "dark_wood")
    for x in (-1.22, 0, 1.22):
        for z in (-.67, .67):
            b.solid((x - .05, .8, z - .05), (x + .05, .865, z + .05), "black")
    legs(b, -1.15, 1.15, -.6, .6, .55, .14, "dark_wood")
    return b


def jukebox():
    b = Builder()
    mats(b, "dark_wood", "jukebox", "chrome")
    b.solid((-.45, 0, -.3), (.45, 1.2, .3), "dark_wood")
    b.box((-.4, .2, -.31), (.4, 1.2, -.3), {'-z': "jukebox"})
    b.ring("chrome", 0, 0, .45, .45, 1.2, 1.25, 16)
    b.dome("jukebox", 0, 0, .45, 1.2, 4, 16, .8)
    return b


def dartboard():
    """A darts panel against the wall (its back at +Z), floor to 2.2 m: the board in a black
    surround at 1.73 m, a chalk score board beside it, a skirting rail."""
    b = Builder()
    mats(b, "dart", "black", "dark_wood")
    b.solid((-.7, 0, .02), (.7, 2.2, .08), "dark_wood")
    b.solid((-.72, 0, -.02), (.72, .12, .08), "black")
    b.box((-.49, 1.49, -.02), (-.01, 1.97, .02), {'-z': "dart", '+y': "black", '-y': "black", '+x': "black", '-x': "black"})
    b.solid((.15, 1.3, -.01), (.6, 1.9, .02), "black")
    return b


# ---------------------------------------------------------------- the home
def sofa(colour):
    """2.2 x 0.9 m, back against the wall (+Z)."""
    b = Builder()
    mats(b, colour, "dark_wood")
    b.solid((-1.1, .08, -.45), (1.1, .42, .45), colour)
    b.solid((-1.1, .42, .2), (1.1, .9, .45), colour)
    for x in (-1.1, .92):
        b.solid((x, .42, -.45), (x + .18, .65, .45), colour)
    for x in (-1.05, 1.0):
        for z in (-.42, .38):
            b.solid((x, 0, z), (x + .05, .08, z + .05), "dark_wood")
    return b


def coffee_table():
    b = Builder()
    mats(b, "light_wood", "glass")
    legs(b, -.55, .55, -.3, .3, .38, .05, "light_wood")
    b.solid((-.58, .38, -.33), (.58, .42, .33), "glass")
    return b


def rug():
    b = Builder()
    mats(b, "rug")
    b.solid((-1.3, 0, -.9), (1.3, .015, .9), "rug")
    return b


def tv_stand():
    """1.6 x 0.45 m against the wall, the television on it."""
    b = Builder()
    mats(b, "dark_wood", "black", "screen")
    b.solid((-.8, 0, -.22), (.8, .5, .22), "dark_wood")
    b.solid((-.12, .5, -.05), (.12, .56, .1), "black")
    b.box((-.62, .56, -.03), (.62, 1.28, .03), {'-z': "screen", '+z': "black", '+y': "black", '-y': "black", '+x': "black", '-x': "black"})
    return b


def bed_double():
    """1.6 x 2.1 m bed, headboard against the wall (+Z), a nightstand each side: 2.8 x 2.2 m."""
    b = Builder()
    mats(b, "mid_wood", "linen", "duvet", "lamp")
    b.solid((-.8, 0, -1.05), (.8, .32, 1.05), "mid_wood")
    b.solid((-.78, .32, -1.03), (.78, .52, 1.03), "linen")
    b.solid((-.8, .52, -1.05), (.8, .58, .45), "duvet")
    for x in (-.4, .4):
        b.solid((x - .3, .52, .6), (x + .3, .66, .95), "linen")
    b.solid((-.82, 0, 1.0), (.82, 1.1, 1.1), "mid_wood")
    for x in (-1.15, 1.15):
        b.solid((x - .23, 0, .65), (x + .23, .55, 1.1), "mid_wood")
        b.ring("lamp", x, .88, .1, .07, .55, .78, 10)
        disc(b, "lamp", x, .88, .07, .78, 10)
    return b


def kitchen_run():
    """3.2 x 0.65 m along the wall: base cupboards under a worktop with the sink and the hob, wall
    cupboards over, and the fridge at the +X end."""
    b = Builder()
    mats(b, "cream", "dark_wood", "steel", "black", "white", "tile")
    b.solid((-1.6, 0, -.3), (.9, .86, .32), "cream")
    b.solid((-1.62, .86, -.33), (.92, .9, .32), "dark_wood")
    b.box((-1.6, .9, .28), (.9, 1.5, .32), {'-z': "tile"})
    b.solid((-1.2, .87, -.2), (-.7, .91, .15), "steel")                     # the sink
    b.solid((-.95, .9, .1), (-.92, 1.2, .14), "steel")
    b.solid((.1, .9, -.22), (.7, .92, .2), "black")                         # the hob
    b.solid((-1.6, 1.5, -.05), (.9, 2.2, .32), "cream")
    for x in (-1.2, -.4, .4):
        b.solid((x - .05, 1.55, -.06), (x + .05, 1.58, -.05), "steel")
        b.solid((x - .05, .7, -.31), (x + .05, .73, -.3), "steel")
    b.solid((.95, 0, -.3), (1.6, 1.85, .32), "white")                       # the fridge
    b.solid((1.48, .9, -.32), (1.52, 1.5, -.3), "steel")
    return b


def dining_set():
    """A 1.4 x 0.9 m table with four chairs: 1.8 x 1.9 m."""
    b = Builder()
    mats(b, "light_wood", "dark_wood")
    legs(b, -.65, .65, -.4, .4, .74, .06, "light_wood")
    b.solid((-.7, .74, -.45), (.7, .78, .45), "light_wood")
    for x in (-.35, .35):
        chair(b, x, -.72, '+z', "light_wood")
        chair(b, x, .72, '-z', "light_wood")
    return b


def wardrobe():
    b = Builder()
    mats(b, "mid_wood", "brass")
    b.solid((-.6, 0, -.3), (.6, 2.0, .3), "mid_wood")
    b.solid((-.01, .1, -.31), (.01, 1.95, -.3), "brass")
    for x in (-.08, .06):
        b.solid((x, .95, -.33), (x + .02, 1.15, -.3), "brass")
    return b


def bookshelf():
    b = Builder()
    mats(b, "dark_wood", "books")
    b.solid((-.5, 0, -.17), (.5, 1.9, .17), "dark_wood")
    for y in (.1, .55, 1.0, 1.45):
        b.box((-.46, y, -.18), (.46, y + .38, .1), {'-z': "books"})
    return b


def floor_lamp():
    b = Builder()
    mats(b, "black", "lamp")
    b.ring("black", 0, 0, .16, .16, 0, .03, 12)
    disc(b, "black", 0, 0, .16, .03, 12)
    b.solid((-.015, 0, -.015), (.015, 1.45, .015), "black")
    b.ring("lamp", 0, 0, .22, .13, 1.35, 1.65, 12, True)
    return b


def house_plant():
    b = Builder()
    mats(b, "pot", "plant")
    b.ring("pot", 0, 0, .14, .18, 0, .32, 12)
    disc(b, "pot", 0, 0, .17, .3, 12)
    rng = random.Random(5)
    for k in range(7):
        a = k * 2 * math.pi / 7
        x, z = .12 * math.cos(a), .12 * math.sin(a)
        h = .7 + rng.random() * .4
        b.solid((x - .07, .3, z - .07), (x + .07, h, z + .07), "plant")
    return b


# ---------------------------------------------------------------- the clothing store
def clothes_rack():
    """1.6 m chrome rail on two stands, a row of clothes on hangers (both faces show)."""
    b = Builder()
    mats(b, "chrome", "garments", "garments2")
    for x in (-.8, .8):
        b.solid((x - .02, 0, -.25), (x + .02, .03, .25), "chrome")
        b.solid((x - .02, 0, -.02), (x + .02, 1.55, .02), "chrome")
    b.solid((-.8, 1.52, -.015), (.8, 1.55, .015), "chrome")
    b.box((-.75, .7, -.2), (.75, 1.48, .2), {'-z': "garments", '+z': "garments2", '+y': "garments", '-x': "garments2", '+x': "garments"})
    return b


def clothes_table():
    """A display table (1.4 x 0.8 m) with stacks of folded clothes."""
    b = Builder()
    mats(b, "light_wood", "stacks")
    b.solid((-.7, 0, -.4), (.7, .8, .4), "light_wood")
    for x in (-.45, 0, .45):
        for z in (-.18, .18):
            b.solid((x - .17, .8, z - .14), (x + .17, .8 + .1 + .06 * ((x + z * 3) % .3 > .1), z + .14), "stacks")
    return b


def mannequin_plinth():
    """The round plinth a dressed mannequin stands on (0.15 m high; the game stands the figure on it)."""
    b = Builder()
    mats(b, "plinth", "chrome")
    b.ring("plinth", 0, 0, .38, .38, 0, .15, 20)
    disc(b, "plinth", 0, 0, .38, .15, 20)
    b.ring("chrome", 0, 0, .385, .385, .13, .15, 20)
    return b


def fitting_rooms():
    """Three changing cubicles (1.2 m each) side by side: walls, a curtain half drawn across each, a
    mirror and a stool inside; 3.6 x 1.5 m, open to -Z."""
    b = Builder()
    mats(b, "white", "curtain", "chrome", "mirror", "light_wood")
    b.solid((-1.8, 0, .7), (1.8, 2.3, .75), "white")
    for x in (-1.8, -.6, .6, 1.8):
        b.solid((x - .03, 0, -.75), (x + .03, 2.3, .75), "white")
    b.solid((-1.83, 2.3, -.78), (1.83, 2.36, .78), "white")
    for cx in (-1.2, 0, 1.2):
        b.solid((cx - .58, 2.2, -.76), (cx + .58, 2.22, -.74), "chrome")
        b.solid((cx - .57, .15, -.76), (cx - .05, 2.2, -.72), "curtain")
        b.box((cx - .3, .5, .68), (cx + .3, 2.0, .7), {'-z': "mirror"})
        b.solid((cx - .2, 0, .3), (cx + .2, .45, .62), "light_wood")
    return b


def shop_counter():
    """2.4 x 0.7 m: a wooden counter with the till, a stack of carrier bags and a tissue-paper roll."""
    b = Builder()
    mats(b, "light_wood", "white", "black", "steel", "stacks")
    b.solid((-1.2, 0, -.35), (1.2, 1.0, .35), "light_wood")
    b.solid((-1.23, 1.0, -.38), (1.23, 1.04, .38), "white")
    b.solid((.4, 1.04, -.15), (.8, 1.3, .2), "black")
    b.solid((.45, 1.3, -.1), (.75, 1.45, .0), "steel")
    b.solid((-.9, 1.04, -.1), (-.5, 1.3, .2), "stacks")
    return b


def hat_stand():
    """A display stand with two heads at 1.55 m (the game puts a hat on each: x = -0.25, 0.25)."""
    b = Builder()
    mats(b, "dark_wood", "plinth")
    b.solid((-.45, 0, -.25), (.45, .9, .25), "dark_wood")
    for x in (-.25, .25):
        b.solid((x - .02, .9, -.02), (x + .02, 1.38, .02), "dark_wood")
        b.ring("plinth", x, 0, .07, .09, 1.38, 1.46, 10)
        b.dome("plinth", x, 0, .09, 1.46, 4, 10, 1.0)
    return b


def store_mirror():
    b = Builder()
    mats(b, "mirror", "light_wood")
    b.solid((-.45, 0, -.06), (.45, .08, .2), "light_wood")
    b.solid((-.42, .08, .02), (.42, 2.0, .08), "light_wood")
    b.box((-.38, .12, -.0), (.38, 1.96, .02), {'-z': "mirror"})
    return b


def cash_bag():
    """An open holdall stuffed with banknote bricks, 0.8 x 0.4 m, on the floor."""
    b = Builder()
    mats(b, "black", "brass", "cash")
    b.solid((-.4, 0, -.2), (.4, .3, .2), "black")
    b.solid((-.4, .3, -.2), (.4, .34, -.16), "black")
    b.solid((-.4, .3, .16), (.4, .34, .2), "black")
    b.solid((-.36, .3, -.15), (.36, .4, .15), "cash")
    for x in (-.24, .02):
        b.solid((x, .4, -.12), (x + .22, .48, -.01), "cash")
        b.solid((x + .04, .4, .02), (x + .26, .46, .13), "cash")
    for x in (-.2, .17):
        b.solid((x, .3, -.02), (x + .03, .52, .02), "black")
    b.solid((-.2, .5, -.02), (.2, .53, .02), "black")
    b.solid((-.02, .2, -.205), (.02, .3, -.2), "brass")
    return b


def pieces():
    """(file, builder, id, name, footprint w, d, h)"""
    return [
        ("bar_counter.glb", bar_counter(), "Bar counter with back bar (5 x 2.4 m)", 5.1, 2.6, 2.6),
        ("bar_stool.glb", bar_stool(), "Bar stool", .4, .4, .8),
        ("pub_table.glb", pub_table(), "Pub table with four chairs", 1.8, 1.8, .95),
        ("pub_booth.glb", pub_booth(), "Pub booth (two benches and a table)", 1.6, 2.2, 1.3),
        ("pool_table.glb", pool_table(), "Pool table", 2.6, 1.5, .87),
        ("jukebox.glb", jukebox(), "Jukebox", .9, .6, 1.6),
        ("dartboard.glb", dartboard(), "Darts panel with board and score board (against a wall)", 1.45, .12, 2.2),
        ("sofa_green.glb", sofa("green_fabric"), "Sofa, green", 2.2, .9, .9),
        ("sofa_blue.glb", sofa("blue_fabric"), "Sofa, blue", 2.2, .9, .9),
        ("coffee_table.glb", coffee_table(), "Coffee table", 1.2, .7, .42),
        ("rug.glb", rug(), "Rug", 2.6, 1.8, .02),
        ("tv_stand.glb", tv_stand(), "TV on a stand", 1.6, .45, 1.3),
        ("bed_double.glb", bed_double(), "Double bed with nightstands", 2.8, 2.2, 1.1),
        ("kitchen_run.glb", kitchen_run(), "Kitchen: cupboards, sink, hob and fridge", 3.2, .65, 2.2),
        ("dining_set.glb", dining_set(), "Dining table with four chairs", 1.8, 1.9, .95),
        ("wardrobe.glb", wardrobe(), "Wardrobe", 1.2, .6, 2.0),
        ("bookshelf.glb", bookshelf(), "Bookshelf", 1.0, .35, 1.9),
        ("floor_lamp.glb", floor_lamp(), "Floor lamp", .45, .45, 1.65),
        ("house_plant.glb", house_plant(), "House plant", .4, .4, 1.1),
        ("clothes_rack.glb", clothes_rack(), "Clothes rack", 1.6, .5, 1.55),
        ("clothes_table.glb", clothes_table(), "Clothes display table", 1.4, .8, .96),
        ("mannequin_plinth.glb", mannequin_plinth(), "Mannequin plinth", .8, .8, .15),
        ("fitting_rooms.glb", fitting_rooms(), "Fitting rooms (three cubicles)", 3.7, 1.6, 2.4),
        ("shop_counter.glb", shop_counter(), "Shop counter with till", 2.5, .8, 1.45),
        ("hat_stand.glb", hat_stand(), "Hat display stand", .9, .5, 1.6),
        ("store_mirror.glb", store_mirror(), "Standing mirror", .9, .3, 2.0),
        ("cash_bag.glb", cash_bag(), "Holdall full of cash", .8, .4, .53),
    ]


def catalog_header(items):
    lines = [
        "#pragma once",
        "// Generated by tools/generate_furniture.py - do not edit.",
        "#include \"outland/creator/CreatorAssetRegistry.hpp\"",
        "namespace outland::creator {",
        "inline void register_furniture_assets(CreatorAssetRegistry& registry) {",
    ]
    for path, _, name, w, d, h in items:
        ident = "furniture_" + Path(path).stem
        lines.append(f"    (void)registry.add({{.id = \"{ident}\", .name = \"{name}\", .category = CreatorAssetCategory::Prop,")
        lines.append(f"        .model_path = \"assets/verda/furniture/{path}\", .thumbnail_path = \"\", .footprint = {{{float(w):.2f}F, {float(d):.2f}F, {float(h):.2f}F}},")
        lines.append("        .placement = {}, .default_scale = 1, .tags = {\"verda\", \"furniture\", \"interior\"}});")
    lines += ["}", "}  // namespace outland::creator", ""]
    return "\n".join(lines).encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if committed files are stale")
    args = parser.parse_args()
    items = pieces()
    outputs = {OUT / path: builder.glb(Path(path).stem) for path, builder, *_ in items}
    outputs[CATALOG] = catalog_header(items)
    stale = [p for p, data in outputs.items() if not p.exists() or p.read_bytes() != data]
    if args.check:
        for p in stale:
            print(f"stale: {p.relative_to(ROOT)}", file=sys.stderr)
        return 1 if stale else 0
    for p, data in outputs.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    print(f"wrote {len(outputs) - 1} furniture models and {CATALOG.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
