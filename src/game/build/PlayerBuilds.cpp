#include "outland/game/build/PlayerBuilds.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace outland::game::build {

namespace {
using world::physics::WorldCollision;
constexpr const char* bus_stop_model = "assets/verda/transit/bus_stop.glb";
constexpr const char* lamp_model = "assets/verda/transit/lamppost.glb";
constexpr std::array<PieceInfo, piece_count> infos{{
    {"ROAD", "Tap PLACE at each end of the road", {8, .1F, 0}, 0},
    {"COTTAGE", "A single-storey home for a family of four", {13, 4, 10}, 4},
    {"HOUSE", "A two-storey home: room for eight", {11, 7, 9}, 8},
    {"BUS STOP", "Beside a road: buses bring newcomers to your empty homes", {4.17F, 3.19F, 1.36F}, 0},
    {"STREET LAMP", "Lights your street at night", {1.05F, 10.5F, 1.05F}, 0},
    {"TREE", "Shade for the front yard", {1, 7, 1}, 0},
}};
const char* kind_name(Piece piece) {
    switch (piece) {
        case Piece::Road: return "road";
        case Piece::Cottage: return "home_cottage";
        case Piece::House: return "home_house";
        case Piece::BusStop: return "stop";
        case Piece::Lamp: return "lamp";
        default: return "tree";
    }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float ground(float x, float z) { return world::terrain::TerrainHeight::sample(x, z); }
float segment_distance(Vector3 p, Vector3 a, Vector3 b) {
    const float dx = b.x - a.x, dz = b.z - a.z, length2 = dx * dx + dz * dz;
    const float t = length2 < 1e-6F ? 0 : std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / length2, 0.0F, 1.0F);
    return std::hypot(p.x - (a.x + dx * t), p.z - (a.z + dz * t));
}
// Ground point at local (x, z) of a footprint centred on `c`, turned by `yaw` degrees.
Vector3 local(Vector3 c, float yaw, float x, float z) {
    const float a = yaw * DEG2RAD;
    return {c.x + x * std::cos(a) + z * std::sin(a), 0, c.z - x * std::sin(a) + z * std::cos(a)};
}
// How far the footprint's edge is from the nearest road edge (negative: on it).
float road_gap(const world::VerdaRegion& region, Vector3 c, float yaw, Vector3 size) {
    float gap = std::numeric_limits<float>::max();
    for (const auto& town : region.settlements()) for (const auto& road : town.roads)
        for (const float x : {-.5F, 0.0F, .5F}) for (const float z : {-.5F, 0.0F, .5F}) {
            const Vector3 p = local(c, yaw, x * size.x, z * size.z);
            gap = std::min(gap, segment_distance(p, road.start, road.end) - road.width * .5F);
        }
    return gap;
}
std::size_t nearest_town(const std::vector<world::Settlement>& towns, Vector3 at) {
    std::size_t best = 0; float distance = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < towns.size(); ++i)
        if (const float d = flat(towns[i].center, at); d < distance) { distance = d; best = i; }
    return best;
}
}

const PieceInfo& piece_info(Piece piece) { return infos[static_cast<std::size_t>(piece)]; }

std::string Placed::id() const { return std::string("player_") + kind_name(piece) + "_" + std::to_string(serial); }

bool PlayerBuilds::snap_bus_stop(const world::VerdaRegion& region, Vector3 near, Vector3& shelter, float& yaw) {
    float best = 14;
    const world::Road* road = nullptr;
    for (const auto& town : region.settlements()) for (const auto& r : town.roads) {
        const float d = segment_distance(near, r.start, r.end) - r.width * .5F;
        if (d < best) { best = d; road = &r; }
    }
    if (!road) return false;
    const Vector3 along = Vector3Normalize({road->end.x - road->start.x, 0, road->end.z - road->start.z});
    const float dx = road->end.x - road->start.x, dz = road->end.z - road->start.z;
    const float t = std::clamp(((near.x - road->start.x) * dx + (near.z - road->start.z) * dz) / std::max(1e-6F, dx * dx + dz * dz), 0.0F, 1.0F);
    const Vector3 kerb{road->start.x + dx * t, 0, road->start.z + dz * t};
    Vector3 side{-along.z, 0, along.x};
    if ((near.x - kerb.x) * side.x + (near.z - kerb.z) * side.z < 0) side = Vector3Scale(side, -1);
    shelter = Vector3Add(kerb, Vector3Scale(side, road->width * .5F + 2.2F));
    shelter.y = ground(shelter.x, shelter.z);
    // The shelter's open front (its local -z) looks back at the road.
    yaw = std::atan2(side.x, side.z) * RAD2DEG;
    return true;
}

