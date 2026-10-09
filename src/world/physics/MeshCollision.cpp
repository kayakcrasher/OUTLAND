#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <raymath.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>

namespace outland::world::physics {
namespace {
// ---------------------------------------------------------------- minimal JSON (glTF subset)
struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object } type{Type::Null};
    double number{0};
    std::string text;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> fields;
    const Json* get(const char* key) const {
        for (const auto& [name, value] : fields) if (name == key) return &value;
        return nullptr;
    }
    const Json* at(std::size_t index) const { return index < items.size() ? &items[index] : nullptr; }
    double num(const char* key, double fallback) const {
        const auto* value = get(key);
        return value && value->type == Type::Number ? value->number : fallback;
    }
    long integer(const char* key, long fallback) const { return static_cast<long>(num(key, static_cast<double>(fallback))); }
    std::string str(const char* key) const {
        const auto* value = get(key);
        return value && value->type == Type::String ? value->text : std::string{};
    }
};
class JsonParser {
public:
    JsonParser(const char* begin, const char* end) : p_(begin), end_(end) {}
    bool parse(Json& out) { return value(out, 0) && (skip(), true); }
private:
    void skip() { while (p_ < end_ && std::isspace(static_cast<unsigned char>(*p_))) ++p_; }
    bool literal(const char* word) {
        const auto n = std::strlen(word);
        if (static_cast<std::size_t>(end_ - p_) < n || std::memcmp(p_, word, n) != 0) return false;
        p_ += n; return true;
    }
    bool string(std::string& out) {
        if (p_ >= end_ || *p_ != '"') return false;
        ++p_;
        while (p_ < end_ && *p_ != '"') {
            if (*p_ == '\\') {
                if (++p_ >= end_) return false;
                switch (*p_) {
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                    case 'u': if (end_ - p_ < 5) return false; p_ += 4; out += '?'; break;
                    default: out += *p_; break;
                }
                ++p_;
            } else out += *p_++;
        }
        if (p_ >= end_) return false;
        ++p_; return true;
    }
    bool value(Json& out, int depth) {
        if (depth > 64) return false;
        skip();
        if (p_ >= end_) return false;
        if (*p_ == '{') {
            out.type = Json::Type::Object; ++p_; skip();
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }
            while (true) {
                skip(); std::string key;
                if (!string(key)) return false;
                skip(); if (p_ >= end_ || *p_ != ':') return false; ++p_;
                Json child; if (!value(child, depth + 1)) return false;
                out.fields.emplace_back(std::move(key), std::move(child));
                skip(); if (p_ >= end_) return false;
                if (*p_ == ',') { ++p_; continue; }
                if (*p_ == '}') { ++p_; return true; }
                return false;
            }
        }
        if (*p_ == '[') {
            out.type = Json::Type::Array; ++p_; skip();
            if (p_ < end_ && *p_ == ']') { ++p_; return true; }
            while (true) {
                Json child; if (!value(child, depth + 1)) return false;
                out.items.push_back(std::move(child));
                skip(); if (p_ >= end_) return false;
                if (*p_ == ',') { ++p_; continue; }
                if (*p_ == ']') { ++p_; return true; }
                return false;
            }
        }
        if (*p_ == '"') { out.type = Json::Type::String; return string(out.text); }
        if (literal("true")) { out.type = Json::Type::Bool; out.number = 1; return true; }
        if (literal("false")) { out.type = Json::Type::Bool; return true; }
        if (literal("null")) return true;
        char* after = nullptr;
        const std::string number(p_, static_cast<std::size_t>(std::min<std::ptrdiff_t>(end_ - p_, 40)));
        out.number = std::strtod(number.c_str(), &after);
        if (after == number.c_str()) return false;
        out.type = Json::Type::Number; p_ += after - number.c_str();
        return true;
    }
    const char* p_;
    const char* end_;
};

