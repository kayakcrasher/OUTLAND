# Run with: blender -b --python tools/convert_character_assets.py
# The TSV selects sources, textures, categories and roles. Blender is an offline tool only.
import bpy
import hashlib
import json
import pathlib
import sys
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from character_manifest import read_manifest
root=pathlib.Path(__file__).resolve().parents[1]
index_path=root/'assets/verda/characters/runtime/production_index.json'
previous=json.loads(index_path.read_text()) if index_path.exists() else {}
index={}
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
for row in read_manifest(root):
    source=root/row['model']; output=root/row['runtime_model']
    texture=root/row['texture'] if row['texture'] else None
    inputs={'source_sha256':digest(source),'texture_sha256':digest(texture) if texture else None}
    if not source.exists(): raise RuntimeError(f'Missing character source: {source}')
    old=previous.get(row['id'],{})
    dependencies=old.get('dependencies')
    dependencies_current=dependencies is not None and all(digest(root/path)==value for path,value in dependencies.items())
    current=output.exists() and dependencies_current and all(old.get(key)==value for key,value in inputs.items()) and old.get('runtime_sha256')==digest(output)
    if source.suffix.lower()=='.fbx' and not current:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(source))
        if texture and texture.exists():
            image=bpy.data.images.load(str(texture))
            for obj in bpy.context.scene.objects:
                if obj.type!='MESH': continue
                if not obj.data.materials: obj.data.materials.append(bpy.data.materials.new('Character'))
                for material in obj.data.materials:
                    if material is None: continue
                    material.use_nodes=True
                    bsdf=next((node for node in material.node_tree.nodes if node.type=='BSDF_PRINCIPLED'),None)
                    if bsdf:
                        node=material.node_tree.nodes.new('ShaderNodeTexImage'); node.image=image
                        material.node_tree.links.new(node.outputs['Color'],bsdf.inputs['Base Color'])
        # Some source FBX images reference the artist's C: drive. Resolve by unique
        # shipped filename, never by substituting another character's texture.
        for image in bpy.data.images:
            if image.source!='FILE': continue
            name=pathlib.Path(image.filepath.replace('\\','/')).name
            candidate=source.parent/name
            if not candidate.exists():
                matches=list((root/'assets/verda/characters').rglob(name))
                if len(matches)==1: candidate=matches[0]
            if not pathlib.Path(bpy.path.abspath(image.filepath)).exists() and candidate.exists(): image.filepath=str(candidate)
        dependencies={}
        for image in bpy.data.images:
            if image.source!='FILE': continue
            image_path=pathlib.Path(bpy.path.abspath(image.filepath)).resolve()
            if image_path.exists():
                relative=image_path.relative_to(root).as_posix()
                if not relative.startswith('assets/verda/characters/'):
                    raise RuntimeError(f'Character texture outside production pack: {image_path}')
                dependencies[relative]=digest(image_path)
        output.parent.mkdir(parents=True,exist_ok=True)
        bpy.ops.export_scene.gltf(filepath=str(output),export_format='GLB',export_animations=True)
        print('OUTLAND_CONVERTED',row['id'],flush=True)
    if not output.exists(): raise RuntimeError(f'Missing runtime character: {output}')
    index[row['id']]={'source_model':row['model'],'runtime_model':row['runtime_model'],**inputs,'dependencies':dependencies or {},'runtime_sha256':digest(output)}
index_path.parent.mkdir(parents=True,exist_ok=True)
index_path.write_text(json.dumps(index,indent=2,sort_keys=True)+'\n')
