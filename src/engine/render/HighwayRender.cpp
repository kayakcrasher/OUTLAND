#include "outland/engine/render/HighwayRender.hpp"
#include "outland/engine/render/WorldShading.hpp"
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
        if (r.barrier_loaded) engine::render::shade_model(r.barrier);
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
        f(corner(-1, -1, y0), corner(1, -1, y0), corner(1, 1, y0), corner(-1, 1, y0), .72F);      // underside
        f(corner(-1, -1, y0), corner(-1, -1, y1), corner(1, -1, y1), corner(1, -1, y0), .8F);
        f(corner(-1, 1, y0), corner(1, 1, y0), corner(1, 1, y1), corner(-1, 1, y1), .8F);
        f(corner(-1, -1, y0), corner(-1, 1, y0), corner(-1, 1, y1), corner(-1, -1, y1), .7F);
        f(corner(1, -1, y0), corner(1, -1, y1), corner(1, 1, y1), corner(1, 1, y0), .7F);
    }
    rlEnd();
}

// A ring bridge over an interchange's roundabout: the deck slab under the ring's roadway, a parapet
// along each edge, an abutment at each end holding back the embankment, two piers on the island,
// and the island itself (a kerb and a lawn); retaining walls between the climbing ring and the slip
// roads beside it.
void bridge(const hw::Interchange& x) {
    const Color concrete{214, 210, 200, 255}, dark{178, 174, 166, 255}, kerb{206, 202, 194, 255}, lawn{86, 132, 66, 255};
    const Vector3 along = x.ring_forward, outward = Vector3Normalize({x.at.x, 0, x.at.z});
    float deck = 0;
    if (!hw::deck(x.at.x, x.at.z, deck, &along)) return;
    const float under = world::terrain::TerrainHeight::sample(x.at.x, x.at.z);
    const float span = 2 * hw::deck_half, width = 2 * (hw::paved_half + hw::verge);
    // (The ring curves gently over the 100 m: the slab in four pieces.)
    const float radius = std::hypot(x.at.x, x.at.z), bearing = std::atan2(x.at.z, x.at.x);
    const float sense = along.x * -std::sin(bearing) + along.z * std::cos(bearing) > 0 ? 1.0F : -1.0F;
    const auto ring_point = [&](float s, float lateral) {
        const float b = bearing + sense * s / radius, r = radius + lateral;
        return Vector3{r * std::cos(b), 0, r * std::sin(b)};
    };
    const auto ring_along = [&](float s) { const float b = bearing + sense * s / radius; return Vector3{-std::sin(b) * sense, 0, std::cos(b) * sense}; };
    for (int k = 0; k < 4; ++k) {
        const float s = -hw::deck_half + span * (static_cast<float>(k) + .5F) / 4;
        const Vector3 c = ring_point(s, 0), d = ring_along(s);
        float y = deck;
        hw::deck(c.x, c.z, y, &d);
        box(c, d, span / 4 + .2F, width, y - 1.1F, y + lift - .02F, concrete);                     // the slab
        for (const float side : {-1.0F, 1.0F})
            box(ring_point(s, side * hw::parapet), d, span / 4 + .2F, .5F, y, y + 1.05F, concrete);   // parapets
    }
    for (const float side : {-1.0F, 1.0F}) {
        const float s = side * (hw::roundabout_pad + .6F);
        const Vector3 c = ring_point(s, 0), d = ring_along(s);
        float y = deck;
        hw::deck(c.x, c.z, y, &d);
        box(c, d, 1.2F, 2 * hw::grade_flat + 2, under - .5F, y - 1.0F, dark);                      // abutments
        box(ring_point(side * 20, 0), ring_along(side * 20), 2.4F, 5, under, deck - 1.1F, dark);   // piers
        // Retaining walls: the ring's embankment beside each slip road while it climbs.
        for (float t = hw::roundabout_pad + 2; t < hw::slip_reach - 10; t += 4)
            for (const float lateral : {-1.0F, 1.0F}) {
                const Vector3 top = ring_point(side * t, lateral * (hw::grade_flat + .8F)), foot = ring_point(side * t, lateral * (hw::grade_flat + 4.5F));
                const float high = world::terrain::TerrainHeight::sample(top.x, top.z), low = world::terrain::TerrainHeight::sample(foot.x, foot.z);
                if (high - low < .4F) continue;
                box(ring_point(side * t, lateral * (hw::grade_flat + 1.6F)), ring_along(side * t), 4.1F, 1.0F, low - .3F, high + .15F, concrete);
            }
    }
    // The island: a kerb and a lawn, under the bridge.
    const float island = hw::roundabout_radius - hw::roundabout_width * .5F;
    constexpr int pieces = 40;
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(lawn.r, lawn.g, lawn.b, 255);
    for (int k = 0; k < pieces; ++k) {
        const float a0 = 2 * PI * k / pieces, a1 = 2 * PI * (k + 1) / pieces, r = island - .5F;
        const Vector3 p0{x.at.x + std::cos(a0) * r, under + .32F, x.at.z + std::sin(a0) * r}, p1{x.at.x + std::cos(a1) * r, under + .32F, x.at.z + std::sin(a1) * r};
        rlVertex3f(x.at.x, under + .32F, x.at.z); rlVertex3f(p1.x, p1.y, p1.z); rlVertex3f(p0.x, p0.y, p0.z);
    }
    rlEnd();
    for (int k = 0; k < pieces; ++k) {
        const float a = 2 * PI * (k + .5F) / pieces;
        const Vector3 c{x.at.x + std::cos(a) * (island - .25F), 0, x.at.z + std::sin(a) * (island - .25F)};
        box(c, {-std::sin(a), 0, std::cos(a)}, 2 * PI * island / pieces + .15F, .5F, under, under + .3F, kerb);
    }
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


void draw_link(const world::Road& road, int begin, int end) {
    const int segments = world::geometry::road_segments(road);
    begin = std::max(0, begin); end = std::min(segments, end);
    if (segments == 0 || begin >= end) return;
    auto& r = resources();
    const Color white{232, 232, 226, 255};
    if (road.type == world::RoadType::Roundabout) {
        // The piece of the ring it is: its interchange's centre, and the angles at its ends.
        const hw::Interchange* at = nullptr;
        for (const auto& x : hw::interchanges())
            if (std::abs(std::hypot(road.start.x - x.at.x, road.start.z - x.at.z) - hw::roundabout_radius) < 1) at = &x;
        if (!at) {world::geometry::road(road, begin, end); return;}
        float a0 = std::atan2(road.start.z - at->at.z, road.start.x - at->at.x), a1 = std::atan2(road.end.z - at->at.z, road.end.x - at->at.x);
        if (a1 - a0 > PI) a1 -= 2 * PI;
        if (a0 - a1 > PI) a1 += 2 * PI;
        const float f0 = static_cast<float>(begin) / segments, f1 = static_cast<float>(end) / segments;
        const float b0 = a0 + (a1 - a0) * f0, b1 = a0 + (a1 - a0) * f1;
        const auto ring = [&](float a, float radius, float above) {
            const float x = at->at.x + std::cos(a) * radius, z = at->at.z + std::sin(a) * radius;
            return Vector3{x, world::terrain::TerrainHeight::sample(x, z) + above, z};
        };
        const auto band = [&](float r0, float r1, float above, unsigned texture, Color tint) {
            rlSetTexture(texture);
            rlBegin(RL_TRIANGLES);
            rlColor4ub(tint.r, tint.g, tint.b, tint.a);
            const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(b1 - b0) * hw::roundabout_radius / 2)));
            for (int k = 0; k < steps; ++k) {
                const float c0 = b0 + (b1 - b0) * k / steps, c1 = b0 + (b1 - b0) * (k + 1) / steps;
                const Vector3 p00 = ring(c0, r0, above), p01 = ring(c0, r1, above), p10 = ring(c1, r0, above), p11 = ring(c1, r1, above);
                const float u0 = c0 * hw::roundabout_radius / texture_along, u1 = c1 * hw::roundabout_radius / texture_along;
                // (Either winding: the ring runs whichever way round.)
                for (const bool flip : {false, true}) {
                    const auto v = [&](Vector3 p, float u, float w) { rlTexCoord2f(u, w); rlVertex3f(p.x, p.y, p.z); };
                    if (!flip) {v(p00, u0, 0); v(p11, u1, 1); v(p10, u1, 0); v(p00, u0, 0); v(p01, u0, 1); v(p11, u1, 1);}
                    else {v(p00, u0, 0); v(p10, u1, 0); v(p11, u1, 1); v(p00, u0, 0); v(p11, u1, 1); v(p01, u0, 1);}
                }
            }
            rlEnd();
        };
        const float half = hw::roundabout_width * .5F;
        band(hw::roundabout_radius - half, hw::roundabout_radius + half, lift, r.plain.id, asphalt_tint);
        band(hw::roundabout_radius + half - .45F, hw::roundabout_radius + half - .3F, lift + .01F, 0, white);
        band(hw::roundabout_radius - half + .3F, hw::roundabout_radius - half + .45F, lift + .01F, 0, white);
        rlSetTexture(0);
        return;
    }
    // A slip road: asphalt between white edge lines.
    const float length = std::hypot(road.end.x - road.start.x, road.end.z - road.start.z);
    const Vector3 direction{(road.end.x - road.start.x) / length, 0, (road.end.z - road.start.z) / length};
    const Vector3 cross{-direction.z, 0, direction.x};
    const auto frame = [&](int i) {
        const float s = length * static_cast<float>(i) / static_cast<float>(segments);
        return Frame{{road.start.x + direction.x * s, 0, road.start.z + direction.z * s}, cross, direction};
    };
    const float half = road.width * .5F;
    for (int pass = 0; pass < 2; ++pass) {
        rlSetTexture(pass == 0 ? r.plain.id : 0);
        rlBegin(RL_TRIANGLES);
        const Color tint = pass == 0 ? asphalt_tint : white;
        rlColor4ub(tint.r, tint.g, tint.b, tint.a);
        rlNormal3f(0, 1, 0);
        Frame a = frame(begin);
        for (int i = begin; i < end; ++i) {
            const Frame b = frame(i + 1);
            const float sa = length * i / segments, sb = length * (i + 1) / segments;
            if (pass == 0) strip(a, b, sa, sb, -half, half, -half / texture_across, half / texture_across, lift - .01F);
            else for (const float edge : {-half + .3F, half - .45F}) strip(a, b, sa, sb, edge, edge + .15F, 0, 1, lift);
            a = b;
        }
        rlEnd();
    }
    rlSetTexture(0);
}

}