// ---------------------------------------------------------------- glTF helpers
bool read_file(const std::string& path, std::vector<unsigned char>& bytes) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    bytes.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}
std::string url_decode(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) { out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16)); i += 2; }
        else out += text[i];
    }
    return out;
}
bool base64(const std::string& text, std::vector<unsigned char>& out) {
    auto decode = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        return c == '/' ? 63 : -1;
    };
    int bits = 0, value = 0;
    for (char c : text) {
        if (c == '=') break;
        const int d = decode(c); if (d < 0) continue;
        value = (value << 6) | d; bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<unsigned char>((value >> bits) & 0xFF)); }
    }
    return true;
}
Matrix matrix_from(const Json& node) {
    if (const auto* m = node.get("matrix"); m && m->items.size() == 16) {
        float a[16]; for (int i = 0; i < 16; ++i) a[i] = static_cast<float>(m->items[static_cast<std::size_t>(i)].number);
        // glTF is column-major; raylib's named fields mN hold element N of that array (as LoadGLTF does).
        return {a[0], a[4], a[8], a[12], a[1], a[5], a[9], a[13], a[2], a[6], a[10], a[14], a[3], a[7], a[11], a[15]};
    }
    Vector3 t{0, 0, 0}, s{1, 1, 1}; Quaternion r{0, 0, 0, 1};
    if (const auto* v = node.get("translation"); v && v->items.size() == 3) t = {static_cast<float>(v->items[0].number), static_cast<float>(v->items[1].number), static_cast<float>(v->items[2].number)};
    if (const auto* v = node.get("scale"); v && v->items.size() == 3) s = {static_cast<float>(v->items[0].number), static_cast<float>(v->items[1].number), static_cast<float>(v->items[2].number)};
    if (const auto* v = node.get("rotation"); v && v->items.size() == 4) r = {static_cast<float>(v->items[0].number), static_cast<float>(v->items[1].number), static_cast<float>(v->items[2].number), static_cast<float>(v->items[3].number)};
    return MatrixMultiply(MatrixMultiply(MatrixScale(s.x, s.y, s.z), QuaternionToMatrix(r)), MatrixTranslate(t.x, t.y, t.z));
}
std::string lower(std::string text) { for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return text; }

