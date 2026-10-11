#!/usr/bin/env python3
"""Generate the armour vests found as loot (assets/verda/combat/vest_light.glb, vest_heavy.glb):
a light police-style vest and a heavy plate carrier with pouches and a collar. Vertex colours,
metres, lying flat as they would on the ground (front up). Used for pickups; the game applies
the protection (game::combat::armour_reduction).

    python3 tools/generate_armour_models.py            # write the models
    python3 tools/generate_armour_models.py --check    # fail if the committed models are stale
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_sight_models as kit  # noqa: E402  (Mesh: boxes and tubes)
import generate_truck_model as truck  # noqa: E402  (glb writer)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/verda/combat"
NAVY = (36, 44, 62, 255)
OLIVE = (78, 86, 58, 255)
OLIVE_DARK = (58, 64, 44, 255)
STRAP = (28, 30, 30, 255)
PATCH = (225, 225, 220, 255)


def vest_light():
    m = kit.Mesh()
    # Front and back panels laid flat, shoulder straps between, a white patch on the chest.
    m.box(-.24, .24, 0, .04, -.30, .08, NAVY)
    m.box(-.22, .22, 0, .03, .10, .44, NAVY)
    for x in (-.17, .17):
        m.box(x - .04, x + .04, .03, .05, .04, .14, STRAP)
    m.box(-.09, .09, .04, .045, -.12, -.05, PATCH)
    m.box(-.24, .24, .035, .045, -.27, -.24, STRAP)  # waist strap
    return m


def vest_heavy():
    m = kit.Mesh()
    m.box(-.26, .26, 0, .06, -.32, .08, OLIVE)
    m.box(-.24, .24, 0, .05, .10, .46, OLIVE)
    for x in (-.18, .18):
        m.box(x - .05, x + .05, .05, .075, .04, .14, STRAP)
    # Magazine pouches across the front, a collar and side plates.
    for i in range(3):
        x = -.15 + i * .15
        m.box(x - .06, x + .06, .06, .11, -.26, -.12, OLIVE_DARK)
    m.box(-.14, .14, .06, .09, .00, .06, OLIVE_DARK)  # collar
    for x in (-.28, .28):
        m.box(x - .02, x + .02, 0, .05, -.28, .0, OLIVE_DARK)
    return m


MODELS = {"vest_light": vest_light, "vest_heavy": vest_heavy}

if __name__ == "__main__":
    stale = []
    for name, build in MODELS.items():
        data = truck.glb(build().data(), name, False)
        path = OUT / f"{name}.glb"
        if "--check" in sys.argv:
            if not path.exists() or path.read_bytes() != data:
                stale.append(path.name)
        else:
            path.write_bytes(data)
            print(f"wrote {path.relative_to(ROOT)}: {len(data)} bytes")
    if stale:
        raise SystemExit("Stale armour models: " + ", ".join(stale) + f" (run {Path(__file__).name})")
