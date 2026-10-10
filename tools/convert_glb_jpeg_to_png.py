#!/usr/bin/env python3
"""Re-embed a GLB's JPEG textures as PNG.

raylib's default build (SUPPORT_FILEFORMAT_JPG 0), which phone packages commonly use, cannot
decode JPEG, so such textures load as nothing and the model draws white. PNG always loads.
Geometry, materials and every other buffer view are copied byte for byte; only image views change.

    python3 tools/convert_glb_jpeg_to_png.py FILE.glb [...]          # convert in place
    python3 tools/convert_glb_jpeg_to_png.py --check FILE.glb [...]  # fail if any JPEG remains
"""
import io
import json
import struct
import sys

from PIL import Image

GLB_MAGIC, JSON_CHUNK, BIN_CHUNK = 0x46546C67, 0x4E4F534A, 0x004E4942


def read_glb(path):
    data = open(path, "rb").read()
    magic, version, _length = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC or version != 2:
        raise ValueError(f"{path}: not a glTF 2.0 binary")
    offset, doc, binary = 12, None, b""
    while offset < len(data):
        size, kind = struct.unpack_from("<II", data, offset)
        chunk = data[offset + 8:offset + 8 + size]
        if kind == JSON_CHUNK:
            doc = json.loads(chunk)
        elif kind == BIN_CHUNK:
            binary = chunk
        offset += 8 + size
    if doc is None:
        raise ValueError(f"{path}: no JSON chunk")
    return doc, binary


def jpeg_images(doc):
    return [i for i, image in enumerate(doc.get("images", [])) if image.get("mimeType") == "image/jpeg"]


def convert(path):
    doc, binary = read_glb(path)
    targets = set(jpeg_images(doc))
    if not targets:
        return 0
    views = doc["bufferViews"]
    image_view = {doc["images"][i]["bufferView"]: i for i in targets}
    # Rebuild buffer 0 view by view, keeping 4-byte alignment.
    out = bytearray()
    for index, view in enumerate(views):
        if view.get("buffer", 0) != 0:
            continue
        start = view.get("byteOffset", 0)
        payload = binary[start:start + view["byteLength"]]
        if index in image_view:
            picture = Image.open(io.BytesIO(payload))
            picture.load()
            if picture.mode not in ("RGB", "RGBA", "L", "LA"):
                picture = picture.convert("RGB")
            encoded = io.BytesIO()
            picture.save(encoded, format="PNG", optimize=True)
            payload = encoded.getvalue()
        while len(out) % 4:
            out.append(0)
        view["byteOffset"] = len(out)
        view["byteLength"] = len(payload)
        out += payload
    while len(out) % 4:
        out.append(0)
    for i in targets:
        doc["images"][i]["mimeType"] = "image/png"
    doc["buffers"][0]["byteLength"] = len(out)
    text = json.dumps(doc, separators=(",", ":")).encode()
    text += b" " * ((4 - len(text) % 4) % 4)
    total = 12 + 8 + len(text) + 8 + len(out)
    with open(path, "wb") as handle:
        handle.write(struct.pack("<III", GLB_MAGIC, 2, total))
        handle.write(struct.pack("<II", len(text), JSON_CHUNK) + text)
        handle.write(struct.pack("<II", len(out), BIN_CHUNK) + bytes(out))
    return len(targets)


def main(argv):
    check = "--check" in argv
    files = [a for a in argv if a != "--check"]
    if not files:
        print(__doc__)
        return 2
    failed = False
    for path in files:
        if check:
            remaining = jpeg_images(read_glb(path)[0])
            if remaining:
                print(f"{path}: {len(remaining)} JPEG texture(s); run tools/convert_glb_jpeg_to_png.py on it")
                failed = True
        else:
            print(f"{path}: {convert(path)} JPEG texture(s) re-embedded as PNG")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
