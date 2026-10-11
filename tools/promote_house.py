import bpy
from pathlib import Path

root = Path.home() / "Outland"
source = root / "assets/source/elbolilloduro/house/House"
models = source / "Models"
output = root / "assets/verda/residential/houses/source"  # editor source; tools/extract_house.py makes the runtime houses
output.mkdir(parents=True, exist_ok=True)

textures = {}
for path in source.rglob("*"):
    if path.suffix.lower() in (".png", ".jpg", ".jpeg"):
        textures[path.name.lower()] = path

def relink():
    count = 0
    for image in bpy.data.images:
        if image.source != "FILE":
            continue

        name = Path(bpy.path.basename(image.filepath)).name
        if not name:
            name = image.name

        match = textures.get(name.lower())

        if match:
            image.filepath = str(match)
            image.reload()
            image.pack()
            count += 1

    print("TEXTURES RELINKED:", count)

def convert(filename, target):
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    print("IMPORTING:", filename)

    bpy.ops.wm.fbx_import(filepath=str(models / filename))

    relink()

    bpy.ops.export_scene.gltf(
        filepath=str(output / target),
        export_format="GLB",
        export_apply=True
    )

    print("EXPORTED:", target)

convert("House.fbx", "house.glb")
convert("House_Colliders.fbx", "house_colliders.glb")

print("=== HOUSE PROMOTION GREEN ===")
