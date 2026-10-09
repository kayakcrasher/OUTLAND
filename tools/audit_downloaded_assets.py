#!/usr/bin/env python3
"""Check runtime model/catalog coverage, binary resources and original textures."""
import argparse
import json
from pathlib import Path
from urllib.parse import unquote
from gltf_import import read_gltf


def audit(root, selected_pack="all"):
    catalog = (root / 'include/outland/creator/CreatorAssetRegistry.hpp').read_text()
    generated = root / 'include/outland/creator/UrbanAssetCatalog.hpp'
    if generated.exists():
        catalog += generated.read_text()
    packs = {'downtown': root / 'assets/verda/creator/downtown', 'urban': root / 'assets/verda/urban'}
    if selected_pack != "all":
        packs = {selected_pack: packs[selected_pack]}
    missing_buffers, missing_textures, unregistered, counts = [], set(), [], {}
    embedded_images = 0
    for name, folder in packs.items():
        models = sorted(p for p in folder.rglob('*') if p.suffix.lower() in ('.gltf', '.glb'))
        counts[name] = len(models)
        for model in models:
            if model.name not in catalog:
                unregistered.append(str(model.relative_to(root)))
            data, binary = read_gltf(model)
            for buffer in data.get('buffers', []):
                uri = buffer.get('uri', '')
                if uri.startswith('data:'):
                    continue
                if not uri and binary is not None:
                    if len(binary) < buffer['byteLength']:
                        missing_buffers.append(str(model.relative_to(root)))
                    continue
                dependency = model.parent / unquote(uri)
                if not uri or not dependency.is_file() or dependency.stat().st_size < buffer['byteLength']:
                    missing_buffers.append(str(dependency.relative_to(root)))
            for image in data.get('images', []):
                uri = image.get('uri', '')
                if uri and not uri.startswith('data:') and not (model.parent / unquote(uri)).is_file():
                    missing_textures.add(str((model.parent / unquote(uri)).relative_to(root)))
                if 'bufferView' in image:
                    view = data['bufferViews'][image['bufferView']]
                    if binary is None or view.get('byteOffset', 0) + view['byteLength'] > len(binary):
                        missing_buffers.append(str(model.relative_to(root)) + ': image buffer')
                    else:
                        embedded_images += 1
    return {'models': sum(counts.values()), 'packs': counts, 'embedded_images': embedded_images,
            'unregistered_models': unregistered, 'missing_or_truncated_buffers': missing_buffers,
            'missing_textures': sorted(missing_textures)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--strict-textures', action='store_true', help='Fail if original textures are absent.')
    parser.add_argument('--pack', choices=('all', 'urban', 'downtown'), default='all')
    parser.add_argument('--json', action='store_true', help='Print complete dependency paths as JSON.')
    args = parser.parse_args()
    result = audit(Path(__file__).resolve().parents[1], args.pack)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"Downloaded assets: {result['models']} models ({result['packs']}); "
              f"{len(result['unregistered_models'])} unregistered; "
              f"{len(result['missing_or_truncated_buffers'])} missing/truncated buffers; "
              f"{result['embedded_images']} embedded images; "
              f"{len(result['missing_textures'])} missing texture paths.")
        if result['missing_textures']:
            print('Use --json for missing source paths; --strict-textures to enforce completeness.')
    return int(not result['models'] or bool(result['unregistered_models']) or
               bool(result['missing_or_truncated_buffers']) or
               (args.strict_textures and bool(result['missing_textures'])))


if __name__ == '__main__':
    raise SystemExit(main())
