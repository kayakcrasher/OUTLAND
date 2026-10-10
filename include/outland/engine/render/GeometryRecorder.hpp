#pragma once
#include <raylib.h>
#include <rlgl.h>
#include <functional>
#include <vector>

namespace outland::engine {

// Geometry recorded once and drawn as static meshes: one draw call per texture instead of
// rebuilding thousands of vertices on the CPU every frame (which on phones, and on GPU bridges such
// as Termux's virgl, costs more than drawing them).
class RecordedGeometry {
public:
    struct Piece { Mesh mesh{}; unsigned int texture{0}; };
    RecordedGeometry() = default;
    RecordedGeometry(const RecordedGeometry&) = delete;
    RecordedGeometry& operator=(const RecordedGeometry&) = delete;
    RecordedGeometry(RecordedGeometry&& other) noexcept : pieces_(std::move(other.pieces_)) { other.pieces_.clear(); }
    RecordedGeometry& operator=(RecordedGeometry&& other) noexcept;
    ~RecordedGeometry() { unload(); }
    void draw() const;
    void unload();
    bool empty() const { return pieces_.empty(); }
    int triangles() const;
    std::vector<Piece>& pieces() { return pieces_; }
private:
    std::vector<Piece> pieces_;
};

// Records raylib's immediate-mode drawing (DrawCube, DrawSphere, rlBegin...) into RecordedGeometry.
// Must be called inside BeginMode3D (the recorded geometry is also drawn once, as usual). Vertices
// are kept in world space, so transforms pushed while drawing are baked in. Line drawing is dropped.
class GeometryRecorder {
public:
    static constexpr int capacity = 65536; // vertices per recording; more is reported as overflow
    GeometryRecorder() = default;
    GeometryRecorder(const GeometryRecorder&) = delete;
    GeometryRecorder& operator=(const GeometryRecorder&) = delete;
    ~GeometryRecorder();
    // False when the drawing did not fit one recording (the caller should draw it directly).
    bool record(const std::function<void()>& draw, RecordedGeometry& out);
private:
    rlRenderBatch batch_{};
    bool loaded_{false};
};
}