std::string PlayerBuilds::check(const world::VerdaRegion& region, const Placed& c) const {
    const auto& info = piece_info(c.piece);
    if (ground(c.position.x, c.position.z) < .6F) return "Too close to the sea";
    if (c.piece == Piece::Road) {
        const float length = flat(c.position, c.end);
        if (length < 6) return "Make the road at least 6 m long";
        if (length > 160) return "Roads go up to 160 m at a time";
        if (ground(c.end.x, c.end.z) < .6F) return "The road runs into the sea";
        if (std::abs(ground(c.end.x, c.end.z) - ground(c.position.x, c.position.z)) > length * .25F) return "Too steep for a road";
        return "";
    }
    if (c.piece == Piece::BusStop) {
        for (const auto& town : region.settlements()) for (const auto& asset : town.assets)
            if (asset.model_path.ends_with("transit/bus_stop.glb") || asset.model_path.ends_with("transit/bus_stop_alt.glb"))
                if (flat(asset.position, c.position) < 60) return "Another bus stop is within 60 m";
        if (WorldCollision::body_blocked({c.position.x, c.position.y, c.position.z}, c.position.y, region, .5F)) return "Something is in the way";
        return "";
    }
    if (c.home()) {
        float low = std::numeric_limits<float>::max(), high = -low;
        for (const float x : {-.5F, 0.0F, .5F}) for (const float z : {-.5F, 0.0F, .5F}) {
            const Vector3 p = local(c.position, c.yaw, x * (info.size.x + 1), z * (info.size.z + 1));
            const float y = ground(p.x, p.z);
            low = std::min(low, y); high = std::max(high, y);
            if (y < .6F) return "Too close to the sea";
            if (WorldCollision::body_blocked({p.x, y, p.z}, y, region, .45F)) return "Something is in the way";
        }
        if (high - low > 1.6F) return "The ground is too steep here";
        if (road_gap(region, c.position, c.yaw, info.size) < 1.5F) return "Too close to the road";
        return "";
    }
    const float y = ground(c.position.x, c.position.z);
    if (WorldCollision::body_blocked({c.position.x, y, c.position.z}, y, region, .4F)) return "Something is in the way";
    return "";
}

Placed& PlayerBuilds::add(Placed piece) {
    piece.serial = next_serial_++;
    placed_.push_back(piece);
    return placed_.back();
}

PlayerBuilds::Removal PlayerBuilds::remove(Vector3 near, float radius) {
    for (std::size_t i = placed_.size(); i-- > 0;) {
        const auto& p = placed_[i];
        const float d = p.piece == Piece::Road ? segment_distance(near, p.position, p.end) : flat(near, p.position);
        if (radius > 0 && d > radius + piece_info(p.piece).size.x * .5F) continue;
        if (p.residents > 0) return Removal::Occupied;
        placed_.erase(placed_.begin() + static_cast<std::ptrdiff_t>(i));
        return Removal::Removed;
    }
    return Removal::Nothing;
}

world::Building PlayerBuilds::building(const Placed& home) const {
    static constexpr Color walls[]{{236, 232, 220, 255}, {226, 214, 170, 255}, {176, 198, 214, 255}, {184, 196, 160, 255}, {232, 206, 196, 255}};
    static constexpr Color roofs[]{{72, 74, 78, 255}, {104, 78, 60, 255}, {128, 66, 52, 255}, {86, 96, 104, 255}};
    world::Building b;
    b.id = home.id();
    b.style = home.piece == Piece::House ? world::BuildingStyle::TwoStoryHouse : world::BuildingStyle::RuralHouse;
    b.position = {home.position.x, 0, home.position.z};
    b.size = piece_info(home.piece).size;
    b.rotation_y = home.yaw;
    b.wall_color = walls[static_cast<std::size_t>(home.serial) % 5];
    b.roof_color = roofs[static_cast<std::size_t>(home.serial / 5) % 4];
    b.enterable = true;
    b.purpose = world::BuildingPurpose::Home;
    return b;
}

