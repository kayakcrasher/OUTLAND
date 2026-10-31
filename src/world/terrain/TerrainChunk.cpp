#include "outland/world/terrain/TerrainChunk.hpp"

#include "outland/world/terrain/TerrainHeight.hpp"

#include "outland/engine/render/WorldShading.hpp"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>

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
    for(int z=0;z<side;++z) for(int x=0;x<side;++x) {
        const int v=z*side+x;
        const float px=origin_x+x*step,pz=origin_z+z*step;
        mesh.vertices[v*3]=px;mesh.vertices[v*3+1]=TerrainHeight::sample(px,pz);mesh.vertices[v*3+2]=pz;
        // The slope, so hillsides catch the sun or fall into shade.
        const float e=std::max(.5F,step*.5F);
        const float sx=TerrainHeight::sample(px+e,pz)-TerrainHeight::sample(px-e,pz),sz=TerrainHeight::sample(px,pz+e)-TerrainHeight::sample(px,pz-e);
        const Vector3 normal=Vector3Normalize({-sx,2*e,-sz});
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
