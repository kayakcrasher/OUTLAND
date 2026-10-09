#!/usr/bin/env python3
"""Offline Blender conversion of Radiation Workers into the existing character pipeline.
Run: blender --background --python tools/convert_hazmat_characters.py
The TSV remains authoritative; this converter does not add or change manifest rows.
"""
import ctypes,hashlib,json,math,tempfile
from pathlib import Path
import bpy
ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'assets/verda/characters'
SOURCE=PACK/'hazmat/Radiation Workers – Retro PSX Character Pack'
outputs=[]
for filename,folder in [('heavy radiation suit.fbx','Heavy hazmat suit'),('light hazmat suit.fbx','Light radiation suit')]:
    source=SOURCE/filename
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with tempfile.TemporaryDirectory(prefix='outland-hazmat-') as temporary:
        # Blender does not import ASCII FBX. Assimp preserves its scene/skin while
        # translating to GLB for Blender's normal material/export pipeline.
        library=ctypes.CDLL('libassimp.so.5')
        library.aiImportFile.argtypes=[ctypes.c_char_p,ctypes.c_uint];library.aiImportFile.restype=ctypes.c_void_p
        library.aiExportScene.argtypes=[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_char_p,ctypes.c_uint]
        library.aiReleaseImport.argtypes=[ctypes.c_void_p]
        scene=library.aiImportFile(str(source).encode(),0)
        if not scene:raise RuntimeError(f'Cannot import {source}')
        converted=Path(temporary)/'source.glb'
        try:
            if library.aiExportScene(scene,b'glb2',str(converted).encode(),0)!=0:raise RuntimeError('ASCII FBX conversion failed')
        finally:library.aiReleaseImport(scene)
        bpy.ops.import_scene.gltf(filepath=str(converted))
    textures={p.name.lower():p for p in (SOURCE/'Textures'/folder).glob('*.png')}
    resolved=[]
    for image in bpy.data.images:
        if not image.users:continue
        name=Path(image.filepath.replace('\\','/')).name.lower()
        texture=textures.get(name) or textures.get(image.name.lower())
        if not texture:raise RuntimeError(f'Unresolved used image: {source.name}: {image.name} ({image.filepath})')
        image.filepath=str(texture);image.reload();resolved.append(texture)
    output=PACK/'runtime'/source.relative_to(PACK).with_suffix('.glb')
    output.parent.mkdir(parents=True,exist_ok=True)
    # Source has no authored actions; do not fabricate an animation from the bind pose.
    bpy.ops.export_scene.gltf(filepath=str(output),export_format='GLB',export_animations=False,export_yup=True)
    outputs.append({'source':source.relative_to(ROOT).as_posix(),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'runtime_model':output.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
        'texture_sha256':{p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(resolved))}})
(PACK/'hazmat/runtime_conversion.json').write_text(json.dumps(outputs,indent=2)+'\n')
print('HAZMAT_RUNTIME='+json.dumps(outputs))
