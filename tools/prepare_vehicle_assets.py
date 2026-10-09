#!/usr/bin/env python3
"""Preserve vehicle sources, derive hatchback parts and maintain the TSV/Creator catalog.

Existing TSV tuning and manually added variants are preserved. Newly supplied GGBot
GLBs get initial approximate tuning which must be verified on device.
"""
import argparse
import copy
import csv
import hashlib
import json
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIELDS = """id name body wheel length width height wheel_radius front_z rear_z track
acceleration brake reverse drag top_speed steer health engine_health tire_health fuel
seat_x seat_y seat_z exit_x exit_z camera_distance camera_height max_slope body_offset""".split()


def read(path):
    data = path.read_bytes()
    magic, version, total = struct.unpack_from("<III", data)
    if magic != 0x46546C67 or version != 2 or total != len(data):
        raise ValueError(f"Invalid GLB: {path}")
    size, kind = struct.unpack_from("<II", data, 12)
    if kind != 0x4E4F534A:
        raise ValueError(f"Missing GLB JSON: {path}")
    return json.loads(data[20:20 + size]), data[28 + size:]


def glb(document, binary):
    encoded = json.dumps(document, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    binary += b"\0" * (-len(binary) % 4)
    return (struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(binary))
            + struct.pack("<II", len(encoded), 0x4E4F534A) + encoded
            + struct.pack("<II", len(binary), 0x004E4942) + binary)


def part(document, binary, node, center):
    doc = copy.deepcopy(document)
    selected = copy.deepcopy(node)
    selected.pop("children", None)
    selected["translation"] = [-value for value in center]
    selected.pop("rotation", None)
    selected.pop("scale", None)
    # raylib loads all meshes, even those not referenced by active scene nodes.
    doc["meshes"] = [doc["meshes"][selected["mesh"]]]
    selected["mesh"] = 0
    doc.update(nodes=[selected], scenes=[{"nodes": [0]}], scene=0)
    doc.pop("animations", None)
    doc.pop("skins", None)
    return glb(doc, binary)


