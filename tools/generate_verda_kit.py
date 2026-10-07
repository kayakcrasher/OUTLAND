#!/usr/bin/env python3
"""Generate original, palette-mapped Verda OBJ assets, LODs and offline previews.

Standard library only. Units: metres, Y up, front -Z, origin at ground centre.
This is a shape/material blockout kit; textured production assets can replace it.
"""
import argparse
import json
import math
from pathlib import Path
import random
import struct
import zlib

PALETTE = {
    "grass": (112, 151, 77), "leaf": (103, 157, 67),
    "leaf_light": (128, 176, 77), "leaf_dark": (79, 133, 58),
    "bark": (115, 83, 52), "dirt": (161, 128, 83),
    "stone": (148, 151, 130), "stone_dark": (113, 119, 109),
    "plaster": (232, 219, 182), "blue": (165, 199, 204),
    "roof": (157, 79, 54), "trim": (78, 74, 66),
    "glass": (83, 119, 135), "wood": (126, 99, 65),
    "flower": (240, 217, 99), "metal": (104, 121, 119),
}


def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def unit(v):
    length = math.sqrt(sum(x*x for x in v))
    if length < 1e-10:
        raise ValueError("Degenerate geometry")
    return tuple(x/length for x in v)


class Mesh:
    def __init__(self):
        self.faces = []

    def tri(self, a, b, c, material):
        unit(cross(sub(b, a), sub(c, a)))  # Reject degenerate faces at generation time.
        self.faces.append(([a, b, c], material))

    def quad(self, a, b, c, d, material):
        self.tri(a, b, c, material)
        self.tri(a, c, d, material)

    def box(self, center, size, material):
        x, y, z = center
        w, h, d = (s/2 for s in size)
        v = [(x-w,y-h,z-d), (x+w,y-h,z-d), (x+w,y+h,z-d), (x-w,y+h,z-d),
             (x-w,y-h,z+d), (x+w,y-h,z+d), (x+w,y+h,z+d), (x-w,y+h,z+d)]
        for a,b,c,d in [(0,3,2,1),(4,5,6,7),(0,4,7,3),(1,2,6,5),(3,7,6,2),(0,1,5,4)]:
            self.quad(v[a],v[b],v[c],v[d],material)

    def ellipsoid(self, center, radius, rings, sides, material, seed=0, rough=0):
        rng = random.Random(seed)
        cx,cy,cz = center
        rx,ry,rz = radius
        top, bottom = (cx,cy+ry,cz), (cx,cy-ry,cz)
        rows = []
        for i in range(1, rings):
            phi = math.pi*i/rings
            row = []
            for j in range(sides):
                theta = 2*math.pi*j/sides
                r = 1 + rng.uniform(-rough,rough)
                row.append((cx+rx*math.sin(phi)*math.cos(theta)*r,
                            cy+ry*math.cos(phi)*r,
                            cz+rz*math.sin(phi)*math.sin(theta)*r))
            rows.append(row)
        for j in range(sides):
            k = (j+1) % sides
            self.tri(top, rows[0][k], rows[0][j], material)
            self.tri(bottom, rows[-1][j], rows[-1][k], material)
        for a,b in zip(rows,rows[1:]):
            for j in range(sides):
                k = (j+1) % sides
                self.quad(a[j],a[k],b[k],b[j],material)

    def branch(self, start, end, radius, tip_radius, sides, material):
        axis = unit(sub(end,start))
        u = unit(cross(axis, (0,0,1) if abs(axis[2]) < .9 else (1,0,0)))
        v = cross(axis,u)
        rows = []
        for center,r in [(start,radius),(end,tip_radius)]:
            rows.append([tuple(center[k]+r*(u[k]*math.cos(j*2*math.pi/sides)+
                         v[k]*math.sin(j*2*math.pi/sides)) for k in range(3)) for j in range(sides)])
        for j in range(sides):
            k = (j+1) % sides
            self.quad(rows[0][j],rows[0][k],rows[1][k],rows[1][j],material)
            self.tri(start,rows[0][k],rows[0][j],material)
            self.tri(end,rows[1][j],rows[1][k],material)

    def blade(self, x, z, height, width, lean, material):
        a,b,c = (x-width,0,z),(x+width,0,z),(x+lean,height,z+.04)
        self.tri(a,b,c,material)
        self.tri(c,b,a,material)  # Opaque, double-sided geometry; no alpha sorting.

    def export(self, path):
        orientation = "+Z; origin muzzle" if path.stem.startswith("weapon_") else "-Z; origin ground centre"
        lines = ["# Original OUTLAND Verda starter kit; metres; Y-up; front "+orientation,
                 "mtllib palette.mtl", "o "+path.stem, "usemtl verda_palette", "s off"]
        keys = list(PALETTE)
        for vertices, material in self.faces:
            for p in vertices:
                lines.append("v "+" ".join(f"{x:.6f}" for x in p))
        for _, material in self.faces:
            i = keys.index(material)
            uv = ((i%4+.5)/4, 1-(i//4+.5)/4)
            for _ in range(3):
                lines.append(f"vt {uv[0]:.6f} {uv[1]:.6f}")
        for vertices,_ in self.faces:
            normal = unit(cross(sub(vertices[1],vertices[0]),sub(vertices[2],vertices[0])))
            lines.append("vn "+" ".join(f"{x:.6f}" for x in normal))
        for i in range(len(self.faces)):
            lines.append("f "+" ".join(f"{i*3+j+1}/{i*3+j+1}/{i+1}" for j in range(3)))
        path.write_text("\n".join(lines)+"\n")


def asset(name, lod):
    m = Mesh()
    rings, sides = [(6,8),(4,6),(3,5)][lod]
    if name.startswith("grass") or name == "weed":
        rng = random.Random(31 if name == "grass_field" else 73)
        count = ([9,5,2] if name.startswith("grass") else [7,4,2])[lod]
        for i in range(count):
            x,z = rng.uniform(-.26,.26),rng.uniform(-.22,.22)
            h = rng.uniform(.26,.62) if name.startswith("grass") else rng.uniform(.6,1.15)
            m.blade(x,z,h,.025 if name=="weed" else .04,rng.uniform(-.16,.16),
                    "leaf_light" if i%3==0 else "grass")
    elif name.startswith("bush"):
        centers = [(0,.48,0),(-.38,.34,.15),(.4,.4,-.12)] if name == "bush_round" else [(0,.32,0),(-.65,.25,.2),(.62,.28,-.2)]
        radius = (.65,.48,.55) if name == "bush_round" else (.8,.32,.58)
        for i,center in enumerate(centers[:[3,2,1][lod]]):
            m.ellipsoid(center,radius,rings,sides,
                        ["leaf","leaf_dark","leaf_light"][i])
    elif name.startswith("rock"):
        radius = (1.25,.95,.9) if name=="rock_boulder" else (.55,.34,.42)
        # Bottom pole at Y=0, with irregular but grounded outline.
        m.ellipsoid((0,radius[1],0),radius,rings,sides,"stone",seed=17,rough=.12)
    elif name.startswith("tree"):
        scale = 1.0 if name=="tree_oak" else 1.18
        m.branch((0,0,0),(.18,3.5*scale,0),.35,.16,sides,"bark")
        if lod == 0:
            for side in [-1,1]:
                m.branch((0,2.1*scale,0),(side*1.1,3.4*scale,.3*side),.14,.06,5,"bark")
        centers = [(0,4.4*scale,0),(-1,3.6*scale,.35),(1.1,3.8*scale,-.25)]
        for i,center in enumerate(centers[:[3,2,1][lod]]):
            r = [1.6,1.25,1.2][i]
            m.ellipsoid(center,(r,r*.85,r),rings,sides,["leaf","leaf_dark","leaf_light"][i])
    elif name == "house_rural":
        m.box((0,.22,0),(8.25,.44,7.25),"stone_dark")
        m.box((0,2.44,0),(8,4,7),"plaster")
        v = [(-4.35,4.44,-3.85),(4.35,4.44,-3.85),(4.35,4.44,3.85),
             (-4.35,4.44,3.85),(0,6.2,-3.85),(0,6.2,3.85)]
        for i,face in enumerate([(0,4,1),(3,2,5),(0,3,5),(0,5,4),(1,4,5),(1,5,2),(0,1,2),(0,2,3)]):
            m.tri(*(v[j] for j in face),"plaster" if i<2 else "roof")
        if lod < 2:
            m.box((0,1.6,-3.59),(1.35,2.35,.15),"trim")
            m.box((0,1.6,-3.69),(1.12,2.12,.08),"wood")
            for x in [-2.32,2.32]:
                m.box((x,1.75,-3.59),(1.43,1.53,.12),"trim")
                m.box((x,1.75,-3.67),(1.25,1.35,.06),"glass")
                if lod == 0:
                    m.box((x,1.75,-3.73),(.07,1.35,.04),"trim")
                    m.box((x,1.04,-3.74),(1.55,.12,.32),"stone")
            m.box((1.92,5.88,1.4),(.65,1.92,.65),"stone_dark")
        if lod == 0:
            m.box((0,.11,-4.11),(1.8,.22,1.1),"stone")
            m.box((0,.33,-3.81),(1.5,.22,.5),"stone")
            for x in [-3.92,3.92]:
                for z in [-3.42,3.42]:
                    m.box((x,2.44,z),(.2,4,.2),"trim")
            for x in [-4.35,4.35]:
                m.box((x,4.44,0),(.12,.18,7.7),"trim")
    elif name == "weapon_rifle":
        # Attachment pivot at muzzle; barrel points +Z, Y up.
        m.box((0,0,-.47),(.13,.15,.34),"metal")
        m.box((0,0,-.24),(.12,.12,.22),"trim")
        m.branch((0,0,-.13),(0,0,0),.022,.022,sides,"metal")
        m.box((0,-.14,-.49),(.075,.23,.12),"trim")
        m.box((0,-.12,-.36),(.07,.22,.10),"trim")
        m.box((0,-.02,-.74),(.12,.18,.22),"trim")
        if lod < 2:
            m.box((0,.09,-.46),(.08,.035,.31),"stone")
        if lod == 0:
            m.box((0,.12,-.59),(.018,.04,.035),"trim")
            m.box((0,.12,-.18),(.018,.04,.025),"trim")
            m.box((.07,0,-.45),(.015,.055,.09),"trim")
            m.box((.083,.01,-.42),(.04,.022,.022),"stone")
    elif name == "weapon_pistol":
        m.box((0,.015,-.15),(.09,.10,.28),"metal")
        m.box((0,-.11,-.23),(.075,.19,.10),"trim")
        if lod < 2:
            m.box((0,.076,-.25),(.018,.025,.03),"trim")
        if lod == 0:
            m.box((0,.076,-.04),(.015,.025,.02),"trim")
            m.box((.048,.02,-.13),(.012,.035,.05),"trim")
    elif name in ["ground_grass","ground_dirt"]:
        material = "grass" if name=="ground_grass" else "dirt"
        # Tile snaps to a 4m grid; scatter props supply the next ground layer.
        m.quad((-2,0,-2),(-2,0,2),(2,0,2),(2,0,-2),material)
    elif name == "pebble_scatter":
        rng = random.Random(82)
        for i in range([7,4,2][lod]):
            x,z = rng.uniform(-1,1),rng.uniform(-1,1)
            r = rng.uniform(.05,.13)
            m.ellipsoid((x,r*.55,z),(r,r*.55,r*.8),3,5,"stone" if i%2 else "stone_dark")
    else:
        raise ValueError(name)
    return m


NAMES = ["ground_grass","ground_dirt","grass_field","grass_verge","weed","pebble_scatter",
         "bush_round","bush_scrub","rock_field","rock_boulder","tree_oak","tree_tall","house_rural","weapon_pistol","weapon_rifle"]


def write_palette(path):
    def chunk(kind,data):
        return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data)&0xffffffff)
    colors = list(PALETTE.values())
    # 64px solid swatches, all UVs at centres; intended for nearest filtering.
    raw = b"".join(b"\0"+b"".join(bytes(colors[(y//16)*4+x//16]) for x in range(64)) for y in range(64))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",64,64,8,2,0,0,0))+
                     chunk(b"IDAT",zlib.compress(raw,9))+chunk(b"IEND",b""))


def preview(mesh, x, y, width, height):
    def project(p):
        return ((-p[0]+p[2])*.7071,-(p[0]+p[2])*.32-p[1]*.89)
    points = [project(p) for face,_ in mesh.faces for p in face]
    xmin,xmax = min(p[0] for p in points),max(p[0] for p in points)
    ymin,ymax = min(p[1] for p in points),max(p[1] for p in points)
    scale = min(width/max(xmax-xmin,.01),height/max(ymax-ymin,.01))
    result=[]
    for face,material in sorted(mesh.faces,key=lambda f: sum(p[0]+p[2]-p[1]*.72 for p in f[0])/3,reverse=True):
        normal=unit(cross(sub(face[1],face[0]),sub(face[2],face[0])))
        if sum(a*b for a,b in zip(normal,(-.63,.45,-.63))) <= 0:
            continue
        light = .75+.25*max(0,sum(a*b for a,b in zip(normal,(-.35,.85,-.4))))
        color="#"+ "".join(f"{int(c*light):02x}" for c in PALETTE[material])
        coordinates=[]
        for p in face:
            px,py=project(p)
            coordinates.append(f"{x+width/2+(px-(xmin+xmax)/2)*scale:.2f},{y+(py-ymin)*scale:.2f}")
        result.append(f'<polygon points="{" ".join(coordinates)}" fill="{color}"/>')
    return "".join(result)


def generate(output):
    output.mkdir(parents=True,exist_ok=True)
    write_palette(output/"palette.png")
    (output/"palette.mtl").write_text("newmtl verda_palette\nKd 1 1 1\nKa 1 1 1\nKs 0 0 0\nd 1\nillum 1\nmap_Kd palette.png\n")
    manifest={"units":"metres","up_axis":"Y","front":"-Z","origin":"ground centre; weapon pivots at muzzle, barrel +Z",
              "material":"palette.mtl","assets":[]}
    preview_data={}
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1280" height="1140" viewBox="0 0 1280 1140">',
         '<rect width="1280" height="1140" fill="#f2eee3"/>',
         '<text x="40" y="52" font-family="sans-serif" font-size="28" fill="#283b32">OUTLAND / VERDA — ORIGINAL STARTER KIT</text>',
         '<text x="40" y="80" font-family="sans-serif" font-size="16" fill="#62705e">Shape and material study · metres · palette mapped · three LODs · environment and weapon blockouts</text>']
    for i,name in enumerate(NAMES):
        meshes=[asset(name,lod) for lod in range(3)]
        preview_data[name]=[m.faces for m in meshes]
        entry={"id":name,"front":"+Z" if name.startswith("weapon_") else "-Z",
               "origin":"muzzle" if name.startswith("weapon_") else "ground centre","lods":[]}
        for lod,m in enumerate(meshes):
            filename=f"{name}_lod{lod}.obj"
            m.export(output/filename)
            entry["lods"].append({"file":filename,"triangles":len(m.faces)})
        points=[p for f,_ in meshes[0].faces for p in f]
        entry["bounds"]={"min":[min(p[k] for p in points) for k in range(3)],
                         "max":[max(p[k] for p in points) for k in range(3)]}
        manifest["assets"].append(entry)
        x,y=40+(i%4)*305,110+(i//4)*250
        svg.append(f'<rect x="{x}" y="{y}" width="285" height="232" rx="12" fill="#e4e8d6"/>')
        svg.append(preview(meshes[0],x+15,y+10,255,174))
        svg.append(f'<text x="{x+12}" y="{y+205}" font-family="sans-serif" font-size="17" fill="#283b32">{name.replace("_"," ")}</text>')
        counts="/".join(str(len(m.faces)) for m in meshes)
        svg.append(f'<text x="{x+12}" y="{y+224}" font-family="sans-serif" font-size="12" fill="#62705e">LOD0 / 1 / 2: {counts} triangles</text>')
    svg.append("</svg>")
    (output/"contact_sheet.svg").write_text("\n".join(svg))
    (output/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
    template=Path(__file__).with_name("verda_preview.html").read_text()
    (output/"preview.html").write_text(template.replace("__VERDA_DATA__",json.dumps(preview_data,separators=(",",":")))
                                     .replace("__VERDA_PALETTE__",json.dumps(PALETTE)))
    print(f"Generated {len(NAMES)*3} OBJ files, palette, manifest and offline previews in {output}")


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,default=Path(__file__).resolve().parents[1]/"assets/verda/starter")
    generate(parser.parse_args().output)
