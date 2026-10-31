#include "outland/engine/render/GeometryRecorder.hpp"
#include "outland/engine/render/WorldShading.hpp"
#include <raymath.h>
#include <cstring>
#include <map>

namespace outland::engine {

RecordedGeometry& RecordedGeometry::operator=(RecordedGeometry&& other) noexcept {
    if (this != &other) {unload(); pieces_ = std::move(other.pieces_); other.pieces_.clear();}
    return *this;
}

void RecordedGeometry::unload() {
    for (auto& piece : pieces_) UnloadMesh(piece.mesh);
    pieces_.clear();
}

int RecordedGeometry::triangles() const {
    int n = 0;
    for (const auto& piece : pieces_) n += piece.mesh.triangleCount;
    return n;
}

void RecordedGeometry::draw() const {
    static Material material = LoadMaterialDefault();
    static const unsigned int white = rlGetTextureIdDefault();
    render::shade_flat(material);   // recorded shapes have no normals: hazed, not lit
    for (const auto& piece : pieces_) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture.id = piece.texture ? piece.texture : white;
        DrawMesh(piece.mesh, material, MatrixIdentity());
    }
    material.maps[MATERIAL_MAP_DIFFUSE].texture.id = white;
}

GeometryRecorder::~GeometryRecorder() {
    if (loaded_) rlUnloadRenderBatch(batch_);
}

bool GeometryRecorder::record(const std::function<void()>& draw, RecordedGeometry& out) {
    out.unload();
    if (!loaded_) {batch_ = rlLoadRenderBatch(1, capacity / 4); loaded_ = true;}
    rlSetRenderBatchActive(&batch_);
    draw();
    // Read the batch back: each draw call holds vertices in order, padded to an alignment.
    struct Data { std::vector<float> positions, uvs; std::vector<unsigned char> colors; };
    std::map<unsigned int, Data> by_texture;
    const auto& buffer = batch_.vertexBuffer[0];
    const unsigned int white = rlGetTextureIdDefault();
    int offset = 0, total = 0;
    for (int d = 0; d < batch_.drawCounter; ++d) {
        const auto& call = batch_.draws[d];
        const auto add = [&](int v) {
            auto& data = by_texture[call.textureId == white ? 0 : call.textureId];
            data.positions.insert(data.positions.end(), buffer.vertices + v * 3, buffer.vertices + v * 3 + 3);
            data.uvs.insert(data.uvs.end(), buffer.texcoords + v * 2, buffer.texcoords + v * 2 + 2);
            data.colors.insert(data.colors.end(), buffer.colors + v * 4, buffer.colors + v * 4 + 4);
        };
        if (call.mode == RL_TRIANGLES) for (int v = 0; v + 2 < call.vertexCount; v += 3) {add(offset + v); add(offset + v + 1); add(offset + v + 2);}
        else if (call.mode == RL_QUADS) for (int v = 0; v + 3 < call.vertexCount; v += 4) {
            for (const int k : {0, 1, 2, 0, 2, 3}) add(offset + v + k);
        }
        total += call.vertexCount;
        offset += call.vertexCount + call.vertexAlignment;
    }
    // A drawing that filled the batch or its draw-call list was flushed part way: incomplete.
    const bool complete = total < capacity - 64 && batch_.drawCounter < RL_DEFAULT_BATCH_DRAWCALLS - 2;
    rlSetRenderBatchActive(nullptr); // draws this frame's copy and returns to raylib's batch
    if (!complete) return false;
    for (auto& [texture, data] : by_texture) {
        if (data.positions.empty()) continue;
        RecordedGeometry::Piece piece;
        piece.texture = texture;
        auto& mesh = piece.mesh;
        mesh.vertexCount = static_cast<int>(data.positions.size() / 3);
        mesh.triangleCount = mesh.vertexCount / 3;
        mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(data.positions.size() * sizeof(float))));
        mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(data.uvs.size() * sizeof(float))));
        mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(data.colors.size())));
        std::memcpy(mesh.vertices, data.positions.data(), data.positions.size() * sizeof(float));
        std::memcpy(mesh.texcoords, data.uvs.data(), data.uvs.size() * sizeof(float));
        std::memcpy(mesh.colors, data.colors.data(), data.colors.size());
        UploadMesh(&mesh, false);
        out.pieces().push_back(piece);
    }
    return true;
}
}
