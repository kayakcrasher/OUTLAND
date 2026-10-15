#!/usr/bin/env python3
"""Generate OUTLAND's app icon: a faceted green five-pointed star on dark forest green, with
OUTLAND across the top (assets/fonts/DejaVuSans-Bold.ttf).

  platform/android/res/...      an adaptive launcher icon (Android 8+): a dark green background
                                colour and the star and title as the foreground, kept inside the
                                safe zone so any launcher mask (circle, squircle) shows it whole
  platform/web/icon-*.png       the browser tab icon and the home-screen icon (iPhone, Android)

    python3 tools/generate_app_icon.py            # write the icons
    python3 tools/generate_app_icon.py --check    # fail if the committed icons are stale

Needs Pillow.
"""
import io
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "assets/fonts/DejaVuSans-Bold.ttf"
RES = ROOT / "platform/android/res"
WEB = ROOT / "platform/web"

BACKGROUND = "#13261A"
BACK_CENTRE = (34, 66, 44)
BACK_EDGE = (10, 22, 14)
STAR_LIGHT = (92, 224, 128)
STAR_MID = (34, 177, 76)
STAR_DARK = (14, 118, 50)
STAR_EDGE = (8, 70, 30)
TITLE = (244, 240, 226)


def star_points(cx, cy, outer, inner):
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = outer if i % 2 == 0 else inner
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def draw_design(size, content):
    """The star and the title in a square of `size` px; `content` is the fraction of the square
    they may use (1 for full-bleed, ~0.62 for an adaptive foreground's safe zone)."""
    s = 4  # supersampling
    big = size * s
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    box = big * content
    left = (big - box) / 2
    top = (big - box) / 2
    # The title along the top.
    font_size = int(box * 0.165)
    font = ImageFont.truetype(str(FONT), font_size)
    text = "OUTLAND"
    spacing = font_size * 0.06
    widths = [d.textlength(ch, font=font) for ch in text]
    total = sum(widths) + spacing * (len(text) - 1)
    if total > box * 0.96:
        font_size = int(font_size * box * 0.96 / total)
        font = ImageFont.truetype(str(FONT), font_size)
        spacing = font_size * 0.06
        widths = [d.textlength(ch, font=font) for ch in text]
        total = sum(widths) + spacing * (len(text) - 1)
    ty = top + box * 0.02
    shadow = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    x = (big - total) / 2
    for ch, w in zip(text, widths):
        sd.text((x + font_size * 0.04, ty + font_size * 0.06), ch, font=font, fill=(0, 0, 0, 170))
        x += w + spacing
    img = Image.alpha_composite(img, shadow.filter(ImageFilter.GaussianBlur(font_size * 0.05)))
    d = ImageDraw.Draw(img)
    x = (big - total) / 2
    for ch, w in zip(text, widths):
        d.text((x, ty), ch, font=font, fill=TITLE + (255,))
        x += w + spacing
    # The star below it: a glow, a dark rim, then ten facets lit from the upper left.
    title_bottom = ty + font_size * 1.15
    space = top + box - title_bottom
    outer = min(space * 0.53, box * 0.42)
    cx, cy = big / 2, title_bottom + space * 0.52
    inner = outer * 0.40
    glow = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    ImageDraw.Draw(glow).polygon(star_points(cx, cy, outer * 1.06, inner * 1.06), fill=(60, 220, 110, 120))
    img = Image.alpha_composite(img, glow.filter(ImageFilter.GaussianBlur(outer * 0.09)))
    d = ImageDraw.Draw(img)
    d.polygon(star_points(cx, cy, outer * 1.035, inner * 1.035), fill=STAR_EDGE + (255,))
    pts = star_points(cx, cy, outer, inner)
    light = (-0.6, -0.8)
    for i in range(10):
        a, b = pts[i], pts[(i + 1) % 10]
        mx, my = (a[0] + b[0]) / 2 - cx, (a[1] + b[1]) / 2 - cy
        n = math.hypot(mx, my) or 1
        facing = (mx * light[0] + my * light[1]) / n          # -1 .. 1
        # Each arm has a lit and a shaded side.
        side = 1 if i % 2 == 0 else -1
        t = max(0.0, min(1.0, 0.5 + 0.45 * facing + 0.12 * side))
        base = STAR_DARK if t < .5 else STAR_LIGHT
        colour = tuple(int(STAR_MID[k] + (base[k] - STAR_MID[k]) * abs(t - .5) * 2) for k in range(3))
        d.polygon([(cx, cy), a, b], fill=colour + (255,))
    return img.resize((size, size), Image.LANCZOS)


def background(size):
    img = Image.new("RGBA", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            t = min(1.0, math.hypot(x - size / 2, y - size * 0.55) / (size * 0.72))
            px[x, y] = tuple(int(BACK_CENTRE[k] + (BACK_EDGE[k] - BACK_CENTRE[k]) * t) for k in range(3)) + (255,)
    return img


def full_icon(size):
    return Image.alpha_composite(background(size), draw_design(size, 0.86))


def png(img):
    out = io.BytesIO()
    img.save(out, "PNG", optimize=True)
    return out.getvalue()


def outputs():
    files = {}
    # Android adaptive icon: 108 dp layers; the foreground's safe zone is the middle 66 dp.
    for density, scale in (("mdpi", 1), ("hdpi", 1.5), ("xhdpi", 2), ("xxhdpi", 3), ("xxxhdpi", 4)):
        size = int(108 * scale)
        files[RES / f"mipmap-{density}/ic_launcher_foreground.png"] = png(draw_design(size, 0.53))
    adaptive = ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<adaptive-icon xmlns:android=\"http://schemas.android.com/apk/res/android\">\n"
                "    <background android:drawable=\"@color/ic_launcher_background\"/>\n"
                "    <foreground android:drawable=\"@mipmap/ic_launcher_foreground\"/>\n"
                "</adaptive-icon>\n").encode()
    files[RES / "mipmap-anydpi-v26/ic_launcher.xml"] = adaptive
    files[RES / "mipmap-anydpi-v26/ic_launcher_round.xml"] = adaptive
    files[RES / "values/ic_launcher_colors.xml"] = (
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<resources>\n"
        f"    <color name=\"ic_launcher_background\">{BACKGROUND}</color>\n</resources>\n").encode()
    # The web: tab icon, home-screen icons, a preview.
    for size in (32, 180, 192, 512):
        files[WEB / f"icon-{size}.png"] = png(full_icon(size))
    return files


if __name__ == "__main__":
    files = outputs()
    stale = [p for p, data in files.items() if not p.exists() or p.read_bytes() != data]
    if "--check" in sys.argv:
        for p in stale:
            print(f"stale: {p.relative_to(ROOT)}", file=sys.stderr)
        sys.exit(1 if stale else 0)
    for p, data in files.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
        print(f"wrote {p.relative_to(ROOT)}: {len(data)} bytes")
