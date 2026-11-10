// Sovaĝejo's country (world/SovagejoCounty.hpp): short rough tracks off the mountain road and
// the homestead road, each joined cleanly where it leaves, the land cut and filled to it and level
// under every home and under Fajrejo; then on the world map: the homes and the little settlement,
// their yards in Sovaĝejo's way, everyone counted as Sovaĝejo's, the coast and the mountain left
// alone - Sovaĝa Monto still the island's highest ground.
#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/culture/Signage.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/MountainRoad.hpp"
#include "outland/world/SovagejoCounty.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Zones.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace outland;
namespace cty = outland::world::county;
namespace mtn = outland::world::mountain;
using world::terrain::TerrainHeight;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float to_segment(Vector3 p, Vector3 a, Vector3 b) {
    const float dx = b.x - a.x, dz = b.z - a.z, l2 = dx * dx + dz * dz;
    const float t = l2 > 1e-6F ? std::clamp(((p.x - a.x) * dx + (p.z - a.z) * dz) / l2, 0.0F, 1.0F) : 0;
    return std::hypot(p.x - a.x - dx * t, p.z - a.z - dz * t);
}
}

int main() {
    TerrainHeight::set_coastal_layout(true);
    const auto& tracks = cty::tracks();
    const auto& sites = cty::sites();
    const auto& v = cty::village();
    check(tracks.size() >= 5 && tracks.size() <= 9, "a few tracks, no more: " + std::to_string(tracks.size()));
    // Each track joins what it leaves exactly, and ends at its home or the village.
    for (const auto& t : tracks) {
        const Vector3 a = t.points.front(), b = t.points.back();
        check(t.length() > 25 && t.length() < 220, t.name + ": short (" + std::to_string(t.length()) + " m)");
        if (t.from == cty::From::Mountain) {
            check(mtn::distance(a.x, a.z) < .3F, t.name + ": leaves the mountain road from its centre line");
            float s_best = 0, best = 1e9F;
            for (float s = 0; s < mtn::road().length(); s += .5F) if (flat(mtn::road().at(s), a) < best) {best = flat(mtn::road().at(s), a); s_best = s;}
            check(std::abs(a.y - mtn::road().at(s_best).y) < .05F, t.name + ": at the road's own height");
        }
        if (t.from == cty::From::Homesteads) check(std::abs(a.x - world::layout::sovagejo_center.x) < .3F && a.z > 142 && a.z < 324, t.name + ": off the homestead road");
        if (t.from == cty::From::Village) check(flat(a, v.west) < .5F || flat(a, v.east) < .5F, t.name + ": out of Fajrejo's lot");
        if (t.from == cty::From::Track) {
            float best = 1e9F, y = 0;
            for (const auto& q : tracks[static_cast<std::size_t>(t.parent)].points) if (flat(q, a) < best) {best = flat(q, a); y = q.y;}
            check(best < .5F && std::abs(y - a.y) < .05F, t.name + ": off its track, level with it");
        }
        bool ends = flat(b, v.centre) < v.flat;
        for (const auto& s : sites) ends |= flat(b, s.centre) < s.flat;
        check(ends, t.name + ": ends at a home or in Fajrejo");
    }
    // The land meets each track: its profile, level across the bed, no steeper than a rough track
    // gets (away from the mountain road's own bed and from the meetings of two tracks).
    for (std::size_t k = 0; k < tracks.size(); ++k) {
        const auto& t = tracks[k];
        float off = 0, across = 0, grade = 0;
        for (float s = 1; s < t.length() - 1; s += 1) {
            const Vector3 p = t.at(s), h = t.heading(s), left{-h.z, 0, h.x};
            if (mtn::distance(p.x, p.z) < 22) continue;
            bool meeting = false;
            for (std::size_t j = 0; j < tracks.size(); ++j) {
                if (j == k) continue;
                for (const auto& q : tracks[j].points) meeting |= flat(q, p) < 14;
            }
            if (meeting) continue;
            const float y = TerrainHeight::sample(p.x, p.z);
            off = std::max(off, std::abs(y - p.y));
            across = std::max(across, std::abs(TerrainHeight::sample(p.x + left.x * 2.5F, p.z + left.z * 2.5F) - TerrainHeight::sample(p.x - left.x * 2.5F, p.z - left.z * 2.5F)));
            grade = std::max(grade, std::abs(TerrainHeight::sample(p.x + h.x, p.z + h.z) - y));
        }
        check(off < .08F, t.name + ": the ground is the track (" + std::to_string(off) + ")");
        check(across < .2F, t.name + ": level across (" + std::to_string(across) + ")");
        check(grade < cty::max_grade + .06F, t.name + ": no steeper than a rough track (" + std::to_string(grade) + ")");
    }
    // The mountain road as it was: its centre line on its profile.
    {
        float off = 0;
        for (float s = 2; s < mtn::road().length() - 60; s += 2) {
            const Vector3 p = mtn::road().at(s);
            off = std::max(off, std::abs(TerrainHeight::sample(p.x, p.z) - p.y));
        }
        check(off < .05F, "Sovaĝa Monto's road untouched: " + std::to_string(off));
    }
    // Level pads: every home's, and Fajrejo's; all of it well up from the sea, in Ferarbaro.
    for (const auto& s : sites) {
        for (const float r : {0.0F, s.flat * .5F, s.flat * .9F})
            for (int a = 0; a < 8; ++a) {
                const float x = s.centre.x + r * std::cos(a * .785F), z = s.centre.z + r * std::sin(a * .785F);
                check(std::abs(TerrainHeight::sample(x, z) - s.centre.y) < .05F, s.name + "'s pad is level");
            }
        check(s.centre.y > world::layout::sea_level + 30, s.name + ": well up from the sea");
        check(world::zones::zone_at(s.centre) == world::zones::ZoneId::Ferarbaro, s.name + ": in Ferarbaro");
        float cut = 0;
        for (const float r : {0.0F, s.flat}) for (int a = 0; a < 8; ++a)
            cut = std::max(cut, std::abs(TerrainHeight::ungraded(s.centre.x + r * std::cos(a * .785F), s.centre.z + r * std::sin(a * .785F)) - s.centre.y));
        check(cut < 4, s.name + ": only a little cut and fill (" + std::to_string(cut) + " m)");
    }
    for (const float r : {0.0F, 15.0F, 30.0F, v.flat * .9F})
        for (int a = 0; a < 12; ++a)
            check(std::abs(TerrainHeight::sample(v.centre.x + r * std::cos(a * .52F), v.centre.z + r * std::sin(a * .52F)) - v.centre.y) < .05F, "Fajrejo stands level");
    // Sovaĝa Monto: still the island's highest ground, nothing of the county near it.
    {
        const Vector3 top = mtn::summit();
        float elsewhere = -1e9F;
        for (float x = -2400; x <= 2400; x += 40)
            for (float z = -2400; z <= 2400; z += 40)
                if (std::hypot(x - top.x, z - top.z) > 400) elsewhere = std::max(elsewhere, TerrainHeight::sample(x, z));
        check(top.y > 250 && elsewhere < top.y - 60, "Sovaĝa Monto the highest: " + std::to_string(top.y) + " (" + std::to_string(elsewhere) + " elsewhere)");
    }

    // On the world map.
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    std::size_t sovagejo = 0;
    for (std::size_t i = 0; i < region.settlements().size(); ++i) if (region.settlements()[i].id == "ferarbaro_sovagejo") sovagejo = i;
    const auto& town = region.settlements()[sovagejo];
    // The tracks are Sovaĝejo's dirt roads; the lot its gravel.
    int dirt = 0, lot = 0;
    for (const auto& r : town.roads) {
        dirt += r.type == world::RoadType::Dirt && r.width == cty::track_width && cty::near(r.start.x, r.start.z, 1) && cty::near(r.end.x, r.end.z, 1);
        lot += r.type == world::RoadType::Gravel && r.width == cty::lot_width;
    }
    check(dirt >= static_cast<int>(tracks.size()) && lot == 1, "the tracks and Fajrejo's lot on the map");
    // The homes: 8 to 12 out in the country, Fajrejo's church, store, shed and houses.
    int country = 0, village_homes = 0, church = 0, store = 0, shed = 0, buildings = 0;
    for (const auto& s : region.settlements()) for (const auto& m : s.gameplay_markers) {
        if (m.type != world::GameplayMarkerType::BuildingPurpose || !cty::near(m.position.x, m.position.z, 2)) continue;
        const bool in_village = flat(m.position, v.centre) < v.flat + 6;
        if (m.purpose == world::BuildingPurpose::Home) (in_village ? village_homes : country)++;
        if (in_village) {church += m.purpose == world::BuildingPurpose::Church; store += m.purpose == world::BuildingPurpose::Shop; shed += m.purpose == world::BuildingPurpose::Garage;}
        // Nothing on a road, the homes on their own ground (up from the sea).
        for (const auto& owner : region.settlements()) for (const auto& r : owner.roads)
            check(to_segment(m.position, r.start, r.end) > r.width * .5F + .5F, "a building clear of the roads at " + std::to_string(m.position.x) + "," + std::to_string(m.position.z));
        check(m.position.y > world::layout::sea_level + 30, "on dry ground");
    }
    for (const auto& s : region.settlements()) for (const auto& a : s.assets)
        if (flat(a.position, v.centre) < v.flat + 6 && (a.model_path.find("/city/country/") != std::string::npos || a.model_path.ends_with("church_baptist.glb") ||
                                                       a.model_path.find("/residential/trailers/") != std::string::npos) && a.size.y > 3) ++buildings;
    check(country >= 8 && country <= 12, "8-12 homes out in the country: " + std::to_string(country));
    check(church == 1 && store == 1 && shed == 1 && village_homes >= 3 && buildings >= 6 && buildings <= 10,
          "Fajrejo: a church, a store, a mechanic and houses: " + std::to_string(buildings) + " buildings, " + std::to_string(village_homes) + " homes");
    // Sovaĝejo's way: old cars and parts donors, generators, woodpiles, a deer stand, boats.
    int donors = 0, runners = 0, generators = 0, woodpiles = 0, stands = 0, boats = 0;
    for (const auto& m : town.gameplay_markers)
        if (m.type == world::GameplayMarkerType::VehicleSpawn && m.id.find("_kamparo_") != std::string::npos)
            (game::vehicles::parts_donor(m.id) ? donors : runners)++;
    for (const auto& s : region.settlements()) for (const auto& a : s.assets) {
        if (!cty::near(a.position.x, a.position.z, 40)) continue;
        generators += a.model_path.ends_with("country/generator.glb");
        woodpiles += a.model_path.ends_with("country/woodpile.glb");
        stands += a.model_path.ends_with("country/deer_stand.glb");
        boats += a.model_path.ends_with("country/boat_trailer.glb");
    }
    check(donors >= 8 && runners >= 10 && generators >= 4 && woodpiles >= 4 && stands >= 1 && boats >= 2,
          "the yards: " + std::to_string(runners) + " cars, " + std::to_string(donors) + " donors, " + std::to_string(generators) + " generators, " +
              std::to_string(woodpiles) + " woodpiles, " + std::to_string(stands) + " deer stands, " + std::to_string(boats) + " boats");
    // Everyone here is Sovaĝejo's.
    const auto island = game::life::build_island(region, nullptr);
    int counted = 0;
    for (const auto& place : island.places) {
        if (!cty::near(place.interior.x, place.interior.z, 2)) continue;
        check(place.settlement == static_cast<int>(sovagejo), "a county home is Sovaĝejo's: " + place.id);
        ++counted;
    }
    check(counted >= 12, "the county's places in island life: " + std::to_string(counted));
    // Its signs and the Apelacio: a board at each track's mouth, names at the mailboxes, flags.
    const auto signage = game::culture::build_signage(region, island);
    int boards = 0, flags = 0;
    for (const auto& sign : signage.signs) boards += sign.place.starts_with("kamparo:");
    for (const auto& flag : signage.flags)
        flags += flag.design == game::culture::FlagDesign::Apelacio && cty::near(flag.base.x, flag.base.z, 30);
    check(boards >= static_cast<int>(tracks.size() + sites.size()), "hand-painted boards: " + std::to_string(boards));
    check(flags >= 6, "the Apelacio over the county: " + std::to_string(flags));
    std::cout << "Sovaĝejo's country: " << tracks.size() << " tracks, " << country << " homes, Fajrejo " << buildings << " buildings ("
              << village_homes << " homes), " << runners << " cars and " << donors << " donors, " << boards << " boards, " << flags << " flags: GREEN\n";
}
