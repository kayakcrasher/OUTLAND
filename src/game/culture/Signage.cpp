#include "outland/game/culture/Signage.hpp"
#include "outland/game/culture/Esperanto.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>

namespace outland::game::culture {
namespace {
using world::physics::WorldCollision;
using life::PlaceKind;

constexpr Vector3 wind{.8F, 0, .6F};

float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
Vector3 flat_dir(Vector3 from, Vector3 to) {
    const float length = flat(from, to);
    return length < 1e-3F ? Vector3{0, 0, 0} : Vector3{(to.x - from.x) / length, 0, (to.z - from.z) / length};
}
float yaw_of(Vector3 facing) { return std::atan2(facing.x, facing.z) * RAD2DEG; }
float ground(const world::VerdaRegion& region, Vector3 at, float hint) {
    const float terrain = world::terrain::TerrainHeight::sample(at.x, at.z);
    return WorldCollision::ground_height({at.x, std::max(terrain, hint), at.z}, std::max(terrain, hint) + .3F, region, 1.0F);
}
float estimated_width(const Sign& sign) {
    float width = 0;
    for (const auto& line : sign.front) width = std::max(width, static_cast<float>(utf8_codepoints(line.text).size()) * line.height * .66F);
    return std::min(sign.max_width, width + .3F);
}
// Distance from p to the segment ab (xz).
float segment_distance(Vector3 p, Vector3 a, Vector3 b) {
    const float dx = b.x - a.x, dz = b.z - a.z, length2 = dx * dx + dz * dz;
    const float t = length2 < 1e-6F ? 0 : std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / length2, 0.0F, 1.0F);
    return std::hypot(p.x - (a.x + dx * t), p.z - (a.z + dz * t));
}
float road_clearance(const world::VerdaRegion& region, Vector3 p) {
    float best = std::numeric_limits<float>::max();
    for (const auto& town : region.settlements()) for (const auto& road : town.roads)
        best = std::min(best, segment_distance(p, road.start, road.end) - road.width * .5F);
    return best;
}
bool clear_spot(const world::VerdaRegion& region, Vector3 p, float radius) {
    if (road_clearance(region, p) <= 1.5F) return false;
    const float feet = ground(region, p, p.y);
    return !WorldCollision::body_blocked({p.x, feet, p.z}, feet, region, radius);
}

struct Look { Color board, ink; };
Look look(PlaceKind kind, bool tower) {
    switch (kind) {
        case PlaceKind::Shop: return {{236, 226, 200, 255}, {20, 70, 40, 255}};
        case PlaceKind::Pub: return {{70, 40, 25, 255}, {235, 200, 120, 255}};
        case PlaceKind::Church: return {{240, 238, 230, 255}, {40, 40, 70, 255}};
        case PlaceKind::Police: return {{25, 45, 95, 255}, {255, 255, 255, 255}};
        case PlaceKind::Clinic: return {{245, 245, 245, 255}, {0, 130, 60, 255}};
        case PlaceKind::Garage: return {{240, 190, 40, 255}, {20, 20, 20, 255}};
        case PlaceKind::Industrial: return {{90, 95, 100, 255}, {255, 255, 255, 255}};
        case PlaceKind::Dock: return {{30, 90, 140, 255}, {255, 255, 255, 255}};
        case PlaceKind::Farm: return {{120, 85, 50, 255}, {250, 240, 210, 255}};
        default: return tower ? Look{{215, 210, 195, 255}, {25, 60, 35, 255}} : Look{verda_green, {255, 255, 255, 255}};
    }
}

// Who runs a place: the first resident working there in the trade that owns it.
std::string owner_of(const life::Island& island, int place) {
    for (const auto& r : island.residents)
        if (r.work == place && (r.occupation == life::Occupation::Shopkeeper || r.occupation == life::Occupation::Publican))
            return r.family_name;
    return {};
}

// The facade behind a door: march from outside the doorway toward the interior at sign height
// until something solid is met. The doorway itself is open, but the lintel above it is not.
Vector3 facade(const world::VerdaRegion& region, const life::Place& place, Vector3 out, float sign_y) {
    const Vector3 start{place.door.x + out.x * .6F, sign_y, place.door.z + out.z * .6F};
    for (int k = 0; k < 100; ++k) {
        const Vector3 p{start.x - out.x * .05F * k, sign_y, start.z - out.z * .05F * k};
        if (WorldCollision::body_blocked(p, sign_y - 1.3F, region, .12F)) return {p.x + out.x * .12F, sign_y, p.z + out.z * .12F};
    }
    // Nothing solid found (an open-sided shelter): the footprint's front wall.
    const auto wall = life::door_position(place.interior, place.building_size, place.building_yaw, 0);
    return {wall.x, sign_y, wall.z};
}

bool procedural(const world::VerdaRegion& region, const std::string& id) {
    for (const auto& town : region.settlements())
        for (const auto& building : town.buildings) if (building.id == id) return true;
    return false;
}

void shopfronts(const world::VerdaRegion& region, const life::Island& island, Signage& out) {
    const auto& towns = region.settlements();
    for (std::size_t i = 0; i < island.places.size(); ++i) {
        const auto& place = island.places[i];
        if (place.kind == PlaceKind::Home || place.kind == PlaceKind::Square) continue;
        const auto s = static_cast<std::size_t>(std::max(0, place.settlement));
        const Vector3 centre = s < towns.size() ? towns[s].center : place.interior;
        const bool capital = s < towns.size() && towns[s].id.find("capital") != std::string::npos;
        int ordinal = 0;
        for (const auto& other : island.places)
            if (other.kind == place.kind && other.settlement == place.settlement && &other != &place &&
                (flat(other.door, centre) < flat(place.door, centre) ||
                 (flat(other.door, centre) == flat(place.door, centre) && other.id < place.id))) ++ordinal;
        const auto text = place_sign(place.kind, text_hash(s < towns.size() ? towns[s].id : place.id), owner_of(island, static_cast<int>(i)), ordinal, capital);
        if (text.title.empty()) continue;
        const auto colours = look(place.kind, place.sealed);
        Sign sign;
        sign.place = place.id;
        sign.board = colours.board; sign.ink = colours.ink;
        if (!place.indoor) {
            // Fields: a sign on a post by the track, facing town.
            const Vector3 face = flat_dir(place.door, centre);
            sign.kind = SignKind::Notice;
            sign.post = 1.3F;
            sign.front = {{text.title, .38F}, {text.subtitle, .24F}};
            sign.yaw = yaw_of(face);
            sign.max_width = 3.4F;
            const float base = ground(region, place.door, place.door.y);
            sign.position = {place.door.x + face.x * 3, base + sign.post + .5F, place.door.z + face.z * 3};
            out.signs.push_back(std::move(sign));
            continue;
        }
        Vector3 facing = flat_dir(place.interior, place.door);
        if (Vector3Length(facing) < .5F) continue;
        const bool marker = place.building_size.x <= 3.01F && place.building_size.z <= 3.01F;
        Vector3 wall;
        if (procedural(region, place.building_id)) {
            // Procedural houses stand on the terrain at their centre; the front wall's outer face is
            // half a wall (0.12 m) beyond the footprint, and the doorway is 2.35 m tall.
            const float base = world::terrain::TerrainHeight::sample(place.interior.x, place.interior.z);
            const auto front = life::door_position(place.interior, place.building_size, place.building_yaw, .12F);
            wall = {front.x, base + std::clamp(place.building_size.y * .6F, 3.0F, 3.6F), front.z};
        } else {
            const float base = ground(region, place.door, place.door.y);
            const float lift = place.sealed ? 4.4F : std::clamp(place.building_size.y * .45F, 2.7F, 3.5F);
            wall = facade(region, place, facing, base + lift);
        }
        sign.kind = SignKind::Shopfront;
        sign.yaw = yaw_of(facing);
        sign.position = {wall.x + facing.x * .11F, wall.y, wall.z + facing.z * .11F};
        sign.max_width = place.sealed ? 12.0F : marker ? 5.0F : std::clamp(place.building_size.x * .85F, 1.6F, 9.0F);
        const float title = place.sealed ? .8F : .5F;
        sign.front.push_back({text.title, title});
        if (!text.subtitle.empty()) sign.front.push_back({text.subtitle, title * .45F});
        // Government buildings fly the flag from a staff beside the sign.
        if (place.kind == PlaceKind::Police || (place.kind == PlaceKind::Office && ordinal == 0)) {
            const Vector3 side{facing.z, 0, -facing.x};
            const float reach = estimated_width(sign) * .5F + .7F;
            FlagPole flag;
            flag.place = place.id;
            flag.base = {wall.x + side.x * reach, wall.y + .3F, wall.z + side.z * reach};
            const Vector3 staff = Vector3Normalize({facing.x * .75F, .65F, facing.z * .75F});
            flag.tip = Vector3Add(flag.base, Vector3Scale(staff, place.sealed ? 3.0F : 2.2F));
            flag.fly = side;
            flag.width = place.sealed ? 2.0F : 1.4F;
            out.flags.push_back(flag);
        }
        out.signs.push_back(std::move(sign));
    }
}

// Town squares fly the flag; the capital's courthouse square gets its name too.
void squares(const world::VerdaRegion& region, const life::Island& island, Signage& out) {
    const auto& towns = region.settlements();
    for (std::size_t s = 0; s < towns.size(); ++s) {
        Vector3 anchor = towns[s].center;
        bool courthouse = false;
        for (const auto& owner : towns) for (const auto& asset : owner.assets)
            if (asset.model_path.find("courthouse_square") != std::string::npos && flat(asset.position, towns[s].center) < 250) {
                anchor = asset.position; courthouse = true;
            }
        Vector3 gather = Vector3Add(towns[s].center, {4, 0, 4});
        for (const auto& place : island.places)
            if (place.kind == PlaceKind::Square && place.settlement == static_cast<int>(s)) gather = place.door;
        Vector3 spot = anchor;
        bool found = false;
        for (float radius = courthouse ? 12.0F : 3.0F; radius < 32 && !found; radius += 1.5F)
            for (int a = 0; a < 16 && !found; ++a) {
                const float angle = (a + .5F) * 2 * PI / 16;
                const Vector3 p{anchor.x + std::cos(angle) * radius, anchor.y, anchor.z + std::sin(angle) * radius};
                if (flat(p, gather) > 3 && clear_spot(region, p, 1.0F)) {spot = p; found = true;}
            }
        if (!found) continue;
        const float base = ground(region, spot, spot.y);
        FlagPole flag;
        flag.place = towns[s].id + ":square";
        flag.base = {spot.x, base, spot.z};
        flag.tip = {spot.x, base + (courthouse ? 11.0F : 8.0F), spot.z};
        flag.fly = Vector3Normalize(wind);
        flag.width = courthouse ? 3.2F : 2.4F;
        out.flags.push_back(flag);
        if (courthouse) {
            Sign sign;
            sign.kind = SignKind::Notice;
            sign.place = flag.place;
            const Vector3 face = flat_dir(spot, anchor);
            sign.yaw = yaw_of(Vector3Scale(face, -1));
            sign.post = 1.0F;
            sign.position = {spot.x - face.x * 1.6F, base + 1.5F, spot.z - face.z * 1.6F};
            sign.front = {{"PLACO DE LA RESPUBLIKO", .3F}, {"FONDITA 1961", .18F}};
            sign.back = sign.front;
            sign.max_width = 4.2F;
            sign.board = {40, 40, 40, 255}; sign.ink = {230, 200, 120, 255};
            out.signs.push_back(std::move(sign));
        }
    }
    // The tallest towers fly it from the roof.
    for (const auto& town : towns) for (const auto& asset : town.assets) {
        if (asset.model_path.find("/city/towers/") == std::string::npos || asset.size.y < 50) continue;
        FlagPole flag;
        flag.place = asset.id;
        flag.base = {asset.position.x, asset.position.y + asset.size.y, asset.position.z};
        flag.tip = Vector3Add(flag.base, {0, 7, 0});
        flag.fly = Vector3Normalize(wind);
        flag.width = 3.6F;
        out.flags.push_back(flag);
    }
}

void bus_stops(const world::VerdaRegion& region, Signage& out) {
    for (const auto& town : region.settlements()) for (const auto& asset : town.assets) {
        if (asset.model_path.find("Bus stops/") == std::string::npos) continue;
        Sign sign;
        sign.kind = SignKind::BusStop;
        sign.place = asset.id;
        sign.front = {{"BUSHALTEJO", .26F}, {"LINIO " + std::to_string(1 + text_hash(asset.id) % 9), .18F}};
        sign.back = sign.front;
        sign.board = {250, 205, 40, 255}; sign.ink = {25, 25, 25, 255};
        sign.max_width = 2.2F;
        const float a = asset.rotation_y * DEG2RAD;
        const auto world_of = [&](float x, float z) {
            return Vector3{asset.position.x + x * std::cos(a) + z * std::sin(a), asset.position.y, asset.position.z - x * std::sin(a) + z * std::cos(a)};
        };
        const bool long_x = asset.size.x >= asset.size.z;
        if (asset.size.y > 2) {
            // A shelter: the sign stands on its roof, along its length.
            sign.position = {asset.position.x, asset.position.y + asset.size.y + .45F, asset.position.z};
            sign.yaw = asset.rotation_y + (long_x ? 0.0F : 90.0F);
        } else {
            // A bench: a post at one end.
            const auto end = long_x ? world_of(asset.size.x * .5F + .5F, 0) : world_of(0, asset.size.z * .5F + .5F);
            sign.post = 2.0F;
            sign.position = {end.x, ground(region, end, asset.position.y) + 2.3F, end.z};
            sign.yaw = asset.rotation_y + (long_x ? 0.0F : 90.0F);
        }
        out.signs.push_back(std::move(sign));
    }
}

// Street-name blades on the downtown traffic signals, one per crossing street.
void street_blades(const world::VerdaRegion& region, Signage& out) {
    struct Line { int angle, offset; Vector3 direction; float width; };
    std::vector<Line> lines;
    const auto line_of = [](const world::Road& road) {
        Vector3 d = flat_dir(road.start, road.end);
        if (d.x < -1e-3F || (std::abs(d.x) <= 1e-3F && d.z < 0)) d = Vector3Scale(d, -1);
        const int angle = static_cast<int>(std::lround(std::atan2(d.z, d.x) * RAD2DEG));
        const int offset = static_cast<int>(std::lround(road.start.x * -d.z + road.start.z * d.x));
        return Line{angle, offset, d, road.width};
    };
    std::vector<std::pair<const world::Road*, Line>> roads;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) roads.emplace_back(&road, line_of(road));
    std::map<std::pair<int, int>, int> names;
    for (const auto& town : region.settlements()) for (const auto& asset : town.assets) {
        if (asset.model_path.find("Traffic lights/traffic_light") == std::string::npos) continue;
        std::vector<std::pair<float, Line>> near;
        for (const auto& [road, line] : roads) {
            const float d = segment_distance(asset.position, road->start, road->end) - road->width * .5F;
            if (d < 4) near.emplace_back(d, line);
        }
        std::sort(near.begin(), near.end(), [](const auto& a, const auto& b) {return a.first < b.first;});
        std::vector<Line> chosen;
        for (const auto& [d, line] : near) {
            if (std::any_of(chosen.begin(), chosen.end(), [&](const Line& c) {
                    const int turn = std::abs(c.angle - line.angle) % 180;
                    return std::min(turn, 180 - turn) < 30;})) continue;
            chosen.push_back(line);
            if (chosen.size() == 2) break;
        }
        float height = 3.0F;
        for (const auto& line : chosen) {
            const auto key = std::make_pair(line.angle, line.offset);
            if (!names.contains(key)) names.emplace(key, static_cast<int>(names.size()));
            Sign sign;
            sign.kind = SignKind::Street;
            sign.place = asset.id;
            sign.front = {{street_name(names[key]), .2F}};
            sign.back = sign.front;
            sign.max_width = 3.0F;
            sign.position = {asset.position.x, asset.position.y + height, asset.position.z};
            sign.yaw = yaw_of({-line.direction.z, 0, line.direction.x});
            height += .62F; // stacked clear of each other, crossing at the pole
            out.signs.push_back(std::move(sign));
        }
    }
}

