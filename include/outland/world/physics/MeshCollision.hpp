#pragma once
#include <raylib.h>
#include <cstdint>
#include <string>
#include <vector>

namespace outland::world { class VerdaRegion; }

namespace outland::world::physics {
struct MeshTriangle { Vector3 a, b, c, normal; };

// CPU-only triangle collision for a model-backed asset, built from the same glTF the renderer
// draws (same node order, transforms and ground-centring as raylib LoadModel + ModelCache).
// Window glass, painted "fake interior" panels on one-storey wall pieces, and the painted window
// panels of the modular Building Parts kit are knocked out:
// they neither collide nor draw, so every window is a real opening for movement, sight and bullets.
class CollisionMesh {
public:
    std::vector<MeshTriangle> triangles; // solid triangles in centred model space
    Vector3 min{}, max{};                 // bounds of all solid triangles
    Vector3 source_min{}, source_max{};   // raw glTF bounds before ground-centring (raylib's model bounds)
    std::vector<bool> mesh_knocked_out;   // per raylib Mesh index (glTF triangle primitive order): all gone
    std::vector<std::vector<std::uint32_t>> mesh_knocked_triangles; // per raylib Mesh: removed triangle indices
    int knocked_out{0};                   // triangles removed as glass/fake interior

    // Body queries ignore walkable floor/stair tops; those are ground, not walls.
    bool sphere_blocked(Vector3 centre, float radius) const;
    bool segment(Vector3 start, Vector3 end, float& fraction, Vector3& normal) const;
    // Highest walkable surface at (x,z) whose height is <= max_y.
    bool floor_below(float x, float z, float max_y, float& y) const;
    void build_grid();
private:
    template<class F> void visit(float x0, float z0, float x1, float z1, F&& f) const;
    float cell_{1.5F};
    int nx_{0}, nz_{0};
    std::vector<std::vector<std::uint32_t>> cells_;
    mutable std::vector<std::uint32_t> stamp_;
    mutable std::uint32_t query_{0};
};

class MeshCollisionLibrary {
public:
    // Cached by model path; nullptr when the file is missing, unsupported (non-glTF) or empty,
    // in which case callers keep the asset's box collision.
    static const CollisionMesh* get(const std::string& model_path);
    static void add_root(const std::string& directory);
    static void preload(const VerdaRegion& region);
    static void clear();
};

// World transform of the first skinned mesh node (what raylib bakes into skinned vertices).
// False if the file has no skinned mesh or cannot be read.
bool skinned_mesh_transform(const std::string& path, Matrix& transform);
// Parses glTF/GLB bytes; exposed for tests. `base_directory` resolves external buffers.
bool load_collision_mesh(const std::string& path, CollisionMesh& mesh, std::string& error);
}
