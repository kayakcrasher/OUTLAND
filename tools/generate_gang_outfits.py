#!/usr/bin/env python3
"""Generate La Verda Mano's clothes (assets/verda/characters/outfits/*.png).

Both are repaints of Character_05's texture (a dress shirt, black trousers and polished shoes),
worn on that body by the Green Hand's crews (the game swaps the texture in when it draws them):

  verda_mano_soldato.png  the foot soldiers: the shirt plain white, the open green hand on the
                          chest and, bigger, across the back.
  verda_mano_kapo.png     the officers, dressed sharp: the shirt turned into a charcoal jacket
                          with lapels over a white shirt and a green tie, a small green hand
                          on the lapel and a green pocket square.

and every other gang's colours, so the Policio can tell them apart at a glance:

  gang_<n>_soldato.png    the foot soldiers of gang n (1 Montaj Lupoj, 2 La Ora Ankro, 3 Marĉaj
                          Ranoj, 4 Feraj Hundoj): the shirt in the gang's colour, a dark band
                          across the shoulders, front and back.
  gang_<n>_kapo.png       their bosses: the charcoal jacket with the tie and pocket square in
                          the gang's colour.

the ball clubs' jerseys (game::sports, La Verda Basballigo):

  team_<n>_jersey.png     team n's colours: the shirt in its first colour, a band of the second
                          across the shoulders and a block of it on the back.

and the occupiers' (game::mafia::Endgame):

  rusa_soldato.png        the Russian troops who land when the gangs win: the shirt turned to a
                          woodland camouflage of olive, earth and dark green blotches.

On this texture the shirt's front panel lies on its side (its collar toward +x) and the back
panel upside down (its collar toward +y); the drawing is turned to match.

    python3 tools/generate_gang_outfits.py           # write the textures
    python3 tools/generate_gang_outfits.py --check   # fail if the committed textures are stale
"""
import io
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/verda/characters/psx/textures/Character_05.png"
OUT = ROOT / "assets/verda/characters/outfits"
GREEN = (0, 150, 60, 255)
GREEN_DARK = (0, 92, 36, 255)

FRONT = (110, 125)      # the chest, on the front panel (x toward the collar)
CHEST = (90, 125)       # the soldier's hand, a little lower
BACK = (392, 292)       # between the shoulder blades, on the back panel (y toward the collar)


def shirt_mask(image):
    """The shirt's pale blue cloth."""
    px = image.load()
    w, h = image.size
    mask = Image.new("L", image.size, 0)
    m = mask.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if b > 150 and b > r + 12 and b >= g and a > 0:
                m[x, y] = 255
    return mask


def recolour(image, mask, base, spread):
    """Shirt pixels to `base` (r, g, b), keeping their folds (brightness around the mean)."""
    px, m = image.load(), mask.load()
    w, h = image.size
    total = count = 0
    for y in range(h):
        for x in range(w):
            if m[x, y]:
                r, g, b, _ = px[x, y]
                total += r + g + b
                count += 1
    mean = total / max(1, count) / 3
    for y in range(h):
        for x in range(w):
            if m[x, y]:
                r, g, b, a = px[x, y]
                d = ((r + g + b) / 3 - mean) * spread
                px[x, y] = tuple(max(0, min(255, int(c + d))) for c in base) + (a,)


def hand(size, colour, outline):
    """The open hand, upright, on a transparent square `size` pixels across."""
    s = size * 4
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    u = s / 20
    for grow, fill in ((u * .55, outline), (0, colour)):
        d.ellipse([s / 2 - 4.6 * u - grow, 9.5 * u - grow, s / 2 + 4.6 * u + grow, 18.5 * u + grow], fill=fill)
        d.rectangle([s / 2 - 4.3 * u - grow, 9 * u - grow, s / 2 + 4.3 * u + grow, 15 * u + grow], fill=fill)
        for i, length in enumerate((5.2, 6.6, 6.8, 5.4)):
            x = s / 2 + (-3.3 + 2.2 * i) * u
            d.rounded_rectangle([x - .82 * u - grow, 10 * u - length * u - grow, x + .82 * u + grow, 11 * u + grow],
                                radius=.82 * u + grow, fill=fill)
        d.line([(s / 2 - 3.6 * u, 14.5 * u), (s / 2 - 7.2 * u, 10.2 * u)], fill=fill, width=int(2.2 * u + 2 * grow))
    return im.resize((size, size), Image.LANCZOS)


def stamp(image, art, centre, turn):
    art = art.rotate(turn, expand=True)
    image.alpha_composite(art, (int(centre[0] - art.width / 2), int(centre[1] - art.height / 2)))


def soldato(base):
    image = base.copy()
    recolour(image, shirt_mask(base), (236, 238, 234), .55)
    stamp(image, hand(64, GREEN, GREEN_DARK), CHEST, -90)      # upright on the chest
    stamp(image, hand(96, GREEN, GREEN_DARK), BACK, 180)       # upright across the back
    return image


