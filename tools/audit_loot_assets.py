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
assert {item['category'] for item in items}=={'food','water','medical','ammo','weapon','utility','container','furniture'}
models={entry['model'] for entry in json.loads((PACK/'runtime_manifest.json').read_text())['models']}
models.update(path.relative_to(ROOT).as_posix() for path in (ROOT/'assets/verda/interiors/props').glob('*.glb'))
# Generated combat props (the frag grenade): checked against their generator by CTest.
models.update(path.relative_to(ROOT).as_posix() for path in (ROOT/'assets/verda/combat').glob('*.glb'))
for item in items:
    model=item['model']
    if not model:
        assert item['category']=='weapon' and item['weapon'] in ('pistol','smg','shotgun','sniper','double_barrel','auto_shotgun','rocket_launcher','machine_pistol'),'Only the procedural firearms have no supplied Survival GLB'
        continue
    assert model in models,f'Item is not in a checked runtime pack: {model}'
    document,binary=read_gltf(ROOT/model);low,high=scene_bounds(document)
    assert binary and document.get('meshes') and max(high[i]-low[i] for i in range(3))>1e-6
    if not model.startswith('assets/verda/combat/'):  # generated combat props are vertex-coloured
        assert document.get('images') and all('bufferView' in image and 'uri' not in image for image in document['images']),model
    else:
        assert not document.get('images') and all('COLOR_0' in p['attributes'] for m in document['meshes'] for p in m['primitives']),model
    assert 1<=int(item['stack'])<=999 and int(item['starting_quantity'])>=0
    # `size` is the longest side in metres (the model is scaled uniformly to it): nothing you can
    # pick up off the ground may come out giant.
    size=float(item['size'])
    limit={'food':.3,'water':.35,'medical':.3,'ammo':.3,'weapon':1.3,'utility':2.4,'container':1.0,'furniture':2.9}[item['category']]
    assert .03<=size<=limit,f"{item['id']} is {size} m long (at most {limit} m for {item['category']})"
with (PACK/'gameplay/loot_tables.tsv').open(newline='') as stream:
    tables=list(csv.DictReader(stream,delimiter='\t'))
ids={item['id'] for item in items}
for entry in tables:
    assert entry['item'] in ids and 1<=int(entry['min'])<=int(entry['max'])<=999
    assert int(entry['weight'])>0 and 1<=int(entry['rolls'])<=8
for name in {entry['table'] for entry in tables}:
    assert {'normal','zombie'}<={entry['mode'] for entry in tables if entry['table']==name}<={'normal','zombie','royale'}
    for entry in (e for e in tables if e['table']==name and e['mode']=='normal' and e['item'].endswith('_ammo')):
        zombie=next(e for e in tables if e['table']==name and e['mode']=='zombie' and e['item']==entry['item'])
        assert int(zombie['max'])<int(entry['max']) and int(zombie['weight'])<int(entry['weight'])
print(f'Loot assets: {len(items)} items at real sizes, eight categories, {len({e["table"] for e in tables})} mode-aware tables (Battle Royale arsenal included); supplied models/textures verified.')