Vector3 closest_on_triangle(Vector3 p, Vector3 a, Vector3 b, Vector3 c) {
    // Ericson, Real-Time Collision Detection 5.1.5.
    const auto ab = Vector3Subtract(b, a), ac = Vector3Subtract(c, a), ap = Vector3Subtract(p, a);
    const float d1 = Vector3DotProduct(ab, ap), d2 = Vector3DotProduct(ac, ap);
    if (d1 <= 0 && d2 <= 0) return a;
    const auto bp = Vector3Subtract(p, b);
    const float d3 = Vector3DotProduct(ab, bp), d4 = Vector3DotProduct(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) return Vector3Add(a, Vector3Scale(ab, d1 / (d1 - d3)));
    const auto cp = Vector3Subtract(p, c);
    const float d5 = Vector3DotProduct(ab, cp), d6 = Vector3DotProduct(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) return Vector3Add(a, Vector3Scale(ac, d2 / (d2 - d6)));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0)
        return Vector3Add(b, Vector3Scale(Vector3Subtract(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    const float denom = 1 / (va + vb + vc);
    return Vector3Add(a, Vector3Add(Vector3Scale(ab, vb * denom), Vector3Scale(ac, vc * denom)));
}

struct Library {
    std::vector<std::string> roots;
    std::unordered_map<std::string, std::unique_ptr<CollisionMesh>> meshes;
    std::unordered_map<std::string, bool> failed;
};
Library& library() { static Library value; return value; }
}

bool load_collision_mesh(const std::string& path, CollisionMesh& mesh, std::string& error) {
    std::vector<unsigned char> file;
    if (!read_file(path, file)) { error = "cannot read " + path; return false; }
    std::vector<unsigned char> glb_bin;
    const char* json_begin = reinterpret_cast<const char*>(file.data());
    const char* json_end = json_begin + file.size();
    if (file.size() >= 12 && std::memcmp(file.data(), "glTF", 4) == 0) {
        std::size_t offset = 12;
        json_end = json_begin;
        while (offset + 8 <= file.size()) {
            std::uint32_t length = 0, type = 0;
            std::memcpy(&length, &file[offset], 4); std::memcpy(&type, &file[offset + 4], 4);
            if (offset + 8 + length > file.size()) { error = "truncated GLB"; return false; }
            if (type == 0x4E4F534A) { json_begin = reinterpret_cast<const char*>(&file[offset + 8]); json_end = json_begin + length; }
            else if (type == 0x004E4942) glb_bin.assign(file.begin() + static_cast<long>(offset + 8), file.begin() + static_cast<long>(offset + 8 + length));
            offset += 8 + length;
        }
    }
    Json root;
    if (!JsonParser(json_begin, json_end).parse(root) || root.type != Json::Type::Object) { error = "invalid glTF JSON"; return false; }
    const auto directory = std::filesystem::path(path).parent_path();
    std::vector<std::vector<unsigned char>> buffers;
    if (const auto* list = root.get("buffers")) for (const auto& buffer : list->items) {
        std::vector<unsigned char> data;
        const auto uri = buffer.str("uri");
        if (uri.empty()) data = glb_bin;
        else if (uri.rfind("data:", 0) == 0) { const auto comma = uri.find(','); if (comma != std::string::npos) base64(uri.substr(comma + 1), data); }
        else if (!read_file((directory / url_decode(uri)).string(), data)) { error = "missing buffer " + uri; return false; }
        buffers.push_back(std::move(data));
    }
    const auto* views = root.get("bufferViews");
    const auto* accessors = root.get("accessors");
    // Returns a pointer to element 0, the stride and the count, after bounds checks.
    const auto locate = [&](long index, int components, int component_size, const unsigned char*& data, std::size_t& stride, std::size_t& count) {
        const auto* accessor = accessors ? accessors->at(static_cast<std::size_t>(index)) : nullptr;
        if (!accessor || accessor->get("sparse")) return false;
        const auto* view = views ? views->at(static_cast<std::size_t>(accessor->integer("bufferView", -1))) : nullptr;
        if (!view) return false;
        const auto buffer_index = static_cast<std::size_t>(view->integer("buffer", -1));
        if (buffer_index >= buffers.size()) return false;
        const auto& buffer = buffers[buffer_index];
        count = static_cast<std::size_t>(accessor->integer("count", 0));
        stride = static_cast<std::size_t>(view->integer("byteStride", 0));
        if (stride == 0) stride = static_cast<std::size_t>(components * component_size);
        const auto start = static_cast<std::size_t>(view->integer("byteOffset", 0) + accessor->integer("byteOffset", 0));
        if (count == 0 || start + (count - 1) * stride + static_cast<std::size_t>(components * component_size) > buffer.size()) return false;
        data = buffer.data() + start;
        return true;
    };
    const auto* nodes = root.get("nodes");
    const auto* meshes = root.get("meshes");
    const auto* materials = root.get("materials");
    if (!nodes || !meshes) { error = "no meshes"; return false; }
    // World transforms through the parent chain (raylib applies them to every node, scenes ignored).
    std::vector<long> parent(nodes->items.size(), -1);
    for (std::size_t i = 0; i < nodes->items.size(); ++i)
        if (const auto* children = nodes->items[i].get("children"))
            for (const auto& child : children->items)
                if (child.number >= 0 && static_cast<std::size_t>(child.number) < parent.size()) parent[static_cast<std::size_t>(child.number)] = static_cast<long>(i);
    const auto world = [&](std::size_t index) {
        Matrix result = matrix_from(nodes->items[index]);
        long up = parent[index];
        for (int guard = 0; up >= 0 && guard < 256; ++guard) {
            result = MatrixMultiply(result, matrix_from(nodes->items[static_cast<std::size_t>(up)]));
            up = parent[static_cast<std::size_t>(up)];
        }
        return result;
    };
    struct Raw { Vector3 a, b, c; int material; Vector2 uv; std::size_t primitive, index; };
    std::vector<Raw> raw;
    std::vector<int> primitive_material;
    Vector3 lo{1e30F, 1e30F, 1e30F}, hi{-1e30F, -1e30F, -1e30F};
    for (std::size_t n = 0; n < nodes->items.size(); ++n) {
        const long mesh_index = nodes->items[n].integer("mesh", -1);
        const auto* source = mesh_index >= 0 ? meshes->at(static_cast<std::size_t>(mesh_index)) : nullptr;
        const auto* primitives = source ? source->get("primitives") : nullptr;
        if (!primitives) continue;
        const Matrix transform = world(n);
        for (const auto& primitive : primitives->items) {
            if (primitive.integer("mode", 4) != 4) continue; // raylib only loads triangle primitives
            const int material = static_cast<int>(primitive.integer("material", -1));
            primitive_material.push_back(material);
            const auto* attributes = primitive.get("attributes");
            const long position_index = attributes ? attributes->integer("POSITION", -1) : -1;
            const auto* position_accessor = accessors ? accessors->at(static_cast<std::size_t>(position_index)) : nullptr;
            if (!position_accessor || position_accessor->integer("componentType", 0) != 5126) continue;
            const unsigned char* data = nullptr; std::size_t stride = 0, count = 0;
            if (!locate(position_index, 3, 4, data, stride, count)) continue;
            std::vector<Vector2> uvs;
            if (const long uv_index = attributes->integer("TEXCOORD_0", -1); uv_index >= 0 &&
                accessors->at(static_cast<std::size_t>(uv_index)) && accessors->at(static_cast<std::size_t>(uv_index))->integer("componentType", 0) == 5126) {
                const unsigned char* uv_data = nullptr; std::size_t uv_stride = 0, uv_count = 0;
                if (locate(uv_index, 2, 4, uv_data, uv_stride, uv_count) && uv_count == count) {
                    uvs.resize(count);
                    for (std::size_t i = 0; i < count; ++i) std::memcpy(&uvs[i], uv_data + i * uv_stride, sizeof(Vector2));
                }
            }
            std::vector<Vector3> points(count);
            for (std::size_t i = 0; i < count; ++i) {
                float v[3]; std::memcpy(v, data + i * stride, sizeof v);
                points[i] = Vector3Transform({v[0], v[1], v[2]}, transform);
                lo = Vector3Min(lo, points[i]); hi = Vector3Max(hi, points[i]);
            }
            std::vector<std::uint32_t> indices;
            if (const long index_accessor = primitive.integer("indices", -1); index_accessor >= 0) {
                const auto type = accessors->at(static_cast<std::size_t>(index_accessor))->integer("componentType", 0);
                const int size = type == 5121 ? 1 : type == 5123 ? 2 : type == 5125 ? 4 : 0;
                const unsigned char* index_data = nullptr; std::size_t index_stride = 0, index_count = 0;
                if (!size || !locate(index_accessor, 1, size, index_data, index_stride, index_count)) continue;
                indices.resize(index_count);
                for (std::size_t i = 0; i < index_count; ++i) {
                    std::uint32_t value = 0; std::memcpy(&value, index_data + i * index_stride, static_cast<std::size_t>(size));
                    indices[i] = value;
                }
            } else for (std::uint32_t i = 0; i < count; ++i) indices.push_back(i);
            for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
                if (indices[i] >= count || indices[i + 1] >= count || indices[i + 2] >= count) continue;
                Vector2 uv{-1, -1};
                if (!uvs.empty()) uv = {(uvs[indices[i]].x + uvs[indices[i + 1]].x + uvs[indices[i + 2]].x) / 3,
                    (uvs[indices[i]].y + uvs[indices[i + 1]].y + uvs[indices[i + 2]].y) / 3};
                raw.push_back({points[indices[i]], points[indices[i + 1]], points[indices[i + 2]], material, uv, primitive_material.size() - 1, i / 3});
            }
        }
    }
    if (raw.empty()) { error = "no triangles"; return false; }
    // Same ground-centring as ModelCache (bounds of every loaded vertex).
    const Vector3 offset{-(lo.x + hi.x) * .5F, -lo.y, -(lo.z + hi.z) * .5F};
    // Wall pieces (one storey) lose their glass. Tall prebuilt facades are hollow shells with no
    // floors inside, so they keep their glass and painted interiors and act as solid cover.
    const bool building_scale = hi.y - lo.y > 1.5F && hi.y - lo.y <= 4.5F;
    std::vector<bool> knock;
    if (materials) for (const auto& material : materials->items) {
        const auto name = lower(material.str("name"));
        knock.push_back(building_scale && (name.find("glass") != std::string::npos || name.find("fakeinterior") != std::string::npos ||
            name.find("fake_interior") != std::string::npos));
    }
    // The modular "Building Parts" kit paints its windows onto flat panels of the shared Houses
    // atlas. Those panels are knocked out too, leaving a real opening above the sill.
    std::vector<bool> atlas;
    const bool modular_kit = path.find("Building Parts") != std::string::npos;
    if (materials) for (const auto& material : materials->items) atlas.push_back(modular_kit && lower(material.str("name")) == "houses");
    constexpr Rectangle window_tiles[]{{.124F, .124F, .062F, .12F}, {.184F, .124F, .054F, .069F}, {.389F, .124F, .062F, .121F},
        {.675F, 0, .074F, .095F}, {.675F, .092F, .076F, .094F}, {.748F, 0, .04F, .085F}};
    const auto knocked = [&](const Raw& t) {
        if (t.material < 0) return false;
        const auto m = static_cast<std::size_t>(t.material);
        if (m < knock.size() && knock[m]) return true;
        if (m >= atlas.size() || !atlas[m]) return false;
        for (const auto& r : window_tiles)
            if (t.uv.x >= r.x && t.uv.x <= r.x + r.width && t.uv.y >= r.y && t.uv.y <= r.y + r.height) return true;
        return false;
    };
    mesh = {};
    mesh.source_min = lo; mesh.source_max = hi;
    mesh.mesh_knocked_triangles.assign(primitive_material.size(), {});
    std::vector<std::size_t> primitive_triangles(primitive_material.size(), 0);
    mesh.min = {1e30F, 1e30F, 1e30F}; mesh.max = {-1e30F, -1e30F, -1e30F};
    for (const auto& triangle : raw) {
        ++primitive_triangles[triangle.primitive];
        if (knocked(triangle)) {
            ++mesh.knocked_out;
            mesh.mesh_knocked_triangles[triangle.primitive].push_back(static_cast<std::uint32_t>(triangle.index));
            continue;
        }
        MeshTriangle t{Vector3Add(triangle.a, offset), Vector3Add(triangle.b, offset), Vector3Add(triangle.c, offset), {}};
        const auto normal = Vector3CrossProduct(Vector3Subtract(t.b, t.a), Vector3Subtract(t.c, t.a));
        const float length = Vector3Length(normal);
        if (length < 1e-8F) continue;
        t.normal = Vector3Scale(normal, 1 / length);
        for (const auto& p : {t.a, t.b, t.c}) { mesh.min = Vector3Min(mesh.min, p); mesh.max = Vector3Max(mesh.max, p); }
        mesh.triangles.push_back(t);
    }
    for (std::size_t k = 0; k < primitive_material.size(); ++k)
        mesh.mesh_knocked_out.push_back(primitive_triangles[k] > 0 && mesh.mesh_knocked_triangles[k].size() == primitive_triangles[k]);
    if (mesh.triangles.empty()) { error = "only knocked-out triangles"; return false; }
    mesh.build_grid();
    return true;
}