def kapo(base, tie=GREEN, knot=GREEN_DARK, pin=True):
    image = base.copy()
    recolour(image, shirt_mask(base), (40, 42, 48), .35)
    # The front, drawn upright then turned onto the panel: the shirt's V between the lapels,
    # the tie, a hand on the lapel, a pocket square.
    art = Image.new("RGBA", (150, 160), (0, 0, 0, 0))
    d = ImageDraw.Draw(art)
    cx = 75
    d.polygon([(cx - 26, 0), (cx + 26, 0), (cx, 92)], fill=(244, 244, 240, 255))               # the shirt
    d.polygon([(cx - 26, 0), (cx - 34, 0), (cx - 4, 104), (cx, 92)], fill=(26, 27, 32, 255))     # lapel edges
    d.polygon([(cx + 26, 0), (cx + 34, 0), (cx + 4, 104), (cx, 92)], fill=(26, 27, 32, 255))
    d.polygon([(cx - 6, 4), (cx + 6, 4), (cx + 4, 14), (cx - 4, 14)], fill=knot)                # the knot
    d.polygon([(cx - 4, 14), (cx + 4, 14), (cx + 8, 78), (cx, 90), (cx - 8, 78)], fill=tie)       # the tie
    d.polygon([(cx + 34, 40), (cx + 58, 40), (cx + 58, 47), (cx + 34, 47)], fill=tie)            # pocket square
    if pin:
        art.alpha_composite(hand(18, GREEN, (12, 12, 12, 255)), (cx - 44, 14))                   # lapel pin
    for y in (104, 124, 144):                                                                     # jacket buttons
        d.ellipse([cx - 3, y - 3, cx + 3, y + 3], fill=(12, 12, 14, 255))
    stamp(image, art, (FRONT[0] - 5, FRONT[1]), -90)
    return image


# The other gangs' colours (game::mafia::gangs), by gang index.
GANG_COLOURS = {1: (168, 176, 190), 2: (240, 196, 64), 3: (64, 178, 190), 4: (200, 86, 52)}


def shade(colour, k):
    return tuple(max(0, min(255, int(c * k))) for c in colour[:3]) + (255,)


def colours_shirt(colour):
    def make(base):
        image = base.copy()
        mask = shirt_mask(base)
        recolour(image, mask, colour, .5)
        # A dark band across the shoulders, front and back: on the front panel the collar is
        # toward +x, on the back panel toward +y.
        band = shade(colour, .38)
        px, m = image.load(), mask.load()
        w, h = image.size
        for y in range(h):
            for x in range(w):
                if not m[x, y]:
                    continue
                front = x < 256 and y < 256
                if (front and 150 <= x <= 172) or (not front and x >= 256 and 348 <= y <= 372):
                    px[x, y] = band
        return image
    return make


# La Verda Basballigo's five teams (game::sports::teams): the shirt in the team's colour, a band
# of its second colour across the shoulders and a block of it on the back (the players' and the
# fans' jerseys alike).
TEAM_COLOURS = [((18, 118, 58), (228, 182, 40)), ((104, 66, 38), (240, 122, 24)), ((26, 46, 108), (118, 190, 240)),
                ((0, 124, 126), (236, 236, 226)), ((40, 40, 44), (196, 42, 30))]


def team_jersey(primary, secondary):
    def make(base):
        image = base.copy()
        mask = shirt_mask(base)
        recolour(image, mask, primary, .45)
        band = tuple(secondary) + (255,)
        px, m = image.load(), mask.load()
        w, h = image.size
        for y in range(h):
            for x in range(w):
                if not m[x, y]:
                    continue
                front = x < 256 and y < 256
                if (front and 146 <= x <= 176) or (not front and x >= 256 and 344 <= y <= 376):
                    px[x, y] = band
                elif not front and x >= 256 and 300 <= y <= 330 and 360 <= x <= 424:   # the number block on the back
                    px[x, y] = band
        return image
    return make


def camouflage(base):
    """The occupiers' field shirt: olive with blotches of earth and dark green (a fixed pattern)."""
    image = base.copy()
    mask = shirt_mask(base)
    recolour(image, mask, (92, 98, 62), .45)
    blotches = Image.new("RGBA", image.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(blotches)
    state = 20251024
    def rand():
        nonlocal state
        state = (state * 1103515245 + 12345) & 0x7fffffff
        return state / 0x7fffffff
    for colour in ((70, 56, 38, 255), (44, 58, 34, 255), (128, 120, 82, 255)):
        for _ in range(70):
            x, y = rand() * image.width, rand() * image.height
            rx, ry = 6 + rand() * 16, 4 + rand() * 10
            d.ellipse([x - rx, y - ry, x + rx, y + ry], fill=colour)
    px, m, b = image.load(), mask.load(), blotches.load()
    for y in range(image.height):
        for x in range(image.width):
            if m[x, y] and b[x, y][3]:
                r, g, bl, a = px[x, y]
                # Keep the folds: the blotch colour moved by the shirt's own shading.
                shade_k = (r + g + bl) / 3 / 84
                px[x, y] = tuple(max(0, min(255, int(c * shade_k))) for c in b[x, y][:3]) + (a,)
    return image


def build():
    base = Image.open(SOURCE).convert("RGBA")
    out = {}
    makers = [("verda_mano_soldato.png", soldato), ("verda_mano_kapo.png", kapo)]
    for gang, colour in GANG_COLOURS.items():
        makers.append((f"gang_{gang}_soldato.png", colours_shirt(colour)))
        makers.append((f"gang_{gang}_kapo.png", lambda b, c=colour: kapo(b, shade(c, 1), shade(c, .6), pin=False)))
    makers.append(("rusa_soldato.png", camouflage))
    for team, (primary, secondary) in enumerate(TEAM_COLOURS):
        makers.append((f"team_{team}_jersey.png", team_jersey(primary, secondary)))
    for name, make in makers:
        buffer = io.BytesIO()
        make(base).save(buffer, "PNG", optimize=True)
        out[OUT / name] = buffer.getvalue()
    return out


if __name__ == "__main__":
    outputs = build()
    for path, data in outputs.items():
        if "--check" in sys.argv:
            if not path.exists() or path.read_bytes() != data:
                raise SystemExit(f"Stale outfit {path.name}: run {Path(__file__).name}")
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {len(data)} bytes")
