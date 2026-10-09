#!/usr/bin/env python3
"""Offline Blender conversion: one textured runtime GLB per Survival mesh object.
Run: blender --background --python tools/convert_survival_assets.py
Committed GLBs are ready for Termux; Blender is not a runtime/build dependency.
"""
import hashlib,json,re
from pathlib import Path
import bpy
ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'assets/verda/survival'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(PACK/'Models/Survival.fbx'))
textures={p.name.lower():p for p in (PACK/'Textures').iterdir() if p.is_file()}
resolved=[];missing=[]
for image in bpy.data.images:
    basename=Path(image.filepath.replace('\\','/')).name.lower()
    texture=textures.get(basename) or textures.get(image.name.lower()) or textures.get(image.name.lower()+'.png')
    if texture:
        image.filepath=str(texture);image.reload();resolved.append(texture.name)
    elif image.users:missing.append(image.name)
output=PACK/'runtime';output.mkdir(exist_ok=True)
entries=[]
for obj in sorted(bpy.context.scene.objects,key=lambda o:o.name):
    if obj.type!='MESH':continue
    stem=re.sub(r'[^A-Za-z0-9_.-]+','_',obj.name)
    path=output/(stem+'.glb')
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_animations=False,export_yup=True)
    identifier='survival_'+re.sub(r'[^a-z0-9]+','_',obj.name.lower()).strip('_')+'_'+hashlib.sha256(obj.name.encode()).hexdigest()[:8]
    entries.append({'id':identifier,'name':obj.name.replace('_',' '),'source_object':obj.name,'model':path.relative_to(ROOT).as_posix(),
        'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
report={'source':'assets/verda/survival/Models/Survival.fbx','source_sha256':hashlib.sha256((PACK/'Models/Survival.fbx').read_bytes()).hexdigest(),
    'texture_sha256':{path.name:hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(textures.values())},
    'textures_resolved':sorted(set(resolved)),'unresolved_source_images':sorted(missing),'models':entries}
(PACK/'runtime_manifest.json').write_text(json.dumps(report,indent=2)+'\n')
print('SURVIVAL_RUNTIME='+json.dumps({'models':len(entries),'unresolved':missing}))
