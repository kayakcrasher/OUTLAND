#!/usr/bin/env python3
"""Check gameplay catalog references against the shipped Survival runtime library."""
import csv,json
from pathlib import Path
from gltf_import import read_gltf,scene_bounds
ROOT=Path(__file__).resolve().parents[1]
PACK=ROOT/'assets/verda/survival'
with (PACK/'gameplay/items.tsv').open(newline='') as stream:
    items=list(csv.DictReader(stream,delimiter='\t'))
assert items and len({item['id'] for item in items})==len(items)
assert {item['category'] for item in items}=={'food','water','medical','ammo','weapon','utility','container'}
models={entry['model'] for entry in json.loads((PACK/'runtime_manifest.json').read_text())['models']}
for item in items:
    model=item['model']
    if not model:
        assert item['category']=='weapon' and item['weapon']=='pistol','Only the existing procedural pistol has no supplied Survival GLB'
        continue
    assert model in models,f'Item is not in the checked runtime pack: {model}'
    document,binary=read_gltf(ROOT/model);low,high=scene_bounds(document)
    assert binary and document.get('meshes') and high[1]-low[1]>1e-6
    assert document.get('images') and all('bufferView' in image and 'uri' not in image for image in document['images']),model
    assert 1<=int(item['stack'])<=999 and int(item['starting_quantity'])>=0
with (PACK/'gameplay/loot_tables.tsv').open(newline='') as stream:
    tables=list(csv.DictReader(stream,delimiter='\t'))
ids={item['id'] for item in items}
for entry in tables:
    assert entry['item'] in ids and 1<=int(entry['min'])<=int(entry['max'])<=999
    assert int(entry['weight'])>0 and 1<=int(entry['rolls'])<=8
for name in {entry['table'] for entry in tables}:
    assert {entry['mode'] for entry in tables if entry['table']==name}=={'normal','zombie'}
    for entry in (e for e in tables if e['table']==name and e['mode']=='normal' and e['item'].endswith('_ammo')):
        zombie=next(e for e in tables if e['table']==name and e['mode']=='zombie' and e['item']==entry['item'])
        assert int(zombie['max'])<int(entry['max']) and int(zombie['weight'])<int(entry['weight'])
print(f'Loot assets: {len(items)} items, seven categories, {len({e["table"] for e in tables})} mode-aware tables; supplied models/textures verified.')
