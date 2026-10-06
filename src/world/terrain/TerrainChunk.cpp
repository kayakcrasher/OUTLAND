#include "outland/world/terrain/TerrainChunk.hpp"

#include "outland/world/terrain/TerrainHeight.hpp"

#include <raylib.h>

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
    if (resolution_ < 1) {
        resolution_ = 1;
    }

    if (size_ <= 0.0F) {
        size_ = 128.0F;
    }

    const int cells =
        resolution_;

    const int triangle_count =
        cells *
        cells *
        2;

    const int vertex_count =
        triangle_count *
        3;

    Mesh mesh{};

    mesh.vertexCount =
        vertex_count;

    mesh.triangleCount =
        triangle_count;

    mesh.vertices =
        static_cast<float*>(
            MemAlloc(
                static_cast<unsigned int>(
                    vertex_count *
                    3 *
                    sizeof(float)
                )
            )
        );

    mesh.normals =
        static_cast<float*>(
            MemAlloc(
                static_cast<unsigned int>(
                    vertex_count *
                    3 *
                    sizeof(float)
                )
            )
        );

    mesh.texcoords =
        static_cast<float*>(
            MemAlloc(
                static_cast<unsigned int>(
                    vertex_count *
                    2 *
                    sizeof(float)
                )
            )
        );

    const float step =
        size_ /
        static_cast<float>(
            cells
        );

    const float origin_x =
        static_cast<float>(
            chunk_x_
        ) *
        size_;

    const float origin_z =
        static_cast<float>(
            chunk_z_
        ) *
        size_;

    int vertex = 0;

    const auto write_vertex =
        [&](
            const float world_x,
            const float world_z,
            const float u,
            const float v
        ) {
            const float world_y =
                TerrainHeight::sample(
                    world_x,
                    world_z
                );

            const int position_index =
                vertex * 3;

            mesh.vertices[
                position_index
            ] = world_x;

            mesh.vertices[
                position_index + 1
            ] = world_y;

            mesh.vertices[
                position_index + 2
            ] = world_z;

            /*
             * Temporary upward-facing normals.
             *
             * Proper slope normals come when we
             * add lighting and terrain materials.
             */
            mesh.normals[
                position_index
            ] = 0.0F;

            mesh.normals[
                position_index + 1
            ] = 1.0F;

            mesh.normals[
                position_index + 2
            ] = 0.0F;

            const int texture_index =
                vertex * 2;

            mesh.texcoords[
                texture_index
            ] = u;

            mesh.texcoords[
                texture_index + 1
            ] = v;

            ++vertex;
        };

    for (
        int z = 0;
        z < cells;
        ++z
    ) {
        for (
            int x = 0;
            x < cells;
            ++x
        ) {
            const float x0 =
                origin_x +
                static_cast<float>(x) *
                step;

            const float x1 =
                x0 + step;

            const float z0 =
                origin_z +
                static_cast<float>(z) *
                step;

            const float z1 =
                z0 + step;

            const float u0 =
                static_cast<float>(x) /
                static_cast<float>(cells);

            const float u1 =
                static_cast<float>(x + 1) /
                static_cast<float>(cells);

            const float v0 =
                static_cast<float>(z) /
                static_cast<float>(cells);

            const float v1 =
                static_cast<float>(z + 1) /
                static_cast<float>(cells);

            /*
             * Triangle A
             *
             * p00 ----- p10
             *  |      /
             *  |    /
             *  |  /
             * p01
             */
            write_vertex(
                x0,
                z0,
                u0,
                v0
            );

            write_vertex(
                x0,
                z1,
                u0,
                v1
            );

            write_vertex(
                x1,
                z0,
                u1,
                v0
            );

            /*
             * Triangle B
             *
             *          p10
             *         / |
             *       /   |
             *     /     |
             * p01 ----- p11
             */
            write_vertex(
                x1,
                z0,
                u1,
                v0
            );

            write_vertex(
                x0,
                z1,
                u0,
                v1
            );

            write_vertex(
                x1,
                z1,
                u1,
                v1
            );
        }
    }

    UploadMesh(
        &mesh,
        false
    );

    model_ =
        LoadModelFromMesh(
            mesh
        );

    /*
     * Temporary Verdan grass material.
     * Later this becomes a proper terrain
     * material system.
     */
    model_
        .materials[0]
        .maps[MATERIAL_MAP_DIFFUSE]
        .color = Color{
            78,
            105,
            61,
            255
        };

    loaded_ = true;
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