// "BONVENON AL ..." where each road enters a town, and on the way out the distances onward.
void town_roads(const world::VerdaRegion& region, const life::Island& island, Signage& out) {
    const auto& towns = region.settlements();
    std::vector<float> radius(towns.size(), 45);
    for (const auto& place : island.places)
        if (place.settlement >= 0 && static_cast<std::size_t>(place.settlement) < towns.size() && place.indoor)
            radius[static_cast<std::size_t>(place.settlement)] = std::max(radius[static_cast<std::size_t>(place.settlement)],
                flat(place.door, towns[static_cast<std::size_t>(place.settlement)].center) + 18);
    for (auto& r : radius) r = std::min(r, 230.0F);

    // Road graph: endpoints within a metre are one node.
    std::vector<Vector3> nodes;
    struct Edge { int to; float length; };
    std::vector<std::vector<Edge>> edges;
    const auto node = [&](Vector3 p) {
        for (std::size_t i = 0; i < nodes.size(); ++i) if (flat(nodes[i], p) < 1) return static_cast<int>(i);
        nodes.push_back(p); edges.emplace_back();
        return static_cast<int>(nodes.size() - 1);
    };
    std::vector<const world::Road*> roads;
    for (const auto& town : towns) for (const auto& road : town.roads) {
        const int a = node(road.start), b = node(road.end);
        const float length = flat(road.start, road.end);
        edges[static_cast<std::size_t>(a)].push_back({b, length});
        edges[static_cast<std::size_t>(b)].push_back({a, length});
        roads.push_back(&road);
    }

    for (std::size_t s = 0; s < towns.size(); ++s) {
        const Vector3 c = towns[s].center;
        const float R = radius[s];
        struct Exit { Vector3 at, out; float width; std::vector<float> to; };
        std::vector<Exit> exits;
        for (const auto* road : roads) {
            const float da = flat(road->start, c), db = flat(road->end, c);
            if ((da < R) == (db < R)) continue;
            const Vector3 inside = da < R ? road->start : road->end, outside = da < R ? road->end : road->start;
            // Where the segment crosses the circle.
            float lo = 0, hi = 1;
            for (int k = 0; k < 30; ++k) {
                const float mid = (lo + hi) * .5F;
                (flat(Vector3Lerp(inside, outside, mid), c) < R ? lo : hi) = mid;
            }
            const Vector3 at = Vector3Lerp(inside, outside, lo);
            if (std::any_of(exits.begin(), exits.end(), [&](const Exit& e) {return flat(e.at, at) < 12;})) continue;
            Exit exit{at, flat_dir(inside, outside), road->width, std::vector<float>(towns.size(), -1)};
            // Shortest road distance to every other town, leaving this way (never back through town).
            std::vector<float> cost(nodes.size(), std::numeric_limits<float>::max());
            using Item = std::pair<float, int>;
            std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
            const int start = node(outside);
            cost[static_cast<std::size_t>(start)] = flat(at, outside);
            open.emplace(cost[static_cast<std::size_t>(start)], start);
            while (!open.empty()) {
                const auto [d, n] = open.top(); open.pop();
                if (d > cost[static_cast<std::size_t>(n)]) continue;
                for (const auto& e : edges[static_cast<std::size_t>(n)]) {
                    if (flat(nodes[static_cast<std::size_t>(e.to)], c) < R) continue;
                    const float next = d + e.length;
                    if (next < cost[static_cast<std::size_t>(e.to)]) {cost[static_cast<std::size_t>(e.to)] = next; open.emplace(next, e.to);}
                }
            }
            for (std::size_t t = 0; t < towns.size(); ++t) {
                if (t == s) continue;
                for (std::size_t n = 0; n < nodes.size(); ++n) {
                    if (cost[n] == std::numeric_limits<float>::max() || flat(nodes[n], towns[t].center) > radius[t]) continue;
                    const float d = cost[n] + flat(nodes[n], towns[t].center);
                    if (exit.to[t] < 0 || d < exit.to[t]) exit.to[t] = d;
                }
            }
            exits.push_back(std::move(exit));
        }
        const std::string name = towns[s].name.empty() ? towns[s].id : towns[s].name;
        const bool capital = towns[s].id.find("capital") != std::string::npos;
        for (const auto& exit : exits) {
            const Vector3 d = exit.out;
            const float aside = exit.width * .5F + 1.8F;
            // Entry: on the incoming driver's right, facing them.
            const Vector3 right_in{d.z, 0, -d.x};
            Vector3 p{exit.at.x + right_in.x * aside, 0, exit.at.z + right_in.z * aside};
            for (int k = 0; k < 4 && !clear_spot(region, p, .6F); ++k) p = Vector3Add(p, right_in);
            Sign entry;
            entry.kind = SignKind::TownEntry;
            entry.place = towns[s].id;
            entry.post = 1.4F;
            entry.position = {p.x, ground(region, p, c.y) + entry.post + 1.0F, p.z};
            entry.yaw = yaw_of(d);
            entry.front = {{"BONVENON AL", .34F}, {upper(name), .85F}, {capital ? "ĈEFURBO DE VERDA" : "RESPUBLIKO VERDA", .28F}};
            entry.back = {{"ĜIS REVIDO!", .55F}};
            entry.max_width = 6.5F;
            out.signs.push_back(std::move(entry));

            // Onward: the towns this road is the best way to, nearest first.
            std::vector<std::pair<float, std::size_t>> onward;
            for (std::size_t t = 0; t < towns.size(); ++t) {
                if (exit.to[t] < 0) continue;
                float best = exit.to[t];
                for (const auto& other : exits) if (other.to[t] >= 0) best = std::min(best, other.to[t]);
                if (exit.to[t] <= best + 30) onward.emplace_back(exit.to[t], t);
            }
            std::sort(onward.begin(), onward.end());
            if (onward.empty()) continue;
            const Vector3 right_out{-d.z, 0, d.x};
            Vector3 q{exit.at.x - d.x * 10 + right_out.x * aside, 0, exit.at.z - d.z * 10 + right_out.z * aside};
            for (int k = 0; k < 4 && !clear_spot(region, q, .6F); ++k) q = Vector3Add(q, right_out);
            Sign distance;
            distance.kind = SignKind::Distance;
            distance.place = towns[s].id;
            distance.post = 1.3F;
            distance.yaw = yaw_of(Vector3Scale(d, -1));
            distance.board = {245, 245, 240, 255}; distance.ink = {20, 60, 30, 255};
            distance.max_width = 4.2F;
            for (std::size_t k = 0; k < onward.size() && k < 3; ++k) {
                const auto& town = towns[onward[k].second];
                distance.front.push_back({(town.name.empty() ? town.id : town.name) + "  " + format_distance(onward[k].first + 10), .36F});
            }
            const float rows = static_cast<float>(distance.front.size());
            distance.position = {q.x, ground(region, q, c.y) + distance.post + rows * .22F + .15F, q.z};
            out.signs.push_back(std::move(distance));
        }
    }
}
}

