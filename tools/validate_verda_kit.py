#!/usr/bin/env python3
"""Validate exported geometry, LOD budgets, portable material paths and PCM audio."""
import argparse
import json
import math
from pathlib import Path
import struct
import wave


def validate(root):
    kit=root/"assets/verda/starter"
    manifest=json.loads((kit/"manifest.json").read_text())
    checked=0
    for asset in manifest["assets"]:
        counts=[]
        for lod in asset["lods"]:
            path=kit/lod["file"]
            vertices=[];uvs=[];normals=[];faces=[]
            for line in path.read_text().splitlines():
                parts=line.split()
                if not parts: continue
                if parts[0]=="v": vertices.append(tuple(map(float,parts[1:])))
                if parts[0]=="vt": uvs.append(tuple(map(float,parts[1:])))
                if parts[0]=="vn": normals.append(tuple(map(float,parts[1:])))
                if parts[0]=="f": faces.append([tuple(map(int,p.split("/"))) for p in parts[1:]])
            assert len(faces)==lod["triangles"] and faces,path
            assert all(math.isfinite(v) for p in vertices for v in p),path
            assert all(0<u<1 and 0<v<1 for u,v in uvs),path
            assert all(abs(sum(v*v for v in n)-1)<.00001 for n in normals),path
            for face in faces:
                assert len(face)==3,path
                for v,t,n in face:
                    assert 1<=v<=len(vertices) and 1<=t<=len(uvs) and 1<=n<=len(normals),path
                a,b,c=[vertices[v-1] for v,_,_ in face]
                u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
                cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                assert sum(v*v for v in cross)>1e-14,path
                n=normals[face[0][2]-1]
                assert sum(cross[i]*n[i] for i in range(3))>0,path
            counts.append(len(faces));checked+=1
        assert counts[0]>=counts[1]>=counts[2],asset["id"]
        if not asset["id"].startswith("ground_"): assert counts[2]<counts[0],asset["id"]
    assert (kit/"palette.png").read_bytes().startswith(b"\x89PNG\r\n\x1a\n")
    assert "map_Kd palette.png" in (kit/"palette.mtl").read_text()
    audio=root/"assets/audio/environment"
    wavs=list(audio.glob("*.wav"))
    assert len(wavs)==15
    for path in wavs:
        with wave.open(str(path),"rb") as f:
            assert (f.getnchannels(),f.getsampwidth(),f.getframerate())==(1,2,22050),path
            samples=struct.unpack("<"+"h"*f.getnframes(),f.readframes(f.getnframes()))
            assert 0<max(abs(v) for v in samples)<32767,path
            if path.name=="verda_wind.wav":
                assert abs(samples[0]-samples[-1])<100,path
                assert len(samples)==22050*8,path
    html=(kit/"preview.html").read_text()
    assert "__VERDA_" not in html and "https://" not in html
    print(f"[PASS] {checked} OBJ assets: valid indices, nondegenerate triangles, normals, UVs, descending LOD budgets; 15 unclipped WAVs; continuous wind loop; offline preview")


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root",type=Path,default=Path(__file__).resolve().parents[1])
    validate(parser.parse_args().root)
