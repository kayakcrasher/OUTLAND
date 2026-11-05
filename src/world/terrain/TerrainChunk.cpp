#include "outland/world/terrain/TerrainChunk.hpp"

#include "outland/world/terrain/TerrainHeight.hpp"

#include "outland/engine/render/WorldShading.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include "outland/world/Highways.hpp"

namespace outland::world::terrain {

TerrainChunk::TerrainChunk(
    const int chunk_x,
    const int chunk_z,
    const float size,
    const int resolution
)
    : chunk_x_(chunk_x),
      chunk_z_(chunk_z),
      size_(size),
      resolution_(resolution) {
    build();
}

TerrainChunk::~TerrainChunk() {
    if (loaded_) {
        UnloadModel(model_);
    }
}

void TerrainChunk::build() {
    // Shared grid vertices: sample each terrain point once, not six times per cell.
    resolution_=std::clamp(resolution_,1,254); // unsigned-short index limit
    if(size_<=0) size_=128;
    const int cells=resolution_,side=cells+1;
    Mesh mesh{};
    mesh.vertexCount=side*side;
    mesh.triangleCount=cells*cells*2;
    mesh.vertices=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));
    mesh.normals=static_cast<float*>(MemAlloc(mesh.vertexCount*3*sizeof(float)));
    mesh.texcoords=static_cast<float*>(MemAlloc(mesh.vertexCount*2*sizeof(float)));
    mesh.indices=static_cast<unsigned short*>(MemAlloc(mesh.triangleCount*3*sizeof(unsigned short)));
    const float step=size_/cells,origin_x=chunk_x_*size_,origin_z=chunk_z_*size_;
    // The land at every grid point, with one ring beyond the chunk's edge (for the slopes there,
    // so the shading runs on unbroken into the next chunk). Each point sampled once.
    const int ring=side+2;
    std::vector<float> height(static_cast<std::size_t>(ring*ring));
    for(int z=-1;z<=side;++z) for(int x=-1;x<=side;++x)
        height[static_cast<std::size_t>((z+1)*ring+x+1)]=TerrainHeight::sample(origin_x+x*step,origin_z+z*step);
    // Under La Ringo's paving (its car park and truck park, laid 2 cm proud of the land) the land
    // is drawn a little lower, so it never flickers through the slab from afar. (Only drawn: the
    // ground everything stands and drives on is unchanged.)
    if(TerrainHeight::coastal_layout()) {
        struct Slab { Vector3 centre; float yaw, half_x, half_z; };
        std::vector<Slab> slabs;
        for(const auto& rest:highways::rest_areas()) {
            slabs.push_back({rest.lot,rest.lot_yaw,33,33});
            slabs.push_back({rest.truck_park,rest.truck_park_yaw,41,15});
        }
        for(int z=-1;z<=side;++z) for(int x=-1;x<=side;++x) {
            const float px=origin_x+x*step,pz=origin_z+z*step;
            for(const auto& slab:slabs) {
                const float a=slab.yaw*DEG2RAD,dx=px-slab.centre.x,dz=pz-slab.centre.z;
                const float lx=dx*std::cos(a)-dz*std::sin(a),lz=dx*std::sin(a)+dz*std::cos(a);
                if(std::abs(lx)<slab.half_x && std::abs(lz)<slab.half_z) {height[static_cast<std::size_t>((z+1)*ring+x+1)]-=.12F;break;}
            }
        }
    }
    const auto h=[&](int x,int z){return height[static_cast<std::size_t>((z+1)*ring+x+1)];};
    for(int z=0;z<side;++z) for(int x=0;x<side;++x) {
        const int v=z*side+x;
        mesh.vertices[v*3]=origin_x+x*step;mesh.vertices[v*3+1]=h(x,z);mesh.vertices[v*3+2]=origin_z+z*step;
        // The slope, so hillsides catch the sun or fall into shade.
        const Vector3 normal=Vector3Normalize({h(x-1,z)-h(x+1,z),2*step,h(x,z-1)-h(x,z+1)});
        mesh.normals[v*3]=normal.x;mesh.normals[v*3+1]=normal.y;mesh.normals[v*3+2]=normal.z;
        mesh.texcoords[v*2]=static_cast<float>(x)/cells;mesh.texcoords[v*2+1]=static_cast<float>(z)/cells;
    }
    for(int z=0;z<cells;++z) for(int x=0;x<cells;++x) {
        const int i=(z*cells+x)*6,v=z*side+x;
        mesh.indices[i]=v;mesh.indices[i+1]=v+side;mesh.indices[i+2]=v+1;
        mesh.indices[i+3]=v+1;mesh.indices[i+4]=v+side;mesh.indices[i+5]=v+side+1;
    }
    UploadMesh(&mesh,false);
    model_=LoadModelFromMesh(mesh);
    model_.materials[0].maps[MATERIAL_MAP_DIFFUSE].color={112,151,77,255};
    engine::render::shade_model(model_);
    loaded_=true;
}

void TerrainChunk::draw() const {
    if (!loaded_) {
        return;
    }

    DrawModel(
        model_,
        Vector3{
            0.0F,
            0.0F,
            0.0F
        },
        1.0F,
        WHITE
    );
}

int TerrainChunk::chunk_x() const {
    return chunk_x_;
}

int TerrainChunk::chunk_z() const {
    return chunk_z_;
}

float TerrainChunk::size() const {
    return size_;
}

} // namespace outland::world::terrain
