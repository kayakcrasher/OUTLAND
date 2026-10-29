#include "outland/engine/render/HighwayRender.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/RegionGeometry.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace outland::engine::highway {
namespace {
namespace hw = world::highways;
constexpr float lift = .1F;              // above the land (other roads ride .06 up, so a highway covers their ends)
constexpr float texture_across = 7.0F;   // road4.png spans the two lanes of a carriageway
constexpr float texture_along = 14.0F;   // its 2:1 image: a dash every 7 m
const Color asphalt_tint{214, 214, 210, 255};
const Color gravel{148, 140, 118, 255};

struct Resources {
    bool tried{false};
    Texture2D lanes{}, plain{};
    Model barrier{};
    bool barrier_loaded{false};
};
std::string resolve(const char* path) {
    const std::string packaged = std::string(GetApplicationDirectory()) + path;
    return FileExists(packaged.c_str()) ? packaged : std::string(path);
}
Texture2D texture(const char* path) {
    Texture2D t = LoadTexture(resolve(path).c_str());
    if (t.id == 0) return t;
    GenTextureMipmaps(&t);
    SetTextureFilter(t, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    return t;
}
Resources& resources() {
    static Resources r;
    if (r.tried) return r;
    r.tried = true;
    r.lanes = texture("assets/verda/urban/textures/road4.png");
    r.plain = texture("assets/verda/urban/textures/road3.png");
    const std::string barrier = resolve("assets/verda/urban/Cones & Barriers/Concrete Barriers/Jersey_barrier.glb");
    if (FileExists(barrier.c_str())) {
        r.barrier = LoadModel(barrier.c_str());
        r.barrier_loaded = r.barrier.meshCount > 0;
    }
    return r;
}

struct Frame {
    Vector3 centre, cross;   // cross: to the right, scaled for the mitre
    Vector3 heading;         // unit, along the road
};

// The roadway's height at (x, z) for a road heading `heading`: the ring's bridge deck where it
// crosses over a spoke, the graded land everywhere else.
float roadway(float x, float z, Vector3 heading) {
    float y = 0;
    if (hw::deck(x, z, y, &heading)) return y;
    return world::terrain::TerrainHeight::sample(x, z);
}

// One strip of the cross-section (offsets in metres, + to the right) between two frames.
void strip(const Frame& a, const Frame& b, float sa, float sb, float o0, float o1, float v0, float v1, float above) {
    const auto at = [&](const Frame& f, float o) {
        const float x = f.centre.x + f.cross.x * o, z = f.centre.z + f.cross.z * o;
        return Vector3{x, roadway(x, z, f.heading) + above, z};
    };
    const Vector3 p00 = at(a, o0), p01 = at(a, o1), p10 = at(b, o0), p11 = at(b, o1);
    const float u0 = sa / texture_along, u1 = sb / texture_along;
    // Counter-clockwise seen from above (the cross vector points to the road's right).
    rlTexCoord2f(u0, v0); rlVertex3f(p00.x, p00.y, p00.z);
    rlTexCoord2f(u1, v1); rlVertex3f(p11.x, p11.y, p11.z);
    rlTexCoord2f(u1, v0); rlVertex3f(p10.x, p10.y, p10.z);
    rlTexCoord2f(u0, v0); rlVertex3f(p00.x, p00.y, p00.z);
    rlTexCoord2f(u0, v1); rlVertex3f(p01.x, p01.y, p01.z);
    rlTexCoord2f(u1, v1); rlVertex3f(p11.x, p11.y, p11.z);
}

bool inside(const std::vector<hw::Span>& spans, float s) {
    return std::any_of(spans.begin(), spans.end(), [&](const hw::Span& span) { return s >= span.from && s <= span.to; });
}

// A box with its bottom at y0 and top at y1, `length` along `along` and `width` across it (both
// horizontal units), lit from above-ish.
void box(Vector3 centre, Vector3 along, float length, float width, float y0, float y1, Color colour) {
    const Vector3 side{-along.z, 0, along.x};
    const auto corner = [&](float a, float s, float y) {
        return Vector3{centre.x + along.x * a * length * .5F + side.x * s * width * .5F, y, centre.z + along.z * a * length * .5F + side.z * s * width * .5F};
    };
    const auto face = [&](Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float light) {
        const auto c = [&](unsigned char v) { return static_cast<unsigned char>(std::clamp(v * light, 0.0F, 255.0F)); };
        rlColor4ub(c(colour.r), c(colour.g), c(colour.b), colour.a);
        rlVertex3f(p0.x, p0.y, p0.z); rlVertex3f(p1.x, p1.y, p1.z); rlVertex3f(p2.x, p2.y, p2.z);
        rlVertex3f(p0.x, p0.y, p0.z); rlVertex3f(p2.x, p2.y, p2.z); rlVertex3f(p3.x, p3.y, p3.z);
    };
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    // (Both windings, so every face shows whichever way round it is seen.)
    for (const bool flip : {false, true}) {
        const auto f = [&](Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float light) { if (flip) face(p3, p2, p1, p0, light); else face(p0, p1, p2, p3, light); };
        f(corner(-1, -1, y1), corner(-1, 1, y1), corner(1, 1, y1), corner(1, -1, y1), 1.0F);      // top
        f(corner(-1, -1, y0), corner(1, -1, y0), corner(1, 1, y0), corner(-1, 1, y0), .55F);      // underside
        f(corner(-1, -1, y0), corner(-1, -1, y1), corner(1, -1, y1), corner(1, -1, y0), .8F);
        f(corner(-1, 1, y0), corner(1, 1, y0), corner(1, 1, y1), corner(-1, 1, y1), .8F);
        f(corner(-1, -1, y0), corner(-1, 1, y0), corner(-1, 1, y1), corner(-1, -1, y1), .7F);
        f(corner(1, -1, y0), corner(1, -1, y1), corner(1, 1, y1), corner(1, 1, y0), .7F);
    }
    rlEnd();
}

// The ring's bridge over a spoke: the deck slab under the roadway, a parapet along each edge, the
// abutments the deck rests on (holding back the embankments) and a pier in the spoke's median.
void bridge(const hw::Interchange& x) {
    const Color concrete{186, 182, 172, 255}, dark{150, 146, 138, 255};
    const Vector3 along = x.ring_forward, outward = Vector3Normalize({x.at.x, 0, x.at.z});
    float deck = 0;
    if (!hw::deck(x.at.x, x.at.z, deck, &along)) return;
    const float under = world::terrain::TerrainHeight::sample(x.at.x, x.at.z);
    const float span = 2 * hw::deck_half, width = 2 * (hw::paved_half + hw::verge);
    box(x.at, along, span, width, deck - 1.1F, deck + lift - .02F, concrete);                  // the slab
    for (const float side : {-1.0F, 1.0F}) {
        const Vector3 edge = Vector3Add(x.at, Vector3Scale(outward, side * (hw::paved_half + .45F)));
        box(edge, along, span, .5F, deck, deck + 1.05F, concrete);                             // parapets
        const Vector3 face = Vector3Add(x.at, Vector3Scale(along, side * (hw::grade_flat + .6F)));
        box(face, along, 1.2F, width + 6, under - .5F, deck - 1.0F, dark);                      // abutments
    }
    box(x.at, along, 1.2F, 6, under, deck - 1.1F, dark);                                        // the pier
}

void barrier(const Model& model, Vector3 a, Vector3 b) {
    const Vector3 along = Vector3Normalize(Vector3Subtract(b, a));
    const Vector3 up = Vector3Normalize(Vector3Subtract({0, 1, 0}, Vector3Scale(along, along.y)));
    const Vector3 side = Vector3CrossProduct(along, up);
    const Vector3 origin = Vector3Lerp(a, b, .5F);
    const Vector3 sun = Vector3Normalize({.35F, .85F, .4F});
    for (int m = 0; m < model.meshCount; ++m) {
        const Mesh& mesh = model.meshes[m];
        if (!mesh.vertices) continue;
        const int material = model.meshMaterial ? model.meshMaterial[m] : 0;
        const unsigned int texture = material < model.materialCount && model.materials[material].maps
            ? model.materials[material].maps[MATERIAL_MAP_ALBEDO].texture.id : 0;
        rlSetTexture(texture);
        rlBegin(RL_TRIANGLES);
        const int count = mesh.indices ? mesh.triangleCount * 3 : mesh.vertexCount;
        for (int k = 0; k < count; ++k) {
            const int v = mesh.indices ? mesh.indices[k] : k;
            const float* p = mesh.vertices + v * 3;
            if (mesh.normals) {
                const float* n = mesh.normals + v * 3;
                const Vector3 world_normal = Vector3Add(Vector3Add(Vector3Scale(along, n[0]), Vector3Scale(up, n[1])), Vector3Scale(side, n[2]));
                const float light = .62F + .38F * std::max(0.0F, Vector3DotProduct(world_normal, sun));
                const auto shade = static_cast<unsigned char>(std::clamp(light * 255, 0.0F, 255.0F));
                rlColor4ub(shade, shade, shade, 255);
            } else {
                rlColor4ub(235, 235, 235, 255);
            }
            if (mesh.texcoords) rlTexCoord2f(mesh.texcoords[v * 2], mesh.texcoords[v * 2 + 1]);
            const Vector3 q = Vector3Add(origin, Vector3Add(Vector3Add(Vector3Scale(along, p[0]), Vector3Scale(up, p[1])), Vector3Scale(side, p[2])));
            rlVertex3f(q.x, q.y, q.z);
        }
        rlEnd();
    }
    rlSetTexture(0);
}
}

void draw(const world::Road& road, const hw::RoadInfo& info, int begin, int end) {
    const int segments = world::geometry::road_segments(road);
    if (segments == 0) return;
    begin = std::max(0, begin); end = std::min(segments, end);
    if (begin >= end) return;
    const float length = std::hypot(road.end.x - road.start.x, road.end.z - road.start.z);
    const Vector3 direction{(road.end.x - road.start.x) / length, 0, (road.end.z - road.start.z) / length};
    const auto frame = [&](int i) {
        const float s = length * static_cast<float>(i) / static_cast<float>(segments);
        // Narrower where it tapers into a town: the whole cross-section scales with the paved width.
        const float t = s / length, half = info.start_half > 0 ? info.start_half + (info.end_half - info.start_half) * t : hw::paved_half;
        return Frame{{road.start.x + direction.x * s, 0, road.start.z + direction.z * s}, Vector3Scale(hw::cross_at(info, direction, s, length), half / hw::paved_half), direction};
    };
    const auto distance = [&](int i) { return length * static_cast<float>(i) / static_cast<float>(segments); };
    auto& r = resources();
    const bool boulevard = road.width < hw::min_width;   // through a city (narrower, planted median)
    // Lanes (textured, or plain on a junction apron), then the plain asphalt, then the verges.
    struct Band { float o0, o1; };
    constexpr Band lanes[2]{{-hw::lanes_outer, -hw::median_half}, {hw::median_half, hw::lanes_outer}};
    constexpr Band paved[4]{{-hw::paved_half, -hw::lanes_outer}, {-hw::median_half, 0}, {0, hw::median_half}, {hw::lanes_outer, hw::paved_half}};
    constexpr Band verges[2]{{-hw::paved_half - hw::verge, -hw::paved_half}, {hw::paved_half, hw::paved_half + hw::verge}};
    for (int pass = 0; pass < 3; ++pass) {
        const bool textured_pass = pass == 0;
        rlSetTexture(pass == 2 ? 0 : textured_pass ? r.lanes.id : r.plain.id);
        rlBegin(RL_TRIANGLES);
        const Color tint = pass == 2 ? gravel : asphalt_tint;
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlNormal3f(0, 1, 0);
        Frame a = frame(begin);
        for (int i = begin; i < end; ++i) {
            const Frame b = frame(i + 1);
            const float sa = distance(i), sb = distance(i + 1);
            const bool apron = inside(info.plain, (sa + sb) * .5F);
            if (pass == 0) {
                if (!apron) for (const auto& band : lanes) strip(a, b, sa, sb, band.o0, band.o1, 0, 1, lift);
            } else if (pass == 1) {
                for (const auto& band : paved) strip(a, b, sa, sb, band.o0, band.o1, band.o0 / texture_across, band.o1 / texture_across, lift);
                if (apron) for (const auto& band : lanes) strip(a, b, sa, sb, band.o0, band.o1, band.o0 / texture_across, band.o1 / texture_across, lift);
            } else if (!boulevard) {   // (a city boulevard has pavements, not gravel)
                for (const auto& band : verges) strip(a, b, sa, sb, band.o0, band.o1, 0, 1, lift - .03F);
            }
            a = b;
        }
        rlEnd();
    }
    rlSetTexture(0);
    // The ring's bridges, drawn with the piece of ring that passes over each.
    for (const auto& x : hw::interchanges()) {
        const float along = (x.at.x - road.start.x) * direction.x + (x.at.z - road.start.z) * direction.z;
        const float aside = std::abs((x.at.x - road.start.x) * -direction.z + (x.at.z - road.start.z) * direction.x);
        if (aside < 1 && along >= distance(begin) && along < distance(end) && std::abs(direction.x * x.ring_forward.x + direction.z * x.ring_forward.z) > .9F) bridge(x);
    }
    if (!r.barrier_loaded) return;
    // The median barrier: whole barriers spread evenly over each run, drawn by the chunk holding
    // their middle, each laid on the road's grade.
    const float from = distance(begin), to = distance(end);
    for (const auto& span : info.barrier) {
        const int count = static_cast<int>(std::floor((span.to - span.from) / hw::barrier_length));
        if (count < 1) continue;
        const float spacing = (span.to - span.from) / static_cast<float>(count);
        for (int k = 0; k < count; ++k) {
            const float middle = span.from + spacing * (static_cast<float>(k) + .5F);
            if (middle < from || middle >= to) continue;
            const auto point = [&](float s) {
                const float x = road.start.x + direction.x * s, z = road.start.z + direction.z * s;
                return Vector3{x, roadway(x, z, direction) + lift - .02F, z};
            };
            const float half = hw::barrier_length * .5F;
            const Vector3 mid = point(middle);
            if (boulevard || world::layout::in_downtown(mid.x, mid.z)) {
                // Through Verda City the median is a planted boulevard: a stone kerb, a lawn, and
                // low clipped hedges (the barrier's collision stays the same).
                const bool along_z = std::abs(direction.z) >= std::abs(direction.x);
                const float run = spacing + .02F, w = boulevard ? road.width / hw::width : 1.0F;   // (a boulevard's median is narrower)
                const Vector3 size = along_z ? Vector3{1.9F * w, .24F, run} : Vector3{run, .24F, 1.9F * w};
                DrawCubeV({mid.x, mid.y + .12F, mid.z}, size, Color{176, 172, 162, 255});
                DrawCubeV({mid.x, mid.y + .25F, mid.z}, along_z ? Vector3{1.6F * w, .03F, run} : Vector3{run, .03F, 1.6F * w}, Color{78, 128, 62, 255});
                if (k % 3 == 1)
                    DrawCubeV({mid.x, mid.y + .55F, mid.z}, along_z ? Vector3{1.1F * w, .6F, spacing * .7F} : Vector3{spacing * .7F, .6F, 1.1F * w}, Color{46, 96, 44, 255});
                else if (k % 3 == 2)
                    for (const float f : {-.3F, .3F})
                        DrawCubeV({mid.x + (along_z ? f : 0), mid.y + .32F, mid.z + (along_z ? 0 : f)}, {.35F, .12F, .35F}, f < 0 ? Color{214, 72, 70, 255} : Color{236, 196, 60, 255});
                continue;
            }
            barrier(r.barrier, point(middle - half), point(middle + half));
        }
    }
}

}