def generate(check=False):
    source = ROOT / "assets/verda/vehicles/hatchback/compact_classic.glb"
    document, binary = read(source)
    body = next(node for node in document["nodes"] if node.get("name") == "compact_classic")
    wheel = next(node for node in document["nodes"] if node.get("name") == "wheel_FR")
    primitives = document["meshes"][wheel["mesh"]]["primitives"]
    wheel_bounds = document["accessors"][primitives[0]["attributes"]["POSITION"]]
    center = [(lo + hi) / 2 for lo, hi in zip(wheel_bounds["min"], wheel_bounds["max"])]
    rows = [["hatchback", "Classic hatchback", "assets/verda/vehicles/runtime/hatchback_body.glb",
             "assets/verda/vehicles/runtime/hatchback_wheel.glb", 4.2, 1.64, 1.58, .352,
             1.356, -1.126, 1.412, 5, 12, 7, .55, 28, 30, 100, 100, 100, 1,
             -.35, 1.05, .1, 1.65, 0, 6.5, 3, 28, .255]]
    ggbot = ROOT / "assets/verda/vehicles/ggbot"
    models = sorted(ggbot.rglob("*.glb"))
    wheels = [path for path in models if any(word in path.stem.lower()
              for word in ("wheel", "tire", "tyre"))]
    for path in models:
        if path in wheels:
            continue
        relative = path.relative_to(ggbot).with_suffix("").as_posix()
        ident = "ggbot_" + re.sub("[^a-z0-9]+", "_", relative.lower()).strip("_")
        if len(ident) > 80:
            ident = ident[:70] + "_" + hashlib.sha256(relative.encode()).hexdigest()[:8]
        rows.append([ident, path.stem, path.relative_to(ROOT).as_posix(),
                     wheels[0].relative_to(ROOT).as_posix() if wheels else "",
                     4.5, 1.8, 1.6, .35, 1.3, -1.3, 1.5, 4.5, 12, 6, .6, 25, 28,
                     100, 100, 100, 1, -.4, 1.1, 0, 1.8, 0, 7, 3.2, 25, .25])
    manifest = ROOT / "assets/verda/vehicles/vehicle_manifest.tsv"
    if manifest.exists():
        with manifest.open(newline="") as stream:
            existing = list(csv.reader(stream, delimiter="\t"))
        if not existing or existing[0] != FIELDS:
            raise ValueError("Unexpected vehicle manifest schema; existing data was preserved")
        overrides = {}
        for row in existing[1:]:
            if len(row) != len(FIELDS) or row[0] in overrides:
                raise ValueError("Invalid or duplicate vehicle manifest row")
            if not all(math.isfinite(float(value)) for value in row[4:]):
                raise ValueError("Non-finite vehicle tuning")
            overrides[row[0]] = row
        rows = [overrides.pop(str(row[0]), row) for row in rows]
        rows.extend(overrides.values())  # Explicit, data-driven variants are valid definitions too.
    for row in rows:
        for asset in row[2:4]:
            if asset and (not str(asset).startswith("assets/verda/vehicles/") or ".." in asset):
                raise ValueError(f"Invalid vehicle asset path: {asset}")
    hatchback = next(row for row in rows if row[0] == "hatchback")
    body_bounds = [document["accessors"][primitive["attributes"]["POSITION"]]
                   for primitive in document["meshes"][body["mesh"]]["primitives"]]
    source_length = max(bounds["max"][2] for bounds in body_bounds) - min(
        bounds["min"][2] for bounds in body_bounds)
    preview = copy.deepcopy(document)
    ratio = float(hatchback[4]) / source_length
    for node in preview["nodes"]:
        node["translation"] = [value * ratio for value in node.get("translation", [0, 0, 0])]
        node["scale"] = [value * ratio for value in node.get("scale", [1, 1, 1])]
    catalog = ["#pragma once", "#ifdef OUTLAND_DEV_TOOLS",
               '#include "outland/creator/CreatorAssetRegistry.hpp"',
               "namespace outland::creator {",
               "inline void register_vehicle_assets(CreatorAssetRegistry& registry) {"]
    for row in rows:
        ident, name, model, _, length, width, height = row[:7]
        if ident == "hatchback":
            model = "assets/verda/vehicles/runtime/hatchback_preview.glb"
        footprint = ",".join(f"{float(value)}F" for value in (width, length, height))
        catalog.append('(void)registry.add({.id=' + json.dumps("vehicle_" + ident)
                       + ',.name=' + json.dumps(name)
                       + ',.category=CreatorAssetCategory::Vehicle,.model_path=' + json.dumps(model)
                       + ',.thumbnail_path="",.footprint={' + footprint
                       + '},.placement={},.default_scale=1,.tags={"vehicles","vehicle_definition:'
                       + ident + '"}});')
    catalog.extend(["}", "}", "#endif"])
    outputs = {
        "assets/verda/vehicles/runtime/hatchback_body.glb": part(document, binary, body, [0, 0, 0]),
        "assets/verda/vehicles/runtime/hatchback_wheel.glb": part(document, binary, wheel, center),
        "assets/verda/vehicles/runtime/hatchback_preview.glb": glb(preview, binary),
        "assets/verda/vehicles/vehicle_manifest.tsv": ("\t".join(FIELDS) + "\n" + "\n".join(
            "\t".join(map(str, row)) for row in rows) + "\n").encode(),
        "include/outland/creator/VehicleAssetCatalog.hpp": ("\n".join(catalog) + "\n").encode(),
    }
    stale = []
    for relative, data in outputs.items():
        path = ROOT / relative
        if check:
            if not path.exists() or path.read_bytes() != data:
                stale.append(relative)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    if stale:
        raise SystemExit("Stale derived vehicle data: " + ", ".join(stale))
    print(f"Vehicle catalog: {len(rows)} definitions; committed GGBot bodies: {len(models)-len(wheels)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    generate(parser.parse_args().check)