void CollisionMesh::build_grid() {
    const float width = max.x - min.x, depth = max.z - min.z;
    cell_ = std::max(1.5F, std::max(width, depth) / 256);
    nx_ = std::max(1, static_cast<int>(std::ceil(width / cell_)) + 1);
    nz_ = std::max(1, static_cast<int>(std::ceil(depth / cell_)) + 1);
    cells_.assign(static_cast<std::size_t>(nx_ * nz_), {});
    for (std::uint32_t i = 0; i < triangles.size(); ++i) {
        const auto& t = triangles[i];
        const int x0 = std::clamp(static_cast<int>((std::min({t.a.x, t.b.x, t.c.x}) - min.x) / cell_), 0, nx_ - 1);
        const int x1 = std::clamp(static_cast<int>((std::max({t.a.x, t.b.x, t.c.x}) - min.x) / cell_), 0, nx_ - 1);
        const int z0 = std::clamp(static_cast<int>((std::min({t.a.z, t.b.z, t.c.z}) - min.z) / cell_), 0, nz_ - 1);
        const int z1 = std::clamp(static_cast<int>((std::max({t.a.z, t.b.z, t.c.z}) - min.z) / cell_), 0, nz_ - 1);
        for (int z = z0; z <= z1; ++z) for (int x = x0; x <= x1; ++x) cells_[static_cast<std::size_t>(z * nx_ + x)].push_back(i);
    }
    stamp_.assign(triangles.size(), 0);
    query_ = 0;
}

