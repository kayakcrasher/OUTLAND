#!/usr/bin/env python3
"""Add runtime files with proper UTF-8 ZIP names before alignment/signing."""
from pathlib import Path
import argparse,shutil,zipfile

def package(resources,runtime,native,dex,output):
    shutil.copyfile(resources,output)
    with zipfile.ZipFile(output,'a',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=False) as apk:
        for path in sorted(Path(runtime).rglob('*')):
            if path.is_file():apk.write(path,'assets/'+path.relative_to(runtime).as_posix())
        apk.write(native,'lib/arm64-v8a/liboutland.so',compress_type=zipfile.ZIP_STORED)
        apk.write(dex,'classes.dex')
if __name__=='__main__':
    parser=argparse.ArgumentParser()
    for arg in ('resources','runtime','native','dex','output'):parser.add_argument(arg,type=Path)
    args=parser.parse_args();package(**vars(args))
