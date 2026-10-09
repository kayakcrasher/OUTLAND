"""Canonical offline interpretation of the production TSV (no directory discovery)."""
import csv
from pathlib import Path

PACK = 'assets/verda/characters/'

def runtime_path(model):
    path=Path(model)
    if path.suffix!='.fbx': return path.as_posix()
    glb=path.with_suffix('.glb').as_posix()
    return glb if glb.startswith(PACK+'first_person_arms/') else PACK+'runtime/'+glb[len(PACK):]

def read_manifest(root):
    result={}
    with (Path(root)/PACK/'character_manifest.tsv').open(encoding='utf-8-sig',newline='') as stream:
        reader=csv.DictReader(stream,delimiter='\t')
        if reader.fieldnames!=['id','name','category','role','model','texture','source']:
            raise ValueError('Invalid character manifest header')
        for row in reader:
            if None in row or any(value is None for value in row.values()): raise ValueError('Invalid TSV columns')
            if not row['id'] or not row['name'] or not row['source']: raise ValueError('Empty character identity')
            for path in (row['model'],row['texture']):
                if path and (not path.startswith(PACK) or '\\' in path or '..' in Path(path).parts or '.' in path.split('/')):
                    raise ValueError(f'Unsafe pack path: {path}')
            if Path(row['model']).suffix not in ('.fbx','.glb','.gltf'): raise ValueError('Unsupported source model')
            category=row['category']
            if category not in ('civilian','emergency','hostile','creature','player_arms') and not category.startswith('emergency_'):
                raise ValueError(f'Unknown category: {category}')
            if row['role'] not in (('first_person','legacy_first_person') if category=='player_arms' else ('npc',)):
                raise ValueError('Incompatible character role')
            row['runtime_model']=runtime_path(row['model'])
            previous=result.get(row['id'])
            if previous:
                if row['model']==previous['model'] or any(row[key]!=previous[key] for key in row if key!='model'):
                    raise ValueError(f'Conflicting character ID: {row["id"]}')
                if Path(row['model']).suffix=='.fbx': continue
            result[row['id']]=row
    if not result or not any(row['category']=='civilian' for row in result.values()): raise ValueError('No civilian player body')
    return sorted(result.values(),key=lambda row:row['id'])