template<class F> void CollisionMesh::visit(float x0, float z0, float x1, float z1, F&& f) const {
    if (cells_.empty() || x1 < min.x || z1 < min.z || x0 > max.x || z0 > max.z) return;
    if (++query_ == 0) { std::fill(stamp_.begin(), stamp_.end(), 0); query_ = 1; }
    const int cx0 = std::clamp(static_cast<int>((x0 - min.x) / cell_), 0, nx_ - 1), cx1 = std::clamp(static_cast<int>((x1 - min.x) / cell_), 0, nx_ - 1);
    const int cz0 = std::clamp(static_cast<int>((z0 - min.z) / cell_), 0, nz_ - 1), cz1 = std::clamp(static_cast<int>((z1 - min.z) / cell_), 0, nz_ - 1);
    for (int z = cz0; z <= cz1; ++z) for (int x = cx0; x <= cx1; ++x)
        for (const auto index : cells_[static_cast<std::size_t>(z * nx_ + x)]) {
            if (stamp_[index] == query_) continue;
            stamp_[index] = query_;
            if (f(triangles[index])) return;
        }
}

bool CollisionMesh::sphere_blocked(Vector3 centre, float radius) const {
    if (centre.y + radius < min.y || centre.y - radius > max.y) return false;
    bool hit = false;
    visit(centre.x - radius, centre.z - radius, centre.x + radius, centre.z + radius, [&](const MeshTriangle& t) {
        if (std::abs(t.normal.y) > .7F) return false; // floors, stair treads and ceilings are not walls
        hit = Vector3DistanceSqr(closest_on_triangle(centre, t.a, t.b, t.c), centre) < radius * radius;
        return hit;
    });
    return hit;
}

