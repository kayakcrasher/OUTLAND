#include "outland/game/culture/Faith.hpp"
#include "outland/game/economy/Penthouse.hpp"
#include "outland/creator/CreatorAssetRegistry.hpp"
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/dev/VerdaWorldKit.hpp"
#include "outland/game/combat/CombatWorld.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/game/life/LifeSimulation.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/Ballparks.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Highways.hpp"
#include "outland/world/roads/Turnaround.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <set>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
using namespace outland;
using world::physics::WorldCollision;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::size_t count_assets(const world::VerdaRegion& region) {
    std::size_t n = 0;
    for (const auto& s : region.settlements()) n += s.assets.size();
    return n;
}
Vector3 world_of(const dev::KitBuilding& b, float x, float z) {
    const float a = b.yaw * DEG2RAD;
    return {b.origin.x + x * std::cos(a) + z * std::sin(a), b.origin.y, b.origin.z - x * std::sin(a) + z * std::cos(a)};
}
// Walk a body in small steps toward a building-local point; returns the final world position.
Vector3 walk(const world::VerdaRegion& region, const dev::KitBuilding& b, Vector3 position, float x, float z) {
    const auto target = world_of(b, x, z);
    for (int i = 0; i < 400; ++i) {
        const float dx = target.x - position.x, dz = target.z - position.z, length = std::sqrt(dx * dx + dz * dz);
        if (length < .05F) break;
        const float step = std::min(.05F, length);
        const Vector3 next{position.x + dx / length * step, position.y, position.z + dz / length * step};
        position = WorldCollision::resolve_body_movement(position, next, position.y, region, .45F);
        position.y = WorldCollision::ground_height(position, position.y, region);
    }
    return position;
}
bool near(Vector3 p, Vector3 q, float tolerance) { return std::hypot(p.x - q.x, p.z - q.z) < tolerance; }
}
int main() {
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    const creator::CreatorAssetRegistry catalog;
    const auto make = [&](world::VerdaRegion& region) {
        check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_loot_defaults.map"), "load defaults");
        return dev::build_verda_towns(region, catalog);
    };
    world::VerdaRegion region(true);
    const auto before = count_assets(region);
    const auto report = make(region);
    std::cout << report.buildings << " buildings, " << report.assets << " pieces, " << report.skipped_lots << " skipped lots\n";
    for (const auto& p : report.skipped) std::cout << "  skipped lot " << p.x << ", " << p.z << '\n';
    check(report.missing.empty(), "every kit model is in the catalog");
    check(report.buildings >= 40 && report.assets > 400, "towns dressed: " + std::to_string(report.buildings));
    // A church in every town: Baptist chapels in the west, Orthodox churches in the east, both in Verda City.
    check(report.baptist_churches >= 3 && report.orthodox_churches >= 4,
          "churches: " + std::to_string(report.baptist_churches) + " Baptist, " + std::to_string(report.orthodox_churches) + " Orthodox");
    {
        const auto found = outland::game::culture::find_churches(region);
        bool west_baptist = true, capital_both[2]{false, false}, green_hand = false;
        for (const auto& c : found) {
            if (c.settlement == "capital_verda") capital_both[static_cast<int>(c.denomination)] = true;
            else west_baptist &= (c.centre.x < 0) == (c.denomination == outland::game::culture::Denomination::Baptist);
            green_hand |= c.green_hand;
        }
        check(found.size() == static_cast<std::size_t>(report.baptist_churches + report.orthodox_churches) && west_baptist && capital_both[0] && capital_both[1] && green_hand,
              "Baptists west, Orthodox east, Verda City both (La Verda Mano's chapel there)");
    }
    check(count_assets(region) + static_cast<std::size_t>(report.cleared) == before + static_cast<std::size_t>(report.assets), "every piece is a world asset");
    world::VerdaRegion again(true);
    const auto repeat = make(again);
    check(repeat.assets == report.assets && count_assets(again) == count_assets(region), "deterministic");

    // Save/load round trip through the normal Creator map format.
    const auto path = std::filesystem::temp_directory_path() / "outland_world_kit_test.map";
    check(creator::CreatorMapIO::save(region, path.string()), "save");
    world::VerdaRegion loaded(false);
    check(creator::CreatorMapIO::load(loaded, path.string()) && count_assets(loaded) == count_assets(region), "map round trip");

    // Every building can be entered through its doorway, and the doorway is not a wall.
    game::combat::CombatWorld combat(region);
    int two_storey = 0;
    for (const auto& b : report.enterable) {
        // 1.2 m out: on downtown sidewalks the street trees stand 2.4 m from the shopfronts.
        auto start = world_of(b, b.door_x, -b.depth * .5F - 1.2F);
        start.y = WorldCollision::ground_height(start, b.origin.y, region);
        const auto inside = walk(region, b, start, b.door_x, -b.depth * .5F + 2.5F);
        check(near(inside, world_of(b, b.door_x, -b.depth * .5F + 2.5F), .2F),
              "walk in through the front door at " + std::to_string(b.origin.x) + ", " + std::to_string(b.origin.z));
        check(std::abs(inside.y - (b.origin.y + b.ground_floor)) < .05F, "stand on the ground-floor slab at " + std::to_string(b.origin.x) + ", " + std::to_string(b.origin.z));
        // Bullets fly in through the doorway at chest height.
        const auto shot = combat.trace_segment(Vector3Add(world_of(b, b.door_x, -b.depth * .5F - 3), {0, 1.4F, 0}),
                                               Vector3Add(world_of(b, b.door_x, -b.depth * .5F + 1), {0, 1.4F, 0}), false, false, false);
        check(!shot.hit() || shot.kind == game::combat::HitKind::Ground, "doorway is open to bullets");
        if (b.storeys < 2) continue;
        ++two_storey;
        // Upstairs: walk up the flight and step off onto the upper floor, then across it.
        auto foot = world_of(b, b.stairs_start.x, b.stairs_start.z);
        foot.y = b.origin.y + b.ground_floor;
        const auto top = Vector3Add(b.stairs_start, Vector3Scale(b.stairs_direction, b.stairs_length));
        foot = walk(region, b, foot, top.x, top.z);
        check(std::abs(foot.y - (b.origin.y + b.upper_floor)) < .05F, "climb to the upper floor: " + std::to_string(foot.y - b.origin.y));
        foot = walk(region, b, foot, top.x + b.upstairs_walk.x, top.z + b.upstairs_walk.z);
        check(std::abs(foot.y - (b.origin.y + b.upper_floor)) < .05F, "walk around upstairs");
    }
    check(two_storey >= 30, "many multi-storey buildings: " + std::to_string(two_storey));

    // The one-storey civic buildings with a front door - the police stations, the Ŝosea Patrolo
    // outposts, the radio stations, the gun shops - walked into from the street through it (no
    // plinth step at the door too high to climb).
    {
        struct Door { const char* model; float x, d; };
        const Door doors[]{{"civic/police_station.glb", 0, 6}, {"civic/patrol_outpost.glb", -2.8F, 3},
                           {"civic/radio_station_", -3.9F, 4.5F}, {"civic/gun_shop.glb", 0, 4.5F}};
        int buildings = 0, entered = 0;
        for (const auto& settlement : region.settlements()) for (const auto& asset : settlement.assets)
            for (const auto& door : doors) {
                if (asset.model_path.find(door.model) == std::string::npos) continue;
                const auto* mesh = world::physics::MeshCollisionLibrary::get(asset.model_path);
                if (!mesh) continue;
                ++buildings;
                // The kit stores a city model at its bounds' centre: back to the design origin.
                const float cx = (mesh->source_min.x + mesh->source_max.x) * .5F, cz = (mesh->source_min.z + mesh->source_max.z) * .5F;
                const float a = asset.rotation_y * DEG2RAD;
                const auto local = [&](float x, float z) {
                    x -= cx; z -= cz;
                    return Vector3{asset.position.x + x * std::cos(a) + z * std::sin(a), asset.position.y, asset.position.z - x * std::sin(a) + z * std::cos(a)};
                };
                Vector3 p = local(door.x, -door.d - 3);
                p.y = WorldCollision::ground_height(p, p.y + 1, region);
                const Vector3 inside = local(door.x, -door.d + 1.2F);
                for (int i = 0; i < 400; ++i) {
                    const float dx = inside.x - p.x, dz = inside.z - p.z, l = std::sqrt(dx * dx + dz * dz);
                    if (l < .05F) break;
                    const float st = std::min(.05F, l);
                    p = WorldCollision::resolve_body_movement(p, {p.x + dx / l * st, p.y, p.z + dz / l * st}, p.y, region, .35F);
                    p.y = WorldCollision::ground_height(p, p.y, region);
                }
                if (std::hypot(p.x - inside.x, p.z - inside.z) < .25F) ++entered;
                else std::cerr << "  could not walk into " << asset.model_path << " at " << asset.position.x << "," << asset.position.z << ": stopped " << std::hypot(p.x - inside.x, p.z - inside.z) << " m short, at " << p.x << "," << p.y << "," << p.z << '\n';
            }
        check(buildings >= 12, "civic buildings with a front door: " + std::to_string(buildings));
        check(entered == buildings, "walk in at every one's front door: " + std::to_string(entered) + "/" + std::to_string(buildings));
    }

    // Sovaĝejo's trailer park (twelve mobile homes), Ruldomaro's (eight) and the three highway
    // hamlets' (six each): every one walkable from the lane through its door.
    {
        check(report.parks == 1, "trailer park built");
        int trailers = 0, entered = 0;
        for (const auto& settlement : region.settlements()) for (const auto& asset : settlement.assets) {
            const auto at = asset.model_path.find("residential/trailers/trailer_0");
            if (at == std::string::npos) continue;
            ++trailers;
            const int model = asset.model_path[at + 30] - '0';
            const float a = asset.rotation_y * DEG2RAD;
            const auto local = [&](float x, float z) {
                return Vector3{asset.position.x + x * std::cos(a) + z * std::sin(a), asset.position.y, asset.position.z - x * std::sin(a) + z * std::cos(a)};
            };
            const auto step_to = [&](Vector3 p, Vector3 target) {
                for (int i = 0; i < 600; ++i) {
                    const float dx = target.x - p.x, dz = target.z - p.z, l = std::sqrt(dx * dx + dz * dz);
                    if (l < .05F) break;
                    const float st = std::min(.05F, l);
                    p = WorldCollision::resolve_body_movement(p, {p.x + dx / l * st, p.y, p.z + dz / l * st}, p.y, region, .35F);
                    p.y = WorldCollision::ground_height(p, p.y, region);
                }
                return p;
            };
            // From the lane end of the lot: up the side steps (models 1-4) or the front steps (5-6).
            auto p = local(model <= 4 ? -1.5F : -4.0F, 8.0F);
            p.y = WorldCollision::ground_height(p, asset.position.y + .5F, region);
            if (model <= 4) p = step_to(step_to(p, local(-1.5F, 1.6F)), local(-1.5F, .56F));
            else p = step_to(p, local(-4.0F, .56F));
            p = step_to(p, local(.6F, .56F));
            const auto inside = local(.6F, .56F);
            if (std::hypot(p.x - inside.x, p.z - inside.z) < .2F && p.y > asset.position.y + .6F) ++entered;
            else std::cerr << "  could not enter trailer " << model << " at " << asset.position.x << "," << asset.position.z << '\n';
        }
        check(trailers == 38, "thirty-eight trailers: " + std::to_string(trailers));
        check(entered == trailers, "walk into every trailer: " + std::to_string(entered) + "/" + std::to_string(trailers));
    }

    // The hamlets: Ruldomaro off the eastern ring road (eight trailers and a bus stop) and three
    // highway stops of six - each on a level pad, with a gravel lane and a gravel spur to the
    // highway, a general store, loot of every kind, and people.
    {
        check(report.hamlets == 4, "four hamlets built: " + std::to_string(report.hamlets));
        for (const auto& site : world::layout::hamlets) {
            const world::Settlement* hamlet = nullptr;
            for (const auto& s : region.settlements()) if (s.id == site.id) hamlet = &s;
            const std::string name(site.name);
            check(hamlet && hamlet->name == name && hamlet->survivors >= 14, name + " is a settlement with people");
            int gravel = 0;
            for (const auto& road : hamlet->roads) gravel += road.type == world::RoadType::Gravel;
            check(gravel == 2, name + ": a gravel lane and a gravel spur to the highway");
            std::set<std::string> tables;
            for (const auto& m : hamlet->gameplay_markers)
                if (m.type == world::GameplayMarkerType::LootSpawn) tables.insert(m.id.substr(11, m.id.find('_', 11) - 11));
            check(tables.count("camp") && tables.count("general") && tables.count("weapons") && tables.count("medical"), name + ": loot of every kind");
            int homes = 0;
            for (const auto& s : region.settlements()) for (const auto& m : s.gameplay_markers)
                homes += m.type == world::GameplayMarkerType::BuildingPurpose && m.purpose == world::BuildingPurpose::Home &&
                         std::hypot(m.position.x - hamlet->center.x, m.position.z - hamlet->center.z) < 70;
            check(homes == (site.id == "hamlet_ruldomaro" ? 8 : 6), name + " homes: " + std::to_string(homes));
            const float h = world::terrain::TerrainHeight::sample(hamlet->center.x, hamlet->center.z);
            for (const float o : {-60.0F, 60.0F}) check(std::abs(world::terrain::TerrainHeight::sample(hamlet->center.x + o, hamlet->center.z + o * .4F) - h) < .05F, name + " on a level pad");
            // Off a highway: the spur ends on one of the capital's roads.
            const auto& spur = hamlet->roads.back();
            bool joins = false;
            for (const auto& s : region.settlements()) if (s.id == "capital_verda") for (const auto& road : s.roads) {
                const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
                if (l2 < 1) continue;
                const float t = std::clamp(((spur.end.x - road.start.x) * dx + (spur.end.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
                joins |= std::hypot(spur.end.x - road.start.x - dx * t, spur.end.z - road.start.z - dz * t) < .5F;
            }
            check(joins, name + " joins a highway");
        }
    }

    // Roka is an industrial city: a 4 x 4 street grid with the highways ending at its east edge,
    // works west of it, worker flats, and on a wide level pad. Sovaĝejo is north of it at the end
    // of a dirt road.
    {
        check(report.cities == 3, "Roka, Espera and Suda Haveno built as cities");
        const world::Settlement* roka = nullptr;
        const world::Settlement* sovagejo = nullptr;
        for (const auto& s : region.settlements()) { if (s.id == "west_roka") roka = &s; if (s.id == "ferarbaro_sovagejo") sovagejo = &s; }
        check(roka && sovagejo, "Roka and Sovaĝejo");
        int grid = 0;
        // (In town: the freight yard out on the highway is not the grid.)
        for (const auto& road : roka->roads)
            grid += road.type == world::RoadType::Asphalt && road.width >= 12 &&
                    std::hypot(road.start.x - roka->center.x, road.start.z - roka->center.z) < 200 && std::hypot(road.end.x - roka->center.x, road.end.z - roka->center.z) < 200;
        check(grid == 8, "Roka's street grid: " + std::to_string(grid));
        for (const auto& s : region.settlements()) for (const auto& road : s.roads)
            for (const auto& end : {road.start, road.end}) check(std::hypot(end.x - roka->center.x, end.z - roka->center.z) > 1, "no road ends in the middle of the square");
        int works = 0, flats = 0, hangars = 0, sheds = 0;
        for (const auto& s : region.settlements()) for (const auto& asset : s.assets) {
            if (std::hypot(asset.position.x - roka->center.x, asset.position.z - roka->center.z) > 230) continue;
            flats += asset.model_path.find("Buildings Prebuild/flat_") != std::string::npos;
            hangars += asset.model_path.find("Hangar1") != std::string::npos;
            sheds += asset.model_path.find("Large Shed") != std::string::npos;
        }
        for (const auto& b : report.enterable) works += std::hypot(b.origin.x - roka->center.x + 165, b.origin.z - roka->center.z) < 70;
        check(flats == 8 && hangars >= 3 && sheds == 0 && works == 2, "flats, hangars (no white sheds) and factory halls: " + std::to_string(flats) + "/" + std::to_string(hangars) + "/" + std::to_string(sheds) + "/" + std::to_string(works));
        const float h = world::terrain::TerrainHeight::sample(roka->center.x, roka->center.z);
        for (const auto& [x, z] : std::initializer_list<std::pair<float, float>>{{-180, 0}, {120, 120}, {-120, -120}, {180, -60}})
            check(std::abs(world::terrain::TerrainHeight::sample(roka->center.x + x, roka->center.z + z) - h) < .05F, "Roka on a level pad");
        check(sovagejo->center.z < roka->center.z - 300 && std::abs(sovagejo->center.x - roka->center.x) < 80, "Sovaĝejo is north of Roka");
        for (const auto& road : sovagejo->roads) check(road.type == world::RoadType::Dirt, "a dirt road out to Sovaĝejo");
        world::roads::RoadGraph graph(region);
        check(graph.route(sovagejo->center, roka->center).complete, "the dirt road joins Roka's streets");
        for (const auto& s : region.settlements()) for (const auto& asset : s.assets)
            if (asset.model_path.find("transit/bus_stop") != std::string::npos)
                check(std::hypot(asset.position.x - sovagejo->center.x, asset.position.z - sovagejo->center.z) > 300, "no bus stop at Sovaĝejo");
    }

    // Espera and Suda Haveno: the same city plan - a 4 x 4 grid round a square, Main Street rows,
    // flats, a car park and a yard (the co-op's barn, the fish market) - on level pads, with the
    // highway from the capital joining the grid's edge street.
    {
        world::roads::RoadGraph graph(region);
        const world::Settlement* capital = nullptr;
        for (const auto& s : region.settlements()) if (s.id == "capital_verda") capital = &s;
        for (const char* id : {"village_espera", "south_haven"}) {
            const world::Settlement* town = nullptr;
            for (const auto& s : region.settlements()) if (s.id == id) town = &s;
            check(town != nullptr && capital != nullptr, std::string(id) + " is there");
            if (!town || !capital) continue;
            int grid = 0;
            for (const auto& road : town->roads) grid += road.type == world::RoadType::Asphalt && road.width == 12;
            check(grid == 8, town->name + "'s street grid: " + std::to_string(grid));
            int rows = 0, flats = 0, yard = 0, square = 0;
            for (const auto& s : region.settlements()) for (const auto& asset : s.assets) {
                if (std::hypot(asset.position.x - town->center.x, asset.position.z - town->center.z) > 200) continue;
                rows += asset.model_path.find("/main_street/") != std::string::npos;
                flats += asset.model_path.find("Buildings Prebuild/flat_") != std::string::npos;
                yard += asset.model_path.find("ranch/barn") != std::string::npos || asset.model_path.find("Pallet.glb") != std::string::npos;
                check(asset.model_path.find("Large Shed") == std::string::npos, "no white sheds");
                square += asset.model_path.find("courthouse_square") != std::string::npos;
            }
            check(rows >= 30 && flats == 4 && yard >= 1 && square == 1, town->name + ": rows, flats, the yard and the square: " +
                  std::to_string(rows) + "/" + std::to_string(flats) + "/" + std::to_string(yard) + "/" + std::to_string(square));
            const float h = world::terrain::TerrainHeight::sample(town->center.x, town->center.z);
            for (const auto& [x, z] : std::initializer_list<std::pair<float, float>>{{-150, 0}, {130, 130}, {-130, -130}, {150, -60}})
                check(std::abs(world::terrain::TerrainHeight::sample(town->center.x + x, town->center.z + z) - h) < .05F, town->name + " on a level pad");
            const auto route = graph.route(capital->center, town->center);
            check(route.complete && route.length < 2600, "the highway runs into " + town->name + "'s streets: " + std::to_string(route.length));
        }
    }

    // Downtown: the old capital is gone, the grid is in, highways meet the grid edge.
    const world::Settlement* capital = nullptr;
    for (const auto& s : region.settlements()) if (s.id == "capital_verda") capital = &s;
    // The old capital's houses are gone; what the capital owns now are the western suburbs' homes.
    check(capital != nullptr, "capital");
    int suburb_homes = 0;
    for (const auto& house : capital->buildings) {
        check(house.position.x < -170 && house.position.x > -476 && std::abs(house.position.z) < 190, "old capital houses wiped");
        check(house.purpose == world::BuildingPurpose::Home && house.enterable, "suburban homes are enterable homes");
        // Every home fronts a street: 14 m in front of its centre is a street centreline (within 2 m;
        // 3 m further back on the truckers' 14 m road).
        const float a = house.rotation_y * DEG2RAD;
        const Vector3 front{house.position.x - std::sin(a) * 14, 0, house.position.z - std::cos(a) * 14};
        bool fronts = false;
        for (const auto& road : capital->roads) {
            const auto ab = Vector3Subtract(road.end, road.start), ap = Vector3Subtract(front, road.start);
            const float t = std::clamp((ab.x * ap.x + ab.z * ap.z) / (ab.x * ab.x + ab.z * ab.z), 0.0F, 1.0F);
            fronts |= std::hypot(road.start.x + ab.x * t - front.x, road.start.z + ab.z * t - front.z) < (road.width > 13 ? 3.5F : 2.0F);
        }
        check(fronts, "home faces its street");
        // Level ground: the suburbs are on the capital's building pad.
        check(std::abs(world::terrain::TerrainHeight::sample(house.position.x, house.position.z)) < .05F, "suburbs are level");
        ++suburb_homes;
    }
    check(suburb_homes >= 80, "western suburbs: " + std::to_string(suburb_homes) + " homes");
    int grid_streets = 0, highways = 0, through = 0, suburb_streets = 0, freight_roads = 0;
    for (const auto& road : capital->roads) {
        const auto in_suburbs = [](Vector3 p) { return std::abs(p.x) >= 179 && std::abs(p.x) <= 476 && std::abs(p.z) <= 168; };
        // The grid, and its south blocks (avenues on to a street along their far side).
        const auto grid = [](Vector3 p) { return std::abs(p.x) <= 180 && p.z >= -177 && p.z <= 261; };
        const bool inside = grid(road.start) && grid(road.end);
        // The ring interchanges' roundabouts and slip roads (tests/integration/world/test_interchanges).
        if (world::one_way(road.type)) continue;
        if (road.type == world::RoadType::Highway) {
            check(road.width == world::highways::width || road.width == world::highways::boulevard_width, "highways are divided highways (city boulevards narrower)");
            // The east-west highway runs right across the city on z = 0 (Roka to Porto Luma) as a
            // boulevard; the north-south highway runs straight through downtown down x = 0.
            if (std::abs(road.start.z) < .01F && std::abs(road.end.z) < .01F && road.width == world::highways::boulevard_width) ++highways;
            if (std::abs(road.start.x) < .5F && std::abs(road.end.x) < .5F && std::abs(road.start.z) < 176 && std::abs(road.end.z) < 176) ++through;
        } else if (inside) { ++grid_streets; check(road.width == 16 && road.type == world::RoadType::Asphalt, "grid streets are 16 m asphalt"); }
        else if (in_suburbs(road.start) && in_suburbs(road.end)) ++suburb_streets;
        // Verda Transporto's yard road: the suburb street on z = -84 carried west into the industrial quarter.
        else if (road.start.z == -84 && road.end.z == -84 && road.start.x == -476 && road.end.x < world::layout::freight_x1) ++freight_roads;
        // Verda Transporto's freight yards off the spokes (a 16 m road in, a 14 m loop), far out of town.
        else if (std::hypot(road.start.x, road.start.z) > 1000 && (road.width == 16 || road.width == 14)) continue;
        // Verda City's ballpark: an 8 m road from its gate to the street.
        else if (road.width == 8 && std::hypot(road.start.x - world::ballpark::parks[0].centre.x, road.start.z - world::ballpark::parks[0].centre.z) < 90) continue;
        else check(world::roads::turnaround_road(road.width), "a capital road that is neither street, highway nor bus turnaround");
    }
    check(grid_streets == 8 + 5 && highways >= 12 && through >= 4 && suburb_streets == 14, // + the south blocks' four avenues and one street; the west street in three (the truckers' link)
          "grid, suburbs, and the two highways through: " + std::to_string(grid_streets) + "/" + std::to_string(suburb_streets) + "/" +
          std::to_string(highways) + "/" + std::to_string(through));
    check(freight_roads == 1, "the yard road into Verda Transporto's depot");

    // Armilejo: a gun shop in each of the five main cities, a Shop place at its door.
    {
        check(report.gun_shops.size() == 5, "five Armilejo gun shops: " + std::to_string(report.gun_shops.size()));
        int models = 0;
        for (const auto& town : region.settlements())
            for (const auto& a : town.assets) models += a.model_path.ends_with("civic/gun_shop.glb");
        check(models == 5, "five gun shop buildings");
    }

    // The radio stations: Verda Popolko in Verda City, Radio Sovaĝejo, Espera Roko - each studio
    // and mast in its own town, with its door an Office place.
    {
        check(report.radio_stations.size() == 3, "three radio stations built");
        const Vector3 towns[]{{0, 0, 0}, world::layout::sovagejo_center, {0, 0, -1800}};
        const char* styles[]{"popolko", "kantrio", "roko"};
        for (std::size_t i = 0; i < report.radio_stations.size() && i < 3; ++i) {
            const auto door = report.radio_stations[i];
            check(std::hypot(door.x - towns[i].x, door.z - towns[i].z) < 470, std::string("radio station in its town: ") + styles[i]);
            int models = 0;
            for (const auto& town : region.settlements())
                for (const auto& a : town.assets)
                    models += a.model_path.ends_with(std::string("civic/radio_station_") + styles[i] + ".glb") && std::hypot(a.position.x - door.x, a.position.z - door.z) < 12;
            check(models == 1, std::string("the studio and mast stand at the door: ") + styles[i]);
        }
    }

    // Porto Luma's cargo terminal and Verda Transporto's depot.
    {
        check(report.cargo_terminal && report.freight_depot, "the cargo terminal and the trucking depot are built");
        int ships = 0, cranes = 0, depots = 0, rigs = 0, stock = 0;
        Vector3 ship{};
        for (const auto& town : region.settlements()) {
            for (const auto& a : town.assets) {
                if (a.model_path.ends_with("civic/cargo_ship.glb")) { ++ships; ship = a.position; }
                cranes += a.model_path.ends_with("civic/harbour_crane.glb");
                depots += a.model_path.ends_with("civic/truck_depot.glb");
            }
            for (const auto& m : town.gameplay_markers) {
                rigs += m.id.find("vehicle_spawn_semi_depot_") != std::string::npos;
                stock += m.id.find("vehicle_spawn_semi_depot_cattle_") != std::string::npos;
            }
        }
        check(ships == 1 && cranes == 2 && depots == 1, "a ship at the berth, two cranes, the depot");
        check(rigs == 4 && stock == 1, "four company rigs for hire, one with the stock trailer");
        // The ship floats: deep water under her and along the quay face; the quay stands over the sea.
        const float under = world::terrain::TerrainHeight::sample(world::layout::cargo_quay_x1 + 9, world::layout::cargo_z);
        const float quay = world::terrain::TerrainHeight::sample(world::layout::cargo_quay_x1 - 20, world::layout::cargo_z);
        check(under < world::layout::sea_level - 8 && std::abs(quay - world::layout::cargo_quay_height) < .05F, "the berth is deep and the quay level");
        check(std::abs(ship.z - world::layout::cargo_z) < 30, "the ship lies along the quay");
        // The ramp down from the town is a drivable grade (under 16%).
        float worst = 0;
        for (float x = world::layout::cargo_ramp_x0; x < world::layout::cargo_ramp_x1; x += 5) {
            const float a = world::terrain::TerrainHeight::sample(x, world::layout::cargo_z), b = world::terrain::TerrainHeight::sample(x + 5, world::layout::cargo_z);
            worst = std::max(worst, std::abs(b - a) / 5);
        }
        check(worst < .16F, "the ramp's grade: " + std::to_string(worst));
        // The depot's yard is level with the suburbs.
        check(std::abs(world::terrain::TerrainHeight::sample(-570, -110)) < .05F, "the freight yard is level");
    }

    // East Verda: the fancy suburbs are the residential house model, on level ground, facing streets.
    {
        int fancy = 0;
        for (const auto& s : region.settlements()) for (const auto& a : s.assets) {
            if (a.model_path.find("/residential/houses/house_") == std::string::npos) continue;
            // (The county gangs' houses in the other towns are the same model; checked below.)
            bool gang = false;
            for (const auto& h : report.gang_houses) gang |= std::hypot(h.x - a.position.x, h.z - a.position.z) < .5F;
            if (gang) continue;
            ++fancy;
            check(a.position.x > 166 && a.position.x < 476 && std::abs(a.position.z) < 195, "fancy houses east of downtown");
            check(std::abs(a.position.y) < .05F, "east suburbs are level");
            check(a.collision, "houses are solid");
            const float yaw = a.rotation_y * DEG2RAD;
            const Vector3 front{a.position.x - std::sin(yaw) * 21, 0, a.position.z - std::cos(yaw) * 21};
            bool fronts = false;
            for (const auto& road : capital->roads) {
                const auto ab = Vector3Subtract(road.end, road.start), ap = Vector3Subtract(front, road.start);
                const float t = std::clamp((ab.x * ap.x + ab.z * ap.z) / (ab.x * ab.x + ab.z * ab.z), 0.0F, 1.0F);
                fronts |= std::hypot(road.start.x + ab.x * t - front.x, road.start.z + ab.z * t - front.z) < 2;
            }
            check(fronts, "fancy house faces its street");
        }
        check(fancy >= 60, "east suburbs: " + std::to_string(fancy) + " houses");
        // The county gangs' houses: one in each big town the Green Hand don't hold, its purpose
        // marker named so island life leaves it empty and La Mafio moves in.
        check(report.gang_houses.size() == 4, "a gang house in Espera, Porto Luma, Suda Haveno and Roka");
        for (const char* id : {"gang_house_village_espera", "gang_house_port_luma", "gang_house_south_haven", "gang_house_west_roka"}) {
            bool marked = false;
            for (const auto& s : region.settlements()) for (const auto& m : s.gameplay_markers) marked |= m.id == id;
            check(marked, std::string("marker ") + id);
        }
        // Upstairs in every fancy house: from the top of the stairs, through each bedroom door
        // and on into the back bedroom's bathroom.
        int upstairs = 0;
        for (const auto& b : report.enterable) {
            if (b.width != 13.6F) continue;
            const auto at = [&](Vector3 p) { return std::abs(p.y - (b.origin.y + b.upper_floor)) < .05F; };
            auto start = world_of(b, -2.85F, 3.4F);
            start.y = b.origin.y + b.upper_floor;
            const auto routes = std::initializer_list<std::initializer_list<std::pair<float, float>>>{
                {{-3.0F, 3.4F}, {-4.2F, 3.4F}, {-4.2F, -1.5F}, {-5.4F, -1.5F}},     // master bedroom
                {{-1.35F, 3.4F}, {-1.35F, -2.9F}, {1.0F, -2.9F}, {4.5F, -3.5F}},      // front bedroom
                {{-1.35F, 3.4F}, {-1.35F, .9F}, {1.0F, .9F}, {3.5F, 2.8F}, {3.5F, 4.6F}, {3.5F, 6.8F}}}; // back bedroom, bathroom
            bool all = true;
            for (const auto& route : routes) {
                auto p = start;
                for (const auto& [x, z] : route) p = walk(region, b, p, x, z);
                const auto& last = *(route.end() - 1);
                const bool ok = near(p, world_of(b, last.first, last.second), .2F) && at(p);
                if (!ok && upstairs == 0) {
                    const float a = b.yaw * DEG2RAD, dx = p.x - b.origin.x, dz = p.z - b.origin.z;
                    std::cerr << "  upstairs route " << (&route - routes.begin()) << " stuck at local " << dx * std::cos(a) - dz * std::sin(a)
                              << ", " << dx * std::sin(a) + dz * std::cos(a) << " height " << p.y - b.origin.y << '\n';
                }
                all &= ok;
            }
            upstairs += all;
        }
        const int houses = fancy + static_cast<int>(report.gang_houses.size());
        check(upstairs == houses, "every bedroom and bathroom upstairs: " + std::to_string(upstairs) + "/" + std::to_string(houses));
    }

    // Porto Luma: a 4 x 4 street grid round Placo Luma, Main Street rows and harbour towers.
    {
        const world::Settlement* port = nullptr;
        for (const auto& s : region.settlements()) if (s.id == "port_luma") port = &s;
        check(port && port->buildings.empty(), "Porto Luma rebuilt");
        int streets = 0;
        for (const auto& road : port->roads) streets += road.width == 12;
        check(streets == 8, "Porto Luma grid");
        int rows = 0, port_towers = 0;
        for (const auto& b : report.enterable) rows += std::hypot(b.origin.x - 1750, b.origin.z + 400) < 140 && b.width >= 12;
        for (const auto& s : region.settlements()) for (const auto& a : s.assets)
            port_towers += a.model_path.find("/city/towers/") != std::string::npos && std::hypot(a.position.x - 1750, a.position.z + 400) < 140;
        check(rows >= 30, "Porto Luma Main Street: " + std::to_string(rows));
        check(port_towers >= 4, "Porto Luma towers");
        for (const auto& road : capital->roads)
            for (const auto& end : {road.start, road.end})
                check(std::hypot(end.x - 1750, end.z + 400) > 100, "highways stop at the Porto Luma grid edge");
        const float h = world::terrain::TerrainHeight::sample(1750, -400);
        for (const float o : {-126.0F, 126.0F}) for (const float q : {-126.0F, 126.0F})
            check(std::abs(world::terrain::TerrainHeight::sample(1750 + o, -400 + q) - h) < .05F, "Porto Luma grid is level");
    }
    int towers = 0;
    // (Stela Turo is the enterable exception: its front door faces the avenue; checked below.)
    for (const auto& s : region.settlements()) for (const auto& a : s.assets) if (a.model_path.find("/city/towers/") != std::string::npos &&
                                                                                 !a.model_path.ends_with("stela_turo.glb")) {
        ++towers;
        // Towers are solid cover: walking at one from the street stops at its wall.
        Vector3 p{a.position.x + std::max(a.size.x, a.size.z) * .5F + 3, a.position.y, a.position.z};
        for (int i = 0; i < 400; ++i) {
            p = WorldCollision::resolve_body_movement(p, {p.x - .05F, p.y, p.z}, p.y, region, .45F);
            p.y = WorldCollision::ground_height(p, p.y, region);
        }
        check(p.x > a.position.x + std::min(a.size.x, a.size.z) * .5F - 1, "tower is solid");
    }
    check(towers >= 8, "tower core");

    // Every parked car downtown can be entered and driven along its street.
    game::vehicles::VehicleRegistry vehicle_types; std::string error;
    check(vehicle_types.load(std::string(OUTLAND_SOURCE_DIR) + "/assets/verda/vehicles/vehicle_manifest.tsv", error), error);
    game::vehicles::VehicleSystem cars(vehicle_types);
    cars.set_training_structure(false);
    cars.reconcile(region);
    int downtown_cars = 0;
    for (std::size_t i = 0; i < cars.vehicles().size(); ++i) {
        auto* car = cars.asset(region, cars.vehicles()[i].id);
        if (!car || (car->vehicle.marker.find("downtown") == std::string::npos && car->vehicle.marker.find("capital") == std::string::npos)) continue;
        ++downtown_cars;
        const Vector3 start = car->position;
        // From either side (one may be the kerb, by a tree or a lamp).
        Vector3 beside{};
        int found = -1;
        bool entered = false;
        for (const float side : {2.2F, -2.2F}) {
            beside = Vector3Add(start, Vector3RotateByAxisAngle({side, 0, 0}, {0, 1, 0}, car->rotation_y * DEG2RAD));
            found = cars.nearest(region, beside);
            if (found == static_cast<int>(i) && cars.enter(region, found, beside)) { entered = true; break; }
        }
        check(entered, "get into parked car " + car->vehicle.marker +
              " (nearest: " + (found >= 0 ? cars.vehicles()[static_cast<std::size_t>(found)].id : std::string("none")) + ")");
        for (int f = 0; f < 120; ++f) { cars.begin_frame(); cars.update(.02F, {1, 0}, region, car->position); }
        check(Vector3Distance(start, car->position) > 8, "drive away: " + car->vehicle.marker + " moved " + std::to_string(Vector3Distance(start, car->position)));
        // Brake to a stop (a muscle car gets going faster than a hatchback in the same time).
        for (int f = 0; f < 300 && std::abs(cars.vehicles()[i].speed) > .5F; ++f) cars.update(.02F, {0, 0, true}, region, car->position);
        Vector3 out{};
        check(cars.exit(region, out), "get out of " + car->vehicle.marker);
    }
    check(downtown_cars >= 12, "parked cars downtown: " + std::to_string(downtown_cars));

    // Explore: downtown is lived in - Main Street homes, shops and pubs, office workers in the towers.
    game::life::LifeSimulation life;
    life.build(region, nullptr);
    {   // Ruldomaro's store is a shop (the economy opens one at every Shop place).
        bool store = false;
        for (const auto& place : life.places())
            store |= place.kind == game::life::PlaceKind::Shop && std::hypot(place.door.x - 997, place.door.z + 997) < 70;
        check(store, "Ruldomaro has a store");
        for (const auto& site : world::layout::hamlets) {
            bool shop = false;
            for (const auto& place : life.places())
                shop |= place.kind == game::life::PlaceKind::Shop && std::hypot(place.door.x - site.center.x, place.door.z - site.center.z) < 70;
            check(shop, std::string(site.name) + " has a store");
        }
    }
    // Part-built buildings carry authored purposes (markers inside the front door), so island life
    // houses and employs people in them; the works and harbour offices are offices.
    {
        int kit_places = 0, offices = 0, docks = 0, staffed = 0;
        for (std::size_t i = 0; i < life.places().size(); ++i) {
            const auto& place = life.places()[i];
            // (The gangs' houses: kit places too, and nobody lives there - La Mafio moves in.)
            if (place.building_id.starts_with("gang_house_")) {
                ++kit_places;
                check(place.kind == game::life::PlaceKind::Home && place.home_capacity == 0, "a gang house is nobody's home: " + place.id);
                continue;
            }
            // (Verda Transporto's freight yards: named kit places, staffed like the rest.)
            if (!place.building_id.starts_with("creator_marker_building_purpose_kit_") && !place.building_id.starts_with("freight_yard_")) continue;
            ++kit_places;
            offices += place.kind == game::life::PlaceKind::Office;
            docks += place.kind == game::life::PlaceKind::Dock;
            bool used = false;
            for (const auto& r : life.residents()) if (r.work == static_cast<int>(i) || r.home == static_cast<int>(i)) {used = true; break;}
            staffed += used;
            if (!used) std::cerr << "  unused " << place.id << " kind " << game::life::place_name(place.kind) << " town " << place.settlement
                                 << " homes " << place.home_capacity << " jobs " << place.job_capacity << '\n';
        }
        check(report.purposes >= 10 && kit_places == report.purposes - report.retagged,
              "every part-built building is a place: " + std::to_string(kit_places) + "/" + std::to_string(report.purposes - report.retagged));
        // The south blocks' corner bar is a pub.
        bool bar = false;
        for (const auto& place : life.places())
            bar |= place.kind == game::life::PlaceKind::Pub && place.building_id.starts_with("creator_city_main_street_dark_12m_2st") &&
                   place.interior.z > 166 && place.interior.z < 240;
        check(report.south_blocks == 3 && bar, "Verda City's south blocks with their bar");
        check(offices >= 2 && docks >= 1, "authored offices and dock");
        // Government: Registaro de Verda in Verda City and an Urbodomo in each county's main
        // town, both offices with clerks; the stadium ground east of Orienta stays empty.
        check(report.parliament && report.town_halls == 4 && report.stadium_reserved,
              "parliament, " + std::to_string(report.town_halls) + " town halls, stadium ground");
        int seats = 0, clerks = 0;
        for (std::size_t i = 0; i < life.places().size(); ++i) {
            const auto& place = life.places()[i];
            if (place.kind != game::life::PlaceKind::Office) continue;
            bool government = false;
            for (const auto& town : region.settlements())
                for (const auto& asset : town.assets)
                    government |= (asset.model_path.ends_with("civic/parliament.glb") || asset.model_path.ends_with("civic/town_hall.glb")) &&
                                  std::hypot(asset.position.x - place.door.x, asset.position.z - place.door.z) < 30;
            if (!government) continue;
            ++seats;
            for (const auto& r : life.residents()) clerks += r.work == static_cast<int>(i);
        }
        check(seats == 5 && clerks >= 5, "seats of government with clerks: " + std::to_string(seats) + " / " + std::to_string(clerks));
        for (const auto& town : region.settlements())
            for (const auto& asset : town.assets)
                check(!(asset.collision && asset.model_path != world::ballpark::model(0) && asset.id.find("bus_team_0") == std::string::npos &&
                        std::abs(asset.position.x - dev::stadium_reserve.x) < dev::stadium_half &&
                        std::abs(asset.position.z - dev::stadium_reserve.z) < dev::stadium_half),
                      "nothing but Verda City's ballpark (and their bus) on the stadium ground: " + asset.id);
        check(staffed + static_cast<int>(report.gang_houses.size()) == kit_places, "people live or work in every part-built building: " + std::to_string(staffed));
        // Stela Turo: downtown, every floor walkable, both stair cores
        // climbable, the penthouse and the roof reachable.
        check(report.stela_turo, "Stela Turo built");
        game::economy::Penthouse penthouse;
        check(penthouse.locate(region), "Stela Turo found in the world");
        if (penthouse.found()) {
            const auto origin = penthouse.to_world({0, 0, 0});
            check(std::abs(origin.x) < 160 && std::abs(origin.z) < 160, "Stela Turo in downtown Verda City");
            const auto stand = [&](Vector3 local) {
                const auto w = penthouse.to_world(local);
                return world::physics::WorldCollision::ground_height({w.x, w.y + .5F, w.z}, w.y + .5F, region) - (w.y - local.y);
            };
            for (int floor = 0; floor < 17; ++floor) {
                const float top = floor * 3.6F + .25F;
                check(std::abs(stand({0, top, -6}) - top) < .05F, "floor " + std::to_string(floor + 1) + " walkable");
                const auto body = penthouse.to_world({0, top, -6});
                check(!world::physics::WorldCollision::body_blocked(body, body.y, region, .35F), "floor " + std::to_string(floor + 1) + " open");
            }
            // In off the pavement through each front door: a body walks from the street into the lobby.
            for (const float face : {-1.0F, 1.0F}) {
                Vector3 p = penthouse.to_world({0, 0, face * 16});
                p.y = world::physics::WorldCollision::ground_height({p.x, p.y + 1, p.z}, p.y + 1, region);
                const Vector3 lobby = penthouse.to_world({0, .25F, face * 6});
                for (int i = 0; i < 400 && Vector3Distance({p.x, 0, p.z}, {lobby.x, 0, lobby.z}) > .2F; ++i) {
                    const Vector3 step = Vector3Scale(Vector3Normalize(Vector3Subtract({lobby.x, 0, lobby.z}, {p.x, 0, p.z})), .05F);
                    p = world::physics::WorldCollision::resolve_body_movement(p, {p.x + step.x, p.y, p.z + step.z}, p.y, region, .45F);
                    p.y = world::physics::WorldCollision::ground_height({p.x, p.y + .6F, p.z}, p.y + .6F, region);
                }
                check(Vector3Distance({p.x, 0, p.z}, {lobby.x, 0, lobby.z}) < .5F, face < 0 ? "Stela Turo: in at the front door" : "Stela Turo: in at the back door");
            }
            // Up each flight: every step's top is where the stairs say, then the floor above.
            for (const char core : {'L', 'R'}) {
                const int flights = core == 'R' ? 17 : 16;
                for (int s = 0; s < flights; ++s) {
                    const float inner = 10.5F - .35F, lane = (s % 2) ? 1.3F : 0.0F;
                    const float x = core == 'R' ? inner - lane - .65F : -inner + lane + .65F;
                    const float base = s * 3.6F + .25F;
                    for (int k = 0; k < 17; ++k) {
                        const float z = s % 2 == 0 ? -6.5F + (k + .5F) * .28F : -6.5F + (17 - k + .5F) * .28F;
                        const float expect = base + (k + 1) * .2F;
                        check(std::abs(stand({x, expect, z}) - expect) < .06F, std::string("stair ") + core + " flight " + std::to_string(s) + " step " + std::to_string(k));
                    }
                }
            }
            check(std::abs(stand({0, 17 * 3.6F + .25F, -5}) - (17 * 3.6F + .25F)) < .05F, "the roof walkable");
            check(penthouse.inside(penthouse.to_world(game::economy::Penthouse::wake)) && !penthouse.inside(penthouse.to_world({0, 3.85F, 0})), "the penthouse floor");
            check(penthouse.near_bed(penthouse.to_world(game::economy::Penthouse::wake)), "the bed by where you wake");
        }
    }
    int capital_homes = 0, offices = 0, office_workers = 0;
    for (const auto& place : life.places())
        if (life.island().settlement_names[static_cast<std::size_t>(place.settlement)] == "Verda") {
            capital_homes += place.home_capacity > 0;
            offices += place.sealed;
        }
    for (const auto& r : life.residents())
        if (r.work >= 0 && life.places()[static_cast<std::size_t>(r.work)].sealed) ++office_workers;
    check(capital_homes >= 20 && offices >= 8 && office_workers >= 20,
          "downtown residents: homes " + std::to_string(capital_homes) + " offices " + std::to_string(offices) + " office workers " + std::to_string(office_workers));
    // Office workers stay abstract while inside a tower even with the player at the door.
    game::life::LifeConfig config; config.time_scale = 60; life.configure(config);
    characters::NpcSystem npcs;
    life.reset(npcs, game::life::WorldClock::at(0, 10, 0));
    for (int i = 0; i < 40; ++i) life.update(.25F, {0, 0, 9000}, npcs);
    for (const auto& r : life.residents()) {
        if (r.work < 0 || !life.places()[static_cast<std::size_t>(r.work)].sealed || r.place != r.work || !life.indoors(r)) continue;
        life.update(.3F, life.places()[static_cast<std::size_t>(r.work)].door, npcs);
        check(npcs.find_resident(r.id) < 0, "nobody materialises inside a solid tower");
        break;
    }
    std::cout << "world kit tests passed\n";
}
