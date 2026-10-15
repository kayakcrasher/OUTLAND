#!/usr/bin/env python3
"""Artifact checks do not launch the game or substitute for device testing."""
import argparse,json,re,struct,zipfile
from pathlib import Path

def android(path):
    with zipfile.ZipFile(path) as apk:
        names=set(apk.namelist());assert 'classes.dex' in names
        assert 'AndroidManifest.xml' in names
        assert 'resources.arsc' in names and any('ic_launcher_foreground' in n for n in names),'Missing the launcher icon resources'
        library=apk.read('lib/arm64-v8a/liboutland.so')
        assert library[:4]==b'\x7fELF' and struct.unpack_from('<H',library,18)[0]==183
        offset=struct.unpack_from('<Q',library,32)[0];size,count=struct.unpack_from('<HH',library,54)
        for i in range(count):
            header=offset+i*size
            if struct.unpack_from('<I',library,header)[0]==1:
                assert struct.unpack_from('<Q',library,header+48)[0]>=16384,'ELF segments need 16 KiB alignment'
        manifest=apk.read('assets/runtime.manifest').decode().splitlines()
        assert len(manifest)>1000
        for file in manifest[1:]:assert 'assets/'+file in names,file
        assert not any('/source/' in n or n.endswith(('.fbx','.zip','.blend')) for n in names)
        print(f'APK verified: ARM64/16-KiB ELF, DEX, manifest, {len(manifest)-1} runtime files')
def web(directory):
    root=Path(directory)
    for suffix in ('html','js','wasm','data'):assert (root/('outland.'+suffix)).stat().st_size>0
    assert (root/'outland.wasm').read_bytes()[:8]==b'\0asm\x01\0\0\0'
    report=json.loads((root/'runtime-report.json').read_text())
    assert (root/'outland.data').stat().st_size==report['bytes']
    html=(root/'outland.html').read_text();js=(root/'outland.js').read_text()
    assert 'Rotate your phone' in html and 'touch-action:none' in html
    for icon in ('icon-32.png','icon-180.png','icon-192.png','icon-512.png','manifest.webmanifest'):assert (root/icon).stat().st_size>0,f'Missing app icon file: {icon}'
    assert 'apple-touch-icon' in html
    assert 'IDBFS' in js and 'requestAnimationFrame' in js
    for method in ('callMain','FS','ccall'):
        assert re.search(r'Module\s*(?:\[\s*[\"\x27]'+method+r'[\"\x27]\s*\]|\.'+method+r')\s*=',js),f'Missing exported runtime method: {method}'
    print(f'Web verified: HTML/JS/WASM, {report["bytes"]} preloaded bytes, IndexedDB and frame pacing')
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--apk',type=Path);parser.add_argument('--web',type=Path)
    args=parser.parse_args()
    if args.apk:android(args.apk)
    if args.web:web(args.web)