bool CollisionMesh::segment(Vector3 start, Vector3 end, float& fraction, Vector3& normal) const {
    const auto lo = Vector3Min(start, end), hi = Vector3Max(start, end);
    if (hi.x < min.x || hi.y < min.y || hi.z < min.z || lo.x > max.x || lo.y > max.y || lo.z > max.z) return false;
    const auto direction = Vector3Subtract(end, start);
    bool hit = false;
    visit(lo.x, lo.z, hi.x, hi.z, [&](const MeshTriangle& t) {
        // Möller–Trumbore, double-sided.
        const auto e1 = Vector3Subtract(t.b, t.a), e2 = Vector3Subtract(t.c, t.a);
        const auto p = Vector3CrossProduct(direction, e2);
        const float det = Vector3DotProduct(e1, p);
        if (std::abs(det) < 1e-9F) return false;
        const float inv = 1 / det;
        const auto s = Vector3Subtract(start, t.a);
        const float u = Vector3DotProduct(s, p) * inv;
        if (u < 0 || u > 1) return false;
        const auto q = Vector3CrossProduct(s, e1);
        const float v = Vector3DotProduct(direction, q) * inv;
        if (v < 0 || u + v > 1) return false;
        const float f = Vector3DotProduct(e2, q) * inv;
        if (f < 0 || f > 1 || f >= fraction) return false;
        fraction = f; hit = true;
        normal = Vector3DotProduct(t.normal, direction) > 0 ? Vector3Negate(t.normal) : t.normal;
        return false;
    });
    return hit;
}

