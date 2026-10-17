#include "outland/game/services/HighwayPatrol.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/Highways.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::game::services {
namespace {
const world::highways::Line* ring() {
    for (const auto& line : world::highways::lines()) if (line.loop) return &line;
    return nullptr;
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
const Color orange{232, 118, 28, 255};
}

std::vector<Outpost> plan_outposts(Vector3 capital, const std::function<bool(Vector3)>& free) {
    std::vector<Outpost> out;
    const auto* loop = ring();
    if (!loop || loop->points.size() < 16) return out;
    const auto& p = loop->points;
    const int n = static_cast<int>(p.size());
    Vector3 middle{};
    for (const auto& q : p) middle = Vector3Add(middle, q);
    middle = Vector3Scale(middle, 1.0F / static_cast<float>(n));
    int first = 0;
    for (int i = 1; i < n; ++i) if (flat(p[static_cast<std::size_t>(i)], capital) < flat(p[static_cast<std::size_t>(first)], capital)) first = i;
    // Clear of the junctions: no point of another highway within this.
    const auto clear = [&](Vector3 at) {
        for (const auto& line : world::highways::lines()) {
            if (&line == loop) continue;
            for (const auto& q : line.points) if (flat(q, at) < 70) return false;
        }
        return true;
    };
    const float edge = world::highways::paved_half + world::highways::verge;
    const auto beside = [&](Vector3 here, Vector3 outward) {
        Outpost post;
        post.at = Vector3Add(here, Vector3Scale(outward, edge + 11));
        post.yaw = std::atan2(-outward.x, -outward.z) * RAD2DEG;      // facing the road
        // Under the carport on the building's +X side (the model turned to face the road), nose out.
        const float a = post.yaw * DEG2RAD;
        post.parking = Vector3Add(post.at, Vector3Scale({-std::cos(a), 0, std::sin(a)}, outpost_carport));
        post.parking_yaw = post.yaw;
        return post;
    };
    // The whole lot clear: the building, its carport and the yard in front.
    const auto open = [&](const Outpost& post) {
        if (!free) return true;
        const float a = post.yaw * DEG2RAD;
        const Vector3 ahead{std::sin(a), 0, std::cos(a)}, back = Vector3Scale(ahead, -4);
        return free(post.at) && free(post.parking) && free(Vector3Add(post.at, Vector3Scale(ahead, 5))) &&
               free(Vector3Add(post.at, back)) && free(Vector3Add(post.parking, back));
    };
    // The first, the patrol's home: beside a spoke, as near half a kilometre out of Verda City as
    // there is open ground.
    {
        Outpost found;
        float best = 1e9F;
        for (const auto& line : world::highways::lines()) {
            if (line.loop) continue;
            for (std::size_t i = 0; i + 1 < line.points.size(); ++i) {
                const float out_of_town = flat(line.points[i], capital);
                if (out_of_town < 380 || out_of_town > 1100) continue;
                const float score = std::abs(out_of_town - 520);
                if (score >= best) continue;
                Vector3 along = Vector3Subtract(line.points[i + 1], line.points[i]); along.y = 0;
                along = Vector3Normalize(along);
                for (const float side : {1.0F, -1.0F}) {
                    const auto post = beside(line.points[i], {side * along.z, 0, -side * along.x});
                    if (!open(post)) continue;
                    found = post; best = score;
                    break;
                }
            }
        }
        if (best < 1e9F) out.push_back(found);
    }
    // The rest round the ring, evenly, clear of the junctions.
    const int rest = max_outposts - static_cast<int>(out.size());
    for (int k = 0; k < rest; ++k) {
        int i = (first + k * n / rest) % n;
        const auto at = [&](int index) {
            const Vector3 here = p[static_cast<std::size_t>(index)];
            Vector3 outward = Vector3Subtract(here, middle); outward.y = 0;
            return beside(here, Vector3Normalize(outward));
        };
        for (int step = 0; step < n / 8 && (!clear(p[static_cast<std::size_t>(i)]) || !open(at(i))); ++step) i = (i + 1) % n;
        out.push_back(at(i));
    }
    return out;
}

int outposts_open(float law, bool clean) {
    if (clean) return max_outposts;
    return std::clamp(1 + static_cast<int>(std::floor(std::clamp(law, 0.0F, 1.0F) * 4 + 1e-4F)), 1, max_outposts);
}

std::vector<Vector3> patrol_loop(float spacing) {
    std::vector<Vector3> out;
    const auto* loop = ring();
    if (!loop || loop->points.empty()) return out;
    float since = spacing;
    Vector3 last = loop->points.front();
    for (const auto& q : loop->points) {
        since += flat(q, last);
        last = q;
        if (since >= spacing) { out.push_back(q); since = 0; }
    }
    return out;
}

void HighwayPatrolArt::prepare(const Font& font) {
    if (ready_ || !IsWindowReady()) return;
    badge_ = LoadRenderTexture(512, 256);
    SetTextureFilter(badge_.texture, TEXTURE_FILTER_BILINEAR);
    BeginTextureMode(badge_);
    ClearBackground(BLANK);
    // The orange star, outlined in brown.
    const Vector2 c{256, 92};
    for (const auto& [r, colour] : {std::pair{78.0F, Color{70, 44, 20, 255}}, std::pair{68.0F, orange}}) {
        Vector2 pts[10];
        for (int k = 0; k < 10; ++k) {
            const float a = -PI / 2 + static_cast<float>(k) * PI / 5;
            const float rr = k % 2 ? r * .45F : r;
            pts[k] = {c.x + std::cos(a) * rr, c.y + std::sin(a) * rr};
        }
        for (int k = 0; k < 10; ++k) {
            DrawTriangle(c, pts[(k + 1) % 10], pts[k], colour);
            DrawTriangle(c, pts[k], pts[(k + 1) % 10], colour);
        }
    }
    DrawCircleV(c, 16, Color{255, 236, 190, 255});
    const char* name = "ŜOSEA PATROLO";
    const float size = 58;
    const Vector2 m = MeasureTextEx(font, name, size, 3);
    DrawTextEx(font, name, {256 - m.x / 2 + 3, 182 + 3}, size, 3, Color{60, 36, 16, 220});
    DrawTextEx(font, name, {256 - m.x / 2, 182}, size, 3, orange);
    EndTextureMode();
    ready_ = true;
}

void HighwayPatrolArt::unload() {
    if (!ready_) return;
    UnloadRenderTexture(badge_);
    ready_ = false;
}

void HighwayPatrolArt::draw_outpost(const Outpost& post, Vector3 camera) const {
    if (!ready_ || flat(post.at, camera) > 320) return;
    // The building is the city kit's (outpost_model); this is the star sign standing on its roof
    // over the front, above the orange fascia.
    const float a = post.yaw * DEG2RAD;
    const Vector3 n{std::sin(a), 0, std::cos(a)}, right{std::cos(a), 0, -std::sin(a)};
    const Vector3 c = Vector3Add(post.at, Vector3Add(Vector3Scale(n, 2.9F), {0, 4.4F, 0}));
    const float w = 1.5F, h = .75F;
    const Vector3 bl = Vector3Subtract(Vector3Subtract(c, Vector3Scale(right, w)), {0, h, 0});
    const Vector3 br = Vector3Subtract(Vector3Add(c, Vector3Scale(right, w)), {0, h, 0});
    const Vector3 tr = Vector3Add(Vector3Add(c, Vector3Scale(right, w)), {0, h, 0});
    const Vector3 tl = Vector3Add(Vector3Subtract(c, Vector3Scale(right, w)), {0, h, 0});
    // Two legs and a brown backing board.
    for (const float side : {-1.0F, 1.0F}) {
        const Vector3 leg = Vector3Add(Vector3Subtract(c, Vector3Scale(n, .06F)), Vector3Scale(right, side * (w - .25F)));
        DrawCube({leg.x, post.at.y + 3.55F + .3F, leg.z}, .1F, .6F, .1F, Color{120, 124, 130, 255});
    }
    rlPushMatrix();
    rlTranslatef(c.x - n.x * .05F, c.y, c.z - n.z * .05F);
    rlRotatef(post.yaw, 0, 1, 0);
    DrawCube({0, 0, 0}, w * 2 + .12F, h * 2 + .12F, .06F, Color{92, 62, 34, 255});
    rlPopMatrix();
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlSetTexture(badge_.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlTexCoord2f(0, 0); rlVertex3f(bl.x, bl.y, bl.z);
    rlTexCoord2f(1, 0); rlVertex3f(br.x, br.y, br.z);
    rlTexCoord2f(1, 1); rlVertex3f(tr.x, tr.y, tr.z);
    rlTexCoord2f(0, 1); rlVertex3f(tl.x, tl.y, tl.z);
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

void HighwayPatrolArt::draw_livery(Vector3 position, float yaw, Vector3 camera) const {
    if (!ready_ || flat(position, camera) > 90) return;
    const auto at = [&](float x, float y, float z) { return vehicles::VehicleSystem::world_point({x, y, z}, position, yaw); };
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlSetTexture(badge_.texture.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    // Both front doors (render texture: v runs bottom to top), the badge reading front to back.
    for (const float side : {-1.0F, 1.0F}) {
        const float x = side * .935F, y0 = .34F, y1 = .69F, z0 = side > 0 ? .95F : -.2F, z1 = side > 0 ? -.2F : .95F;
        rlTexCoord2f(0, 0); rlVertex3f(at(x, y0, z0).x, at(x, y0, z0).y, at(x, y0, z0).z);
        rlTexCoord2f(1, 0); rlVertex3f(at(x, y0, z1).x, at(x, y0, z1).y, at(x, y0, z1).z);
        rlTexCoord2f(1, 1); rlVertex3f(at(x, y1, z1).x, at(x, y1, z1).y, at(x, y1, z1).z);
        rlTexCoord2f(0, 1); rlVertex3f(at(x, y1, z0).x, at(x, y1, z0).y, at(x, y1, z0).z);
    }
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

} // namespace outland::game::services