void PlayerBuilds::apply(world::VerdaRegion& region) {
    auto& towns = region.runtime_settlements();
    if (towns.empty()) return;
    const auto mine = [](const std::string& id) { return id.starts_with("player_"); };
    for (auto& town : towns) {
        std::erase_if(town.buildings, [&](const world::Building& b) { return mine(b.id); });
        std::erase_if(town.assets, [&](const world::WorldAsset& a) { return mine(a.id); });
        std::erase_if(town.roads, [&](const world::Road& road) {
            for (const auto& old : applied_roads_)
                if (Vector3Distance(old.start, road.start) < .01F && Vector3Distance(old.end, road.end) < .01F) return true;
            return false;
        });
    }
    applied_roads_.clear();
    for (const auto& p : placed_) {
        auto& town = towns[nearest_town(towns, p.position)];
        switch (p.piece) {
            case Piece::Road: {
                world::Road road;
                road.start = {p.position.x, .02F, p.position.z};
                road.end = {p.end.x, .02F, p.end.z};
                road.width = piece_info(Piece::Road).size.x;
                road.type = world::RoadType::Asphalt;
                town.roads.push_back(road);
                applied_roads_.push_back({road.start, road.end});
                break;
            }
            case Piece::Cottage: case Piece::House:
                town.buildings.push_back(building(p));
                break;
            default: {
                world::WorldAsset asset;
                asset.id = p.id();
                asset.position = {p.position.x, ground(p.position.x, p.position.z), p.position.z};
                asset.rotation_y = p.yaw;
                asset.size = piece_info(p.piece).size;
                asset.collision = true;
                if (p.piece == Piece::Tree) {asset.type = world::AssetType::Tree; asset.position.y = 0;}
                else {
                    asset.type = world::AssetType::Sign;
                    asset.model_path = p.piece == Piece::BusStop ? bus_stop_model : lamp_model;
                    if (p.piece == Piece::Lamp) asset.size = {1.05F, 10.52F, 3.47F};
                }
                town.assets.push_back(std::move(asset));
            }
        }
    }
}

int PlayerBuilds::vacant_home_near(Vector3 stop, float radius) const {
    int best = -1; float distance = radius;
    for (std::size_t i = 0; i < placed_.size(); ++i)
        if (placed_[i].home() && placed_[i].residents == 0)
            if (const float d = flat(placed_[i].position, stop); d < distance) { distance = d; best = static_cast<int>(i); }
    return best;
}

Placed* PlayerBuilds::find(const std::string& id) {
    for (auto& p : placed_) if (p.id() == id) return &p;
    return nullptr;
}

int PlayerBuilds::empty_homes() const {
    int n = 0;
    for (const auto& p : placed_) n += p.home() && p.residents == 0;
    return n;
}

bool PlayerBuilds::save(const std::string& path, std::string& error) const {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    const std::string temp = path + ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        if (!out) { error = "cannot write " + temp; return false; }
        out << "OUTLAND_BUILDS 1\n";
        for (const auto& p : placed_)
            out << static_cast<int>(p.piece) << ' ' << p.serial << ' ' << p.position.x << ' ' << p.position.y << ' ' << p.position.z << ' '
                << p.end.x << ' ' << p.end.y << ' ' << p.end.z << ' ' << p.yaw << ' ' << p.residents << '\n';
        if (!out) { error = "write failed"; return false; }
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) { error = ec.message(); return false; }
    return true;
}

bool PlayerBuilds::load(const std::string& path, std::string& error) {
    std::ifstream in(path);
    if (!in) { clear(); return true; } // nothing built yet
    std::string header;
    std::getline(in, header);
    if (header != "OUTLAND_BUILDS 1") { error = "not a builds file: " + path; return false; }
    std::vector<Placed> loaded;
    int serial = 1;
    for (std::string line; std::getline(in, line);) {
        if (line.empty()) continue;
        std::istringstream row(line);
        int piece = 0;
        Placed p;
        row >> piece >> p.serial >> p.position.x >> p.position.y >> p.position.z >> p.end.x >> p.end.y >> p.end.z >> p.yaw >> p.residents;
        if (!row || piece < 0 || piece >= piece_count || !std::isfinite(p.position.x) || !std::isfinite(p.position.z)) {
            error = "bad line in " + path; return false;
        }
        p.piece = static_cast<Piece>(piece);
        p.residents = std::clamp(p.residents, 0, piece_info(p.piece).beds);
        serial = std::max(serial, p.serial + 1);
        loaded.push_back(p);
    }
    placed_ = std::move(loaded);
    next_serial_ = serial;
    return true;
}

std::string PlayerBuilds::path_beside(const std::string& loot_profile) {
    return (std::filesystem::path(loot_profile).parent_path() / "explore.builds").string();
}

} // namespace outland::game::build
