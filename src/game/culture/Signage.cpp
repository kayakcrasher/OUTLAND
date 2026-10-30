#include "outland/game/culture/Signage.hpp"
#include <cctype>
#include "outland/world/Zones.hpp"
#include "outland/game/culture/Esperanto.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/world/Highways.hpp"
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
        if (place.kind == PlaceKind::Home || place.kind == PlaceKind::Square || place.kind == PlaceKind::Ballpark) continue;
        const auto s = static_cast<std::size_t>(std::max(0, place.settlement));
        const Vector3 centre = s < towns.size() ? towns[s].center : place.interior;
        const bool capital = s < towns.size() && towns[s].id.find("capital") != std::string::npos;
        // Stela Turo's offices carry the tower's own name: they do not take a place in the town's order.
        const auto in_tower = [&](const life::Place& p) {
            if (p.kind != PlaceKind::Office) return false;
            for (const auto& town : towns) for (const auto& asset : town.assets)
                if (asset.model_path.ends_with("towers/stela_turo.glb") && flat(asset.position, p.door) < 30) return true;
            return false;
        };
        int ordinal = 0;
        for (const auto& other : island.places)
            if (other.kind == place.kind && other.settlement == place.settlement && &other != &place && !in_tower(other) &&
                (flat(other.door, centre) < flat(place.door, centre) ||
                 (flat(other.door, centre) == flat(place.door, centre) && other.id < place.id))) ++ordinal;
        // The seats of government: Registaro de Verda (the parliament) and the county towns'
        // Urbodomoj. Where one stands, the town's other offices are ordinary businesses.
        const auto near_model = [&](std::string_view suffix) {
            for (const auto& town : towns) for (const auto& asset : town.assets)
                if (asset.model_path.ends_with(suffix) && flat(asset.position, place.door) < 30) return true;
            return false;
        };
        const bool parliament = place.kind == PlaceKind::Office && near_model("civic/parliament.glb");
        const bool town_hall = place.kind == PlaceKind::Office && !parliament && near_model("civic/town_hall.glb");
        bool seat_in_town = false;
        if (s < towns.size())
            for (const auto& asset : towns[s].assets)
                seat_in_town |= asset.model_path.ends_with("civic/parliament.glb") || asset.model_path.ends_with("civic/town_hall.glb");
        const bool seat = parliament || town_hall;
        const int sign_ordinal = place.kind == PlaceKind::Office && seat_in_town && !seat && ordinal == 0 ? 1 : ordinal;
        auto text = place_sign(place.kind, text_hash(s < towns.size() ? towns[s].id : place.id), owner_of(island, static_cast<int>(i)), sign_ordinal, capital);
        if (parliament) text = {"REGISTARO DE VERDA", "PARLAMENTO · RESPUBLIKO VERDA"};
        // Stela Turo, the art deco skyscraper by the stadium ground.
        if (!seat && in_tower(place)) text = {"STELA TURO", "OFICEJOJ · PENTDOMO"};
        if (town_hall) {
            std::string name = s < towns.size() ? towns[s].name : std::string{};
            for (auto& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            text = {"URBODOMO", name};
        }
        // The trucking company's office at its depot.
        if (place.kind == PlaceKind::Office && near_model("civic/truck_depot.glb")) text = {"VERDA TRANSPORTO", "KAMIONADO · ŜARĜOJ · BRUTARO"};
        // Armilejo, the gun shop chain.
        if (place.kind == PlaceKind::Shop && near_model("civic/gun_shop.glb")) text = {"ARMILEJO", "PAFILOJ · MUNICIO · KIRASOJ"};
        // The motor trade: Marnafto's kiosks, the workshops' offices, the car lot's sales office.
        if (place.kind == PlaceKind::Shop && near_model("civic/gas_station.glb")) text = {"MARNAFTO", "BENZINO · DIZELO · MANĜAĴOJ"};
        if (place.kind == PlaceKind::Garage && near_model("civic/mechanic_shop.glb")) text = {"MEĤANIKEJO", "RIPAROJ · MOTOROJ · PNEŬOJ · FARBO"};
        if (place.kind == PlaceKind::Office && near_model("civic/car_lot.glb")) text = {"AŬTOVENDEJO VERDA", "NOVAJ KAJ UZITAJ AŬTOJ"};
        // The ring's bar and grills: La Ringo (the truckers' brotherhood's home at Okcidento).
        if (place.kind == PlaceKind::Pub)
            for (const auto& rest : world::highways::rest_areas())
                if (flat(place.door, rest.building) < 20) text = {"LA RINGO", rest.brotherhood ? "TRINKEJO · KRADROSTEJO · LA FRATECO" : "TRINKEJO · KRADROSTEJO"};
        // The radio stations' studios.
        if (place.kind == PlaceKind::Office && near_model("civic/radio_station_popolko.glb")) text = {"VERDA POPOLKO 98.1", "RADIO · POPMUZIKO KAJ POLKO"};
        if (place.kind == PlaceKind::Office && near_model("civic/radio_station_kantrio.glb")) text = {"RADIO SOVAĜEJO 102.5", "KANTRIO · BLUGRASO"};
        if (place.kind == PlaceKind::Office && near_model("civic/radio_station_roko.glb")) text = {"ESPERA ROKO 105.9", "RADIO · ROKMUZIKO"};
        // The clinic in the hospital building is Hospitalo de Verda.
        if (place.kind == PlaceKind::Clinic)
            for (const auto& town : towns) for (const auto& asset : town.assets)
                if (asset.model_path.ends_with("civic/hospital.glb") && flat(asset.position, place.door) < 30)
                    text = {"HOSPITALO DE VERDA", "URĜA HELPO · AMBULANCOJ"};
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
        // Registaro de Verda's name runs across the portico's entablature (12-13.2 m up, its face
        // 10.5 m in front of the model's centre), over the columns.
        if (parliament)
            for (const auto& town : towns) for (const auto& asset : town.assets) {
                if (!asset.model_path.ends_with("civic/parliament.glb") || flat(asset.position, place.door) > 30) continue;
                const float a = asset.rotation_y * DEG2RAD;
                facing = {-std::sin(a), 0, -std::cos(a)};
                wall = {asset.position.x + facing.x * 10.5F, asset.position.y + 12.6F, asset.position.z + facing.z * 10.5F};
            }
        sign.kind = SignKind::Shopfront;
        sign.yaw = yaw_of(facing);
        sign.position = {wall.x + facing.x * .11F, wall.y, wall.z + facing.z * .11F};
        sign.max_width = place.sealed ? 12.0F : marker ? 5.0F : std::clamp(place.building_size.x * .85F, 1.6F, 9.0F);
        if (parliament) sign.max_width = 20;
        const float title = place.sealed ? .8F : parliament ? .9F : .5F;
        sign.front.push_back({text.title, title});
        if (!text.subtitle.empty() && !parliament) sign.front.push_back({text.subtitle, title * .45F});   // (the entablature takes one line)
        // Government buildings fly the flag from a staff beside the sign.
        // (The parliament and the town halls fly their own from poles in front.)
        if (place.kind == PlaceKind::Police || (place.kind == PlaceKind::Office && ordinal == 0 && !seat_in_town)) {
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

// Sovaĝejo's trailer park flies the Apelacio flag from a pole beside every trailer (and its town
// flag is one too); Ruldomaro, out east, has a couple. Trailers are found by where they stand,
// whichever settlement the map files them under.
void trailer_flags(const world::VerdaRegion& region, Signage& out) {
    const auto& towns = region.settlements();
    for (const auto& town : towns) {
        const bool sovagejo = town.id == "ferarbaro_sovagejo";
        if (sovagejo)
            for (auto& flag : out.flags) if (flag.place == town.id + ":square") flag.design = FlagDesign::Apelacio;
        if (!sovagejo && town.id != "hamlet_ruldomaro") continue;
        int trailer = 0;
        for (const auto& owner : towns) for (const auto& asset : owner.assets) {
            if (asset.model_path.find("/residential/trailers/") == std::string::npos || flat(asset.position, town.center) > 220) continue;
            if (trailer++ % (sovagejo ? 1 : 4) != 0) continue;
            const float reach = std::max(asset.size.x, asset.size.z) * .5F;
            Vector3 spot{};
            bool found = false;
            for (float radius = std::min(asset.size.x, asset.size.z) * .5F + .8F; radius < reach + 5 && !found; radius += .7F)
                for (int a = 0; a < 12 && !found; ++a) {
                    const float angle = (a + .25F * trailer) * 2 * PI / 12;
                    const Vector3 p{asset.position.x + std::cos(angle) * radius, asset.position.y, asset.position.z + std::sin(angle) * radius};
                    if (clear_spot(region, p, .5F)) {spot = p; found = true;}
                }
            if (!found) continue;
            const float base = ground(region, spot, spot.y);
            FlagPole flag;
            flag.place = asset.id;
            flag.base = {spot.x, base, spot.z};
            flag.tip = {spot.x, base + 5.0F, spot.z};
            flag.fly = Vector3Normalize(wind);
            flag.width = 1.6F;
            flag.design = FlagDesign::Apelacio;
            out.flags.push_back(flag);
        }
    }
}

void bus_stops(const world::VerdaRegion& region, Signage& out) {
    for (const auto& town : region.settlements()) for (const auto& asset : town.assets) {
        // The island line's stops (Transit kit shelters) carry their town's name.
        const bool line = asset.model_path.ends_with("transit/bus_stop.glb") || asset.model_path.ends_with("transit/bus_stop_alt.glb");
        if (asset.model_path.find("Bus stops/") == std::string::npos && !line) continue;
        Sign sign;
        sign.kind = SignKind::BusStop;
        sign.place = asset.id;
        sign.front = {{"BUSHALTEJO", .26F}, {"LINIO " + std::to_string(1 + text_hash(asset.id) % 9), .18F}};
        if (line) {
            const world::Settlement* nearest = nullptr;
            for (const auto& other : region.settlements())
                if (!nearest || flat(other.center, asset.position) < flat(nearest->center, asset.position)) nearest = &other;
            sign.front = {{"BUSHALTEJO", .26F}, {upper(nearest ? nearest->name : std::string("Verda")), .22F}, {"LINIO 1 · INSULA RONDO", .15F}};
        }
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
        // Every place a road crosses the town's circle, leaving: a road with one end in town, or a
        // highway passing through the edge of it (both ends out of town, crossing it twice).
        struct Crossing { Vector3 at, outside; };
        std::vector<std::pair<const world::Road*, Crossing>> crossings;
        for (const auto* road : roads) {
            const float dx = road->end.x - road->start.x, dz = road->end.z - road->start.z;
            const float fx = road->start.x - c.x, fz = road->start.z - c.z;
            const float qa = dx * dx + dz * dz, qb = 2 * (fx * dx + fz * dz), qc = fx * fx + fz * fz - R * R;
            const float disc = qb * qb - 4 * qa * qc;
            if (qa < 1e-6F || disc <= 0) continue;
            const bool through = flat(road->start, c) >= R && flat(road->end, c) >= R;
            if (through && road->type != world::RoadType::Highway) continue;
            if (world::one_way(road->type)) continue;   // (an interchange's slip roads and roundabout are not a way into town)
            for (const float sign : {-1.0F, 1.0F}) {
                const float t = (-qb + sign * std::sqrt(disc)) / (2 * qa);
                if (t <= 0 || t >= 1) continue;
                // The smaller root enters the circle going start -> end; the larger leaves it.
                crossings.push_back({road, {Vector3Lerp(road->start, road->end, t), sign < 0 ? road->start : road->end}});
            }
        }
        for (const auto& [road, crossing] : crossings) {
            const Vector3 at = crossing.at, outside = crossing.outside;
            if (std::any_of(exits.begin(), exits.end(), [&](const Exit& e) {return flat(e.at, at) < 12;})) continue;
            Exit exit{at, flat_dir(at, outside), road->width, std::vector<float>(towns.size(), -1)};
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
// Ferarbaro's back roads: plywood boards with what the backcountry thinks, painted by hand.
// Verda belongs to Verdans; my land is mine, yours is yours; help your neighbour, leave people alone.
void backcountry(const world::VerdaRegion& region, Signage& out) {
    struct Words { std::vector<SignLine> lines; int weight; };
    const std::vector<Words> words{
        {{{"VERDA ESTAS", .42F}, {"LIBERA", .6F}}, 4},            // Verda is free
        {{{"LASU NIN", .42F}, {"VIVI", .7F}}, 4},                 // Let us live
        {{{"NENIU", .5F}, {"MASTRO", .6F}}, 4},                   // No master
        {{{"VERDA APARTENAS", .34F}, {"AL VERDANOJ", .4F}}, 2},   // Verda belongs to Verdans
        {{{"MIA TERO ESTAS MIA.", .3F}, {"VIA TERO ESTAS VIA.", .3F}}, 2},
        {{{"HELPU VIAN NAJBARON.", .3F}, {"LASU HOMOJN EN PACO.", .3F}}, 2},
        {{{"RIPAROJ · MOTOROJ", .32F}, {"VELDADO", .45F}}, 1},     // repairs, engines, welding
        {{{"FREŜAJ OVOJ", .45F}}, 1},                             // fresh eggs
        {{{"ATENTU", .4F}, {"LA HUNDON!", .5F}}, 1},               // beware of the dog
    };
    int total = 0;
    for (const auto& w : words) total += w.weight;
    const Color boards[]{{196, 166, 116, 255}, {182, 150, 104, 255}, {214, 206, 186, 255}};
    const Color inks[]{{24, 22, 20, 255}, {150, 30, 24, 255}, {20, 80, 36, 255}};
    std::vector<Vector3> placed;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
        const Vector3 mid = Vector3Lerp(road.start, road.end, .5F);
        if (world::zones::zone_at(road.start) != world::zones::ZoneId::Ferarbaro ||
            world::zones::zone_at(road.end) != world::zones::ZoneId::Ferarbaro) continue;
        // Highways get a board now and then; the gravel and dirt roads, more of them.
        const float spacing = road.type == world::RoadType::Asphalt || road.type == world::RoadType::Highway ? 170 : 55;
        const float length = flat(road.start, road.end);
        const Vector3 d = flat_dir(road.start, road.end), side{d.z, 0, -d.x};
        // A road shorter than the spacing (a piece of a highway's curve) gets one by chance.
        std::vector<float> spots;
        if (length >= spacing) for (float t = spacing * .5F; t < length - 10; t += spacing) spots.push_back(t);
        else if ((static_cast<std::uint32_t>(std::abs(mid.x * 17 + mid.z * 29)) * 2654435761U >> 8) % 1000U < static_cast<std::uint32_t>(length / spacing * 1000))
            spots.push_back(length * .5F);
        for (const float t : spots) {
            const auto h = static_cast<std::uint32_t>(std::abs(mid.x * 13 + mid.z * 7 + t * 31)) * 2654435761U;
            const float flip = (h >> 3) % 2 ? 1.0F : -1.0F;
            Vector3 p = Vector3Add(Vector3Lerp(road.start, road.end, t / length), Vector3Scale(side, flip * (road.width * .5F + 2.2F)));
            if (std::any_of(placed.begin(), placed.end(), [&](Vector3 q) {return flat(p, q) < 25;})) continue;
            int k = 0;
            for (int pick = static_cast<int>((h >> 7) % static_cast<std::uint32_t>(total)); pick >= words[static_cast<std::size_t>(k)].weight; ++k)
                pick -= words[static_cast<std::size_t>(k)].weight;
            bool clear = false;
            for (int step = 0; step < 4 && !(clear = clear_spot(region, p, .5F)); ++step) p = Vector3Add(p, Vector3Scale(side, flip));
            if (!clear) continue;
            Sign sign;
            sign.kind = SignKind::Homemade;
            sign.place = town.id;
            sign.front = words[static_cast<std::size_t>(k)].lines;
            sign.board = boards[(h >> 11) % 3];
            sign.ink = inks[(h >> 13) % 3];
            sign.post = .9F + static_cast<float>((h >> 15) % 4) * .1F;
            sign.max_width = 2.6F;
            const float rows = static_cast<float>(sign.front.size());
            sign.position = {p.x, ground(region, p, p.y) + sign.post + rows * .3F, p.z};
            // Faces the road, a little askew.
            sign.yaw = yaw_of(Vector3Scale(side, -flip)) + static_cast<float>(static_cast<int>((h >> 17) % 21) - 10);
            out.signs.push_back(std::move(sign));
            placed.push_back(p);
        }
    }
}
}

// The ring road's signs, Verda's autobahn: for each interchange and each carriageway, an exit board
// a kilometre before it ("ELIRO 2 / PORTO LUMA / VERDA URBO / 1 km"), another where the slip road
// leaves ("ELIRO 2 »"), and beyond the entry a board with the next two exits and how far; and on the
// highway into each interchange's roundabout, the way onto the ring.
std::string kilometres(float metres) {
    if (metres < 950) return std::to_string(static_cast<int>(std::round(metres / 50) * 50)) + " m";
    const int tenths = static_cast<int>(std::round(metres / 100));
    return std::to_string(tenths / 10) + (tenths % 10 ? "," + std::to_string(tenths % 10) : std::string()) + " km";
}
std::string upper_ascii(std::string text) {
    for (auto& c : text) if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return text;
}
void highway_signs(const world::VerdaRegion& region, Signage& out) {
    namespace hw = world::highways;
    const auto& all = hw::interchanges();
    if (all.empty()) return;
    constexpr float ring_length = 2 * PI * 1500;
    // Where the crossing highway goes the other way (into the island): Verda City.
    const auto board = [&](Vector3 at, Vector3 facing, std::string place, std::vector<SignLine> lines, float post = 3.2F) {
        Sign sign;
        sign.kind = SignKind::Highway;
        sign.place = std::move(place);
        sign.front = std::move(lines);
        sign.max_width = 8.5F;
        sign.post = post;
        float height = 0;
        for (const auto& line : sign.front) height += line.height * 1.25F;
        sign.position = {at.x, ground(region, at, world::terrain::TerrainHeight::sample(at.x, at.z)) + post + height * .5F + .2F, at.z};
        sign.yaw = yaw_of(facing);
        out.signs.push_back(std::move(sign));
    };
    for (std::size_t k = 0; k < all.size(); ++k) {
        const auto& x = all[k];
        const float radius = std::hypot(x.at.x, x.at.z), bearing = std::atan2(x.at.z, x.at.x);
        const float sense = x.ring_forward.x * -std::sin(bearing) + x.ring_forward.z * std::cos(bearing) > 0 ? 1.0F : -1.0F;
        const auto on_ring = [&](float s, float lateral) {
            const float b = bearing + sense * s / radius, r = radius + lateral;
            return Vector3{r * std::cos(b), 0, r * std::sin(b)};
        };
        const auto along_ring = [&](float s) { const float b = bearing + sense * s / radius; return Vector3{-std::sin(b) * sense, 0, std::cos(b) * sense}; };
        const std::string exit = "ELIRO " + std::to_string(x.number);
        const std::string town = upper_ascii(std::string(x.town));
        for (const float way : {1.0F, -1.0F}) {
            // Traffic keeps right: forward (the way the ring's points run) inside the ring, the other way outside.
            const float side = way > 0 ? -1.0F : 1.0F, verge = side * (hw::paved_half + hw::verge + 2.5F);
            const auto facing = [&](float s) { const Vector3 d = along_ring(s); return Vector3{-d.x * way, 0, -d.z * way}; };
            const float advance = -way * 1000, gore = -way * (hw::slip_reach + 40);
            board(on_ring(advance, verge), facing(advance), exit, {{exit, .55F}, {town, .85F}, {"VERDA URBO", .6F}, {"1 km", .5F}});
            board(on_ring(gore, verge), facing(gore), exit, {{exit + "  »", .7F}, {town, .85F}, {"VERDA URBO", .6F}});
            // Beyond the entry: the next two exits this way round, and how far.
            std::vector<SignLine> next{{"RINGVOJO", .7F}};
            for (int n = 1; n <= 2; ++n) {
                const std::size_t count = all.size(), step = static_cast<std::size_t>(n) % count;
                const auto& y = all[way > 0 ? (k + step) % count : (k + count - step) % count];
                float metres = way > 0 ? y.ring_s - x.ring_s : x.ring_s - y.ring_s;
                metres = std::fmod(metres + 2 * ring_length, ring_length) - hw::slip_reach - 120;
                next.push_back({std::to_string(y.number) + "  " + upper_ascii(std::string(y.town)) + "  " + kilometres(metres), .5F});
            }
            const float after = way * (hw::slip_reach + 120);
            board(on_ring(after, verge), facing(after), "ring", next);
        }
        // (Below: the bar and grills.)
        // On the highway into the roundabout, from either side: the way onto the ring.
        for (const float leg : {1.0F, -1.0F}) {
            const Vector3 in{-x.outward.x * leg, 0, -x.outward.z * leg};          // the inbound direction
            const Vector3 right{-in.z, 0, in.x};
            const Vector3 at{x.at.x + x.outward.x * leg * (hw::roundabout_pad + 40) + right.x * (hw::paved_half + 4),
                             0, x.at.z + x.outward.z * leg * (hw::roundabout_pad + 40) + right.z * (hw::paved_half + 4)};
            board(at, {-in.x, 0, -in.z}, exit, {{"RINGVOJO", .75F}, {exit, .5F}, {leg > 0 ? "VERDA URBO" : town, .55F}}, 2.4F);
        }
    }
}

// The ring's bar and grills: a blue services board a kilometre before each and at its slip road,
// and its own tall sign by the ring.
void rest_area_signs(const world::VerdaRegion& region, Signage& out) {
    namespace hw = world::highways;
    for (const auto& rest : hw::rest_areas()) {
        const float side = rest.inside ? -1.0F : 1.0F, way = rest.inside ? 1.0F : -1.0F;
        const auto on_ring = [&](float s, float lateral) {
            const float b = rest.bearing + s / 1500, r = 1500 + side * lateral;
            return Vector3{r * std::cos(b), 0, r * std::sin(b)};
        };
        const auto facing = [&](float s) {   // toward the traffic coming up to it
            const float b = rest.bearing + s / 1500;
            return Vector3{std::sin(b) * way, 0, -std::cos(b) * way};
        };
        const auto add = [&](Vector3 at, Vector3 face, std::vector<SignLine> lines, float post, Color board) {
            Sign sign;
            sign.kind = SignKind::Highway;
            sign.place = "la_ringo_" + std::string(rest.name);
            sign.front = std::move(lines);
            sign.max_width = 8.5F;
            sign.post = post;
            sign.board = board;
            float height = 0;
            for (const auto& line : sign.front) height += line.height * 1.25F;
            sign.position = {at.x, ground(region, at, world::terrain::TerrainHeight::sample(at.x, at.z)) + post + height * .5F + .2F, at.z};
            sign.yaw = yaw_of(face);
            out.signs.push_back(std::move(sign));
        };
        const Color services{20, 70, 150, 255};
        const float verge = hw::paved_half + hw::verge + 2.5F;
        for (const auto& [s, distance] : {std::pair<float, const char*>{-way * 1000, "1 km"}, {-way * 190, "»"}})
            add(on_ring(s, verge), facing(s), {{"LA RINGO", .8F}, {"TRINKEJO · KRADROSTEJO", .5F}, {"DIZELO · PARKEJO", .45F}, {distance, .55F}}, 3.2F, services);
        // Its own sign, tall, by the ring: both faces.
        Sign pole;
        pole.kind = SignKind::Highway;
        pole.place = "la_ringo_" + std::string(rest.name);
        pole.front = {{"LA RINGO", 1.3F}, {"TRINKEJO KAJ KRADROSTEJO", .55F}};
        pole.back = pole.front;
        pole.max_width = 9;
        pole.post = 7;
        pole.board = {150, 30, 30, 255};
        const Vector3 at = on_ring(way * 10, 26);
        pole.position = {at.x, world::terrain::TerrainHeight::sample(at.x, at.z) + pole.post + 1.4F, at.z};
        pole.yaw = yaw_of(facing(0));
        out.signs.push_back(std::move(pole));
        // The brotherhood's home: its board by the bar's door, facing the car park.
        if (rest.brotherhood) {
            const Vector3 door = rest.front_door(0), step = rest.front_door(1), spot = rest.front_door(1.2F, -7);
            add(spot, {step.x - door.x, 0, step.z - door.z},
                {{"LA FRATECO", .62F}, {"DE KAMIONISTOJ", .42F}, {"NIA HEJMO · ĈIU ŜOFORO BONVENAS", .26F}}, 1.6F, Color{96, 52, 24, 255});
        }
    }
}

Signage build_signage(const world::VerdaRegion& region, const life::Island& island) {
    Signage out;
    shopfronts(region, island, out);
    squares(region, island, out);
    trailer_flags(region, out);
    bus_stops(region, out);
    street_blades(region, out);
    town_roads(region, island, out);
    backcountry(region, out);
    highway_signs(region, out);
    rest_area_signs(region, out);
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

Image apelacio_image(int width, const Font* font) {
    width = std::max(16, width);
    const int height = width / 2;
    const int W = width * 2, H = height * 2;
    Image image = GenImageColor(W, H, Color{246, 244, 236, 255});
    auto* pixels = static_cast<Color*>(image.data);
    const float w = static_cast<float>(W), h = static_cast<float>(H);
    const Color pine{22, 110, 48, 255}, trunk{96, 64, 36, 255};
    // The pine: a trunk and five tiers of boughs, widest at the bottom, under the motto.
    const float cx = w * .5F, top = h * .27F, base = h * .9F;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        const float fx = x + .5F, fy = y + .5F;
        if (std::abs(fx - cx) < h * .035F && fy > base - h * .1F && fy < base) pixels[y * W + x] = trunk;
        for (int tier = 0; tier < 5; ++tier) {
            const float t0 = top + (base - h * .1F - top) * tier / 5.0F * .92F, t1 = t0 + (base - top) * .26F;
            if (fy < t0 || fy > t1) continue;
            const float half = (fy - t0) / (t1 - t0) * h * (.12F + tier * .045F);
            if (std::abs(fx - cx) < half) pixels[y * W + x] = pine;
        }
    }
    if (font && font->glyphCount > 0 && font->baseSize > 0) {
        // Laid out glyph by glyph from the font's CPU images (raylib's text measuring needs a GPU).
        std::vector<std::pair<int, int>> glyphs; // glyph index, advance at base size
        float advance = 0;
        const float spacing = font->baseSize * .06F;
        for (int i = 0; apelacio_motto[i];) {
            int bytes = 0;
            const int codepoint = GetCodepointNext(&apelacio_motto[i], &bytes);
            i += std::max(1, bytes);
            const int index = GetGlyphIndex(*font, codepoint);
            const auto& glyph = font->glyphs[index];
            const int step = glyph.advanceX ? glyph.advanceX : static_cast<int>(font->recs[index].width);
            glyphs.push_back({index, step});
            advance += step + spacing;
        }
        const float scale = std::min(h * .15F / font->baseSize, w * .9F / std::max(1.0F, advance));
        float x = (w - advance * scale) * .5F;
        const float y = h * .06F;
        for (const auto& [index, step] : glyphs) {
            const auto& glyph = font->glyphs[index];
            if (glyph.image.data && glyph.image.width > 0) {
                const Rectangle source{0, 0, static_cast<float>(glyph.image.width), static_cast<float>(glyph.image.height)};
                const Rectangle target{x + glyph.offsetX * scale, y + glyph.offsetY * scale, glyph.image.width * scale, glyph.image.height * scale};
                ImageDraw(&image, glyph.image, source, target, Color{24, 30, 26, 255});
            }
            x += (step + spacing) * scale;
        }
    }
    ImageResize(&image, width, height);
    return image;
}

namespace {
bool in_polygon(const std::vector<Vector2>& poly, float x, float y) {
    bool inside = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        if ((poly[i].y > y) != (poly[j].y > y) &&
            x < (poly[j].x - poly[i].x) * (y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x) inside = !inside;
    return inside;
}
std::vector<Vector2> star_points(Vector2 c, float outer, float inner, int points, float turn) {
    std::vector<Vector2> out;
    for (int k = 0; k < points * 2; ++k) {
        const float a = turn + static_cast<float>(k) * PI / static_cast<float>(points);
        const float r = k % 2 ? inner : outer;
        out.push_back({c.x + std::sin(a) * r, c.y - std::cos(a) * r});
    }
    return out;
}
float capsule(float x, float y, Vector2 a, Vector2 b) {   // distance to the segment ab
    const Vector2 ab{b.x - a.x, b.y - a.y};
    const float t = std::clamp(((x - a.x) * ab.x + (y - a.y) * ab.y) / std::max(1e-6F, ab.x * ab.x + ab.y * ab.y), 0.0F, 1.0F);
    return std::hypot(x - (a.x + ab.x * t), y - (a.y + ab.y * t));
}
}

Image verda_mano_image(int width) {
    width = std::max(16, width);
    const int height = width / 2;
    const int W = width * 3, H = height * 3;   // drawn large, then shrunk (smooth edges)
    const float w = static_cast<float>(W), h = static_cast<float>(H);
    const Color field{247, 246, 240, 255}, green{0, 153, 0, 255}, ink{0, 84, 30, 255}, gold{232, 186, 52, 255},
                gold_dark{150, 106, 22, 255}, velvet{0, 86, 40, 255}, pearl{252, 250, 236, 255};
    Image image = GenImageColor(W, H, field);
    auto* px = static_cast<Color*>(image.data);
    const Vector2 c{w * .5F, h * .56F};
    const float band = h * .2F;
    const float star_r = h * .33F;
    const auto star = star_points(c, star_r, star_r * .52F, 5, 0);
    const auto star_rim = star_points(c, star_r + h * .018F, star_r * .52F + h * .018F, 5, 0);
    // The hand: palm, four fingers and the thumb out to the left (as on the hideout tags).
    const float u = h * .022F;
    const Vector2 palm{c.x, c.y + 3.5F * u};
    const Vector2 finger_base[4]{{c.x - 3.3F * u, c.y - .5F * u}, {c.x - 1.1F * u, c.y - 1.2F * u}, {c.x + 1.1F * u, c.y - 1.2F * u}, {c.x + 3.3F * u, c.y - .5F * u}};
    const float finger_len[4]{5.2F * u, 6.6F * u, 6.8F * u, 5.4F * u};
    const auto in_hand = [&](float x, float y) {
        const float dx = (x - palm.x) / (4.6F * u), dy = (y - palm.y) / (4.6F * u);
        if (dx * dx + dy * dy < 1 && y > c.y - 1.5F * u) return true;
        if (std::abs(x - palm.x) < 4.3F * u && y > c.y - .5F * u && y < palm.y + 2 * u) return true;
        for (int f = 0; f < 4; ++f)
            if (capsule(x, y, finger_base[f], {finger_base[f].x, finger_base[f].y - finger_len[f]}) < 1.05F * u) return true;
        return capsule(x, y, {palm.x - 3.2F * u, palm.y + .6F * u}, {palm.x - 6.4F * u, palm.y - 2.2F * u}) < 1.05F * u;   // the thumb
    };
    // The crown: a jewelled gold band, two pearled arches over a green velvet cap, and on top an
    // orb carrying Esperanto's green star.
    const float crown_w = h * .3F, crown_base = c.y - star_r * .74F, band_h = h * .05F;
    const float arch_h = h * .17F, arch_t = h * .022F;
    const Vector2 orb{c.x, crown_base - band_h - arch_h - h * .012F};
    const auto crown_star = star_points({orb.x, orb.y - h * .052F}, h * .042F, h * .019F, 5, 0);
    const auto crown_star_rim = star_points({orb.x, orb.y - h * .052F}, h * .058F, h * .03F, 5, 0);
    const auto arch_at = [&](float fx) {   // height of the arches' outline above the band at x
        const float rel = (fx - c.x) / (crown_w * .5F);
        return std::abs(rel) > 1 ? -1.0F : arch_h * std::sqrt(std::max(0.0F, 1 - rel * rel)) * (.82F + .18F * std::abs(rel));
    };
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        const float fx = x + .5F, fy = y + .5F;
        Color col = field;
        if (std::abs(fx - c.x) < band * .5F || std::abs(fy - h * .5F) < band * .5F) col = green;
        if (in_polygon(star_rim, fx, fy)) col = ink;
        if (in_polygon(star, fx, fy)) col = field;
        if (in_hand(fx, fy)) col = green;
        // Crown: cap, arches (outer and middle), band, pearls, orb, star.
        const float top = crown_base - band_h;
        if (const float arch = arch_at(fx); arch >= 0 && fy < top && fy > top - arch) {
            col = velvet;
            if (fy < top - arch + arch_t) col = gold;                                     // the outer arch
            if (std::abs(fx - c.x) < arch_t * .5F) col = gold;                           // the middle arch
            if (std::abs(fx - c.x) > crown_w * .5F - arch_t) col = gold;
        }
        for (int k = 0; k < 9; ++k) {   // pearls along the outer arch
            const float rel = -.88F + static_cast<float>(k) * .22F;
            const float px_ = c.x + rel * crown_w * .5F;
            if (std::hypot(fx - px_, fy - (top - arch_at(px_) + arch_t * .3F)) < h * .011F) col = pearl;
        }
        if (fy >= top && fy <= crown_base && std::abs(fx - c.x) < crown_w * .56F) {
            col = gold;
            const float slot = std::fmod(fx - c.x + crown_w, h * .06F);
            if (slot < h * .026F && std::abs(fy - (top + band_h * .5F)) < band_h * .28F) col = slot < h * .013F ? green : pearl;   // emeralds and pearls
            if (fy > crown_base - h * .007F || fy < top + h * .006F) col = gold_dark;
        }
        if (std::hypot(fx - orb.x, fy - orb.y) < h * .026F) col = std::abs(fy - orb.y) < h * .005F ? gold_dark : gold;
        if (in_polygon(crown_star_rim, fx, fy)) col = field;
        if (in_polygon(crown_star, fx, fy)) col = green;
        px[y * W + x] = col;
    }
    ImageResize(&image, width, height);
    return image;
}
}
