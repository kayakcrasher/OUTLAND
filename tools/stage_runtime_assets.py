#!/usr/bin/env python3
"""Stage production assets for APK/MEMFS; never package download archives/editor sources."""
import argparse, hashlib, json, shutil
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = {'.glb','.gltf','.bin','.obj','.mtl','.png','.jpg','.jpeg','.wav','.ogg','.ttf','.otf','.tsv','.json','.map','.txt','.md'}
def runtime_files(root=ROOT):
    for folder in ('assets','maps'):
        for path in sorted((root/folder).rglob('*')):
            relative = path.relative_to(root)
            if path.is_file() and path.suffix.lower() in EXTENSIONS and not {'source','staging'}.intersection(relative.parts):
                yield relative

def stage(destination, root=ROOT):
    destination=Path(destination).resolve()
    if destination==root or destination in root.parents or (root in destination.parents and {'assets','maps'}.intersection(destination.relative_to(root).parts)):
        raise ValueError('Staging must not overwrite production assets')
    if destination.exists():
        if any(destination.iterdir()) and not (destination/'runtime.manifest').is_file():
            raise ValueError('Refusing to replace a directory that is not an OUTLAND runtime package')
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    digest=hashlib.sha256(); size=0; files=list(runtime_files(root))
    for relative in files:
        source=root/relative; target=destination/relative
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,target)
        digest.update(relative.as_posix().encode());digest.update(source.read_bytes());size+=source.stat().st_size
    version=digest.hexdigest()
    (destination/'runtime.manifest').write_text(version+'\n'+'\n'.join(p.as_posix() for p in files)+'\n')
    result={'version':version,'files':len(files),'bytes':size}
    (destination/'runtime-report.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result));return result
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('destination',type=Path)
    stage(parser.parse_args().destination)
