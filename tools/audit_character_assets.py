#!/usr/bin/env python3
"""Headless manifest/runtime/dependency/rig audit for the production library."""
from collections import Counter
from pathlib import Path
import hashlib
import json
from character_manifest import read_manifest
from gltf_import import read_gltf, scene_bounds
ROOT=Path(__file__).resolve().parents[1]
rows=read_manifest(ROOT)
index=json.loads((ROOT/'assets/verda/characters/runtime/production_index.json').read_text())
assert set(index)=={row['id'] for row in rows}, 'Runtime index is stale relative to manifest'
images=clips=0
warnings=[]
def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
for row in rows:
    model=ROOT/row['runtime_model']; source=ROOT/row['model']; texture=ROOT/row['texture'] if row['texture'] else None
    entry=index[row['id']]
    assert entry['source_model']==row['model'] and entry['runtime_model']==row['runtime_model'], row['id']
    assert source.exists() and model.exists(), f'Missing source/runtime: {row["id"]}'
    assert entry['source_sha256']==digest(source) and entry['runtime_sha256']==digest(model), f'Stale conversion: {row["id"]}'
    assert entry['texture_sha256']==(digest(texture) if texture else None), f'Stale diffuse texture: {row["id"]}'
    for path,expected in entry['dependencies'].items():
        assert path.startswith('assets/verda/characters/') and '..' not in Path(path).parts, row['id']
        assert expected and digest(ROOT/path)==expected, f'Stale referenced image: {path}'
    document,binary=read_gltf(model)
    low,high=scene_bounds(document)
    assert binary and document.get('meshes'), f'Missing geometry: {model}'
    for buffer in document.get('buffers',[]):
        assert 'uri' not in buffer and buffer['byteLength']<=len(binary), model
    for image in document.get('images',[]):
        assert 'bufferView' in image and 'uri' not in image, model
        view=document['bufferViews'][image['bufferView']]
        assert view.get('byteOffset',0)+view['byteLength']<=len(binary), model
    for skin in document.get('skins',[]):
        assert skin['joints'] and len(set(skin['joints']))==len(skin['joints']), f'Invalid rig: {model}'
        assert all(0<=joint<len(document['nodes']) for joint in skin['joints']), model
        for primitive in (part for mesh in document['meshes'] for part in mesh['primitives']):
            attrs=primitive['attributes']
            assert ('JOINTS_0' in attrs)==('WEIGHTS_0' in attrs), f'Incomplete skin attributes: {model}'
    if texture and not texture.exists(): warnings.append(f"{row['id']}: manifest diffuse texture absent: {row['texture']}")
    if not document.get('images'):
        # Allow only source-declared no-texture variants, never a missing supplied texture.
        assert not texture or not texture.exists(), f'Texture lost during conversion: {row["id"]}'
        warnings.append(f"{row['id']}: supplied material colors, no image")
    if row['role']=='npc':
        height=high[2]-low[2] if row['source']=='elbolilloduro_characters_psx' else high[1]-low[1]
        assert height>.00001, f'Invalid body height: {row["id"]}'
    images+=len(document.get('images',[])); clips+=len(document.get('animations',[]))
print(f'Character assets: {len(rows)} manifest IDs, {images} embedded images, {clips} preserved clips; hashes, rigs and embedded dependencies verified.')
print('Categories:',dict(Counter(row['category'] for row in rows)))
for warning in warnings: print('KNOWN SOURCE GAP:',warning)