Signage build_signage(const world::VerdaRegion& region, const life::Island& island) {
    Signage out;
    shopfronts(region, island, out);
    squares(region, island, out);
    bus_stops(region, out);
    street_blades(region, out);
    town_roads(region, island, out);
    for (auto& sign : out.signs)
        std::erase_if(sign.front, [](const SignLine& line) {return line.text.empty();});
    return out;
}

Image flag_image(int width) {
    width = std::max(16, width);
    const int height = width / 2;
    // Drawn at twice the size and shrunk, for soft edges.
    const int W = width * 2, H = height * 2;
    Image image = GenImageColor(W, H, WHITE);
    auto* pixels = static_cast<Color*>(image.data);
    const float h = static_cast<float>(H);
    const Vector2 star_centre{h * std::sqrt(3.0F) / 6, h / 2};
    std::vector<Vector2> star;
    for (int k = 0; k < 10; ++k) {
        const float angle = -PI / 2 + k * PI / 5, r = (k % 2 ? .38F : 1.0F) * h * .15F;
        star.push_back({star_centre.x + std::cos(angle) * r, star_centre.y + std::sin(angle) * r});
    }
    const auto in_polygon = [](const std::vector<Vector2>& polygon, float x, float y) {
        bool inside = false;
        for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
            if ((polygon[i].y > y) != (polygon[j].y > y) &&
                x < (polygon[j].x - polygon[i].x) * (y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x) inside = !inside;
        return inside;
    };
    const float tip = h * std::sqrt(3.0F) / 2, rim = h * .022F;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        const float fx = x + .5F, fy = y + .5F;
        const int stripe = static_cast<int>(fy / h * 5);
        Color c = stripe % 2 == 0 ? verda_green : WHITE;
        // Hoist triangle: equilateral, side = flag height; a thin green rim keeps it apart from
        // the white stripes.
        const float half = (h / 2) * (1 - fx / tip);
        const float off = std::abs(fy - h / 2);
        if (fx < tip && off < half) {
            c = (half - off) * std::cos(PI / 6) < rim ? verda_green : WHITE;
            if (in_polygon(star, fx, fy)) c = verda_green;
        }
        pixels[y * W + x] = c;
    }
    ImageResize(&image, width, height);
    return image;
}
}