bool CollisionMesh::floor_below(float x, float z, float max_y, float& y) const {
    bool found = false;
    visit(x, z, x, z, [&](const MeshTriangle& t) {
        if (std::abs(t.normal.y) <= .5F) return false;
        const float d = (t.b.z - t.c.z) * (t.a.x - t.c.x) + (t.c.x - t.b.x) * (t.a.z - t.c.z);
        if (std::abs(d) < 1e-9F) return false;
        const float w0 = ((t.b.z - t.c.z) * (x - t.c.x) + (t.c.x - t.b.x) * (z - t.c.z)) / d;
        const float w1 = ((t.c.z - t.a.z) * (x - t.c.x) + (t.a.x - t.c.x) * (z - t.c.z)) / d;
        const float w2 = 1 - w0 - w1;
        if (w0 < -1e-4F || w1 < -1e-4F || w2 < -1e-4F) return false;
        const float height = w0 * t.a.y + w1 * t.b.y + w2 * t.c.y;
        if (height <= max_y && (!found || height > y)) { y = height; found = true; }
        return false;
    });
    return found;
}

const CollisionMesh* MeshCollisionLibrary::get(const std::string& model_path) {
    if (model_path.empty()) return nullptr;
    auto& lib = library();
    if (const auto found = lib.meshes.find(model_path); found != lib.meshes.end()) return found->second.get();
    if (lib.failed.contains(model_path)) return nullptr;
    const auto extension = lower(std::filesystem::path(model_path).extension().string());
    // Character bodies are re-oriented when drawn; they keep simple box collision.
    if ((extension != ".glb" && extension != ".gltf") || model_path.find("assets/verda/characters/") != std::string::npos) {
        lib.failed[model_path] = true; return nullptr;
    }
    std::vector<std::string> roots = lib.roots;
    roots.emplace_back("");
    for (const auto& root : roots) {
        const auto candidate = root.empty() ? model_path : (std::filesystem::path(root) / model_path).string();
        std::error_code ignored;
        if (!std::filesystem::exists(candidate, ignored)) continue;
        auto mesh = std::make_unique<CollisionMesh>();
        std::string error;
        if (!load_collision_mesh(candidate, *mesh, error)) break;
        return lib.meshes.emplace(model_path, std::move(mesh)).first->second.get();
    }
    lib.failed[model_path] = true;
    return nullptr;
}
void MeshCollisionLibrary::add_root(const std::string& directory) {
    auto& roots = library().roots;
    if (std::find(roots.begin(), roots.end(), directory) == roots.end()) roots.insert(roots.begin(), directory);
    library().failed.clear();
}
void MeshCollisionLibrary::preload(const VerdaRegion& region) {
    for (const auto& settlement : region.settlements())
        for (const auto& asset : settlement.assets)
            if (asset.collision && asset.vehicle.definition.empty()) get(asset.model_path);
}
void MeshCollisionLibrary::clear() { library().meshes.clear(); library().failed.clear(); }
}
