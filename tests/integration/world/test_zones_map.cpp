#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/MapScreen.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Zones.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace outland;
namespace zones = outland::world::zones;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
}

int main() {
    // Five zones, one round each town; each seat lies in its own zone.
    check(zones::zones.size() == 5, "five zones");
    for (const auto& zone : zones::zones) {
        const auto* seat = world::layout::find(zone.seat);
        check(seat && zones::zone_at(seat->center) == zone.id, "seat in its zone: " + std::string(zone.name));
    }
    check(zones::zone(zones::ZoneId::Ferarbaro).name == "Ferarbaro" && zones::zone(zones::ZoneId::Marcfluo).name == "Marĉfluo", "names");
    // Compass: west is Ferarbaro, east Lumamaro, north Altverdo, south Marĉfluo, the middle Verdkerno.
    check(zones::zone_at(-1900, 300) == zones::ZoneId::Ferarbaro, "west");
    check(zones::zone_at(1900, -200) == zones::ZoneId::Lumamaro, "east");
    check(zones::zone_at(0, -1900) == zones::ZoneId::Altverdo, "north");
    check(zones::zone_at(200, 1950) == zones::ZoneId::Marcfluo, "south");
    check(zones::zone_at(150, -100) == zones::ZoneId::Verdkerno, "centre");
    // Every zone holds a real share of the island's land.
    std::array<int, zones::zone_count> land{};
    int total = 0;
    for (float z = -2400; z <= 2400; z += 40) for (float x = -2400; x <= 2400; x += 40)
        if (world::terrain::TerrainHeight::sample(x, z) > world::layout::sea_level) {++land[static_cast<std::size_t>(zones::zone_at(x, z))]; ++total;}
    for (std::size_t i = 0; i < land.size(); ++i) {
        std::cout << zones::zones[i].name << " " << 100 * land[i] / total << "%  ";
        if (zones::zones[i].id == zones::ZoneId::Verdkerno)
            check(land[i] * 50 > total && land[i] * 10 < total, "Verdkerno is the capital district, not a county: " + std::to_string(100 * land[i] / total) + "%");
        else check(land[i] * 6 > total, "a real share of land: " + std::string(zones::zones[i].name));
    }
    // The capital district holds Verda City and stops at its edge.
    check(zones::zone_at(0, 0) == zones::ZoneId::Verdkerno && zones::zone_at(450, 0) == zones::ZoneId::Verdkerno &&
          zones::zone_at(0, -450) == zones::ZoneId::Verdkerno, "Verda City is Verdkerno");
    check(zones::zone_at(800, 0) == zones::ZoneId::Lumamaro && zones::zone_at(-800, 0) == zones::ZoneId::Ferarbaro &&
          zones::zone_at(0, -800) == zones::ZoneId::Altverdo && zones::zone_at(0, 800) == zones::ZoneId::Marcfluo,
          "outside the city the counties begin");
    std::cout << '\n';
    // Borders wander but stay consistent: nearby points agree away from them.
    check(zones::zone_at(-1750, 450) == zones::zone_at(-1745, 452), "stable");

    // The map: north is up, the capital in the middle, the island inside the frame.
    const Rectangle area{100, 50, 600, 600};
    const auto centre = game::MapScreen::to_map(area, 0, 0), north = game::MapScreen::to_map(area, 0, -1800);
    check(std::abs(centre.x - 400) < .01F && std::abs(centre.y - 350) < .01F, "capital in the middle");
    check(north.y < centre.y && std::abs(north.x - centre.x) < .01F, "north up");
    for (const auto& site : world::layout::sites) {
        const auto p = game::MapScreen::to_map(area, site.center.x, site.center.z);
        check(CheckCollisionPointRec(p, area), "towns on the map: " + std::string(site.name));
    }
    // The settings panel's buttons fit beside the map and never overlap.
    for (const auto [w, h] : {std::pair{1280, 720}, std::pair{2400, 1080}, std::pair{844, 390}}) {
        const auto map = game::MapScreen::map_area(w, h);
        check(map.x >= 0 && map.y >= 0 && map.x + map.width <= w && map.y + map.height <= h, "map on screen");
        for (int i = 0; i < 8; ++i) {
            const auto b = game::MapScreen::settings_button(i, w, h);
            check(b.x >= map.x + map.width && b.x + b.width <= w && b.y + b.height <= h, "button on screen beside the map");
            for (int j = 0; j < i; ++j) check(!CheckCollisionRecs(b, game::MapScreen::settings_button(j, w, h)), "buttons apart");
            check(!CheckCollisionRecs(b, game::MapScreen::close_button(w, h)), "clear of CLOSE");
        }
    }

    // Settings survive a save and load, and nonsense is clamped.
    const auto dir = std::filesystem::temp_directory_path() / "outland_settings_test";
    std::filesystem::create_directories(dir / "maps");
    const auto path = game::GameSettings::path_for((dir / "maps" / "verda.map").string());
    check(path == (dir / "profiles" / "settings.cfg").string(), "settings sit with the profiles: " + path);
    game::GameSettings settings;
    settings.look_sensitivity = 2.5F; settings.mouse_sensitivity = .7F; settings.aim_sensitivity = .6F; settings.invert_y = true;
    settings.aim_toggle = true; settings.aim_assist = false; settings.vibration = false; settings.touch_controls = game::GameSettings::TouchControls::Off;
    settings.button_size = 1.2F; settings.volume = .3F; settings.muted = true; settings.graphics = game::GameSettings::Graphics::Fast;
    settings.bindings.bind(input::Action::Fire, 1, KEY_F);
    check(settings.save(path), "save settings");
    game::GameSettings read;
    check(read.load(path) && read.look_sensitivity == 2.5F && std::abs(read.mouse_sensitivity - .7F) < .001F &&
          std::abs(read.aim_sensitivity - .6F) < .001F && read.invert_y && read.aim_toggle && !read.aim_assist && !read.vibration &&
          read.touch_controls == game::GameSettings::TouchControls::Off && std::abs(read.button_size - 1.2F) < .001F &&
          std::abs(read.volume - .3F) < .001F && read.muted && read.graphics == game::GameSettings::Graphics::Fast, "load settings");
    check(read.bindings[input::Action::Fire][1] == KEY_F && read.bindings[input::Action::Interact][1] == 0 &&
          read.bindings[input::Action::Forward][0] == KEY_W, "key bindings survive");
    // The old look stick's speed is not a swipe or mouse speed: it is left behind.
    { std::ofstream bad(path); bad << "look_sensitivity 4\ntouch_sensitivity 99\nmouse_sensitivity -3\nvolume -4\ngraphics 9\nunknown 1 2 3\nbind nothing 1 2\n"; }
    game::GameSettings clamped;
    check(clamped.load(path) && clamped.look_sensitivity == game::GameSettings::max_sensitivity &&
          clamped.mouse_sensitivity == game::GameSettings::min_mouse && clamped.volume == 0 &&
          clamped.graphics == game::GameSettings::Graphics::Sharp && clamped.bindings[input::Action::Fire][0] == input::Bindings::mouse(MOUSE_BUTTON_LEFT), "clamped");
    game::GameSettings missing;
    check(!missing.load((dir / "none.cfg").string()) && missing.look_sensitivity == 1.0F && missing.mouse_sensitivity == 1.0F &&
          missing.touch_controls == game::GameSettings::TouchControls::Auto, "defaults when there is no file");
    read.reset_controls();
    check(read.volume < .5F && read.muted && !read.invert_y && read.bindings[input::Action::Interact][1] == KEY_F, "reset keeps sound and graphics");
    std::filesystem::remove_all(dir);

    // Sovaĝejo, heart of Ferarbaro, in the published world.
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    const world::Settlement* town = nullptr;
    for (const auto& s : region.settlements()) if (s.id == "ferarbaro_sovagejo") town = &s;
    check(town && town->name == "Sovaĝejo", "Sovaĝejo is on the map");
    check(zones::zone_at(town->center) == zones::ZoneId::Ferarbaro, "in Ferarbaro");
    int dirt = 0, gravel = 0, paved = 0, donors = 0, runners = 0, shacks = 0, trailers = 0;
    for (const auto& road : town->roads) {
        dirt += road.type == world::RoadType::Dirt; gravel += road.type == world::RoadType::Gravel; paved += road.type == world::RoadType::Asphalt;
    }
    for (const auto& marker : town->gameplay_markers) if (marker.type == world::GameplayMarkerType::VehicleSpawn) {
        if (game::vehicles::parts_donor(marker.id)) ++donors; else ++runners;
    }
    for (const auto& s : region.settlements()) for (const auto& asset : s.assets) {
        if (std::hypot(asset.position.x - town->center.x, asset.position.z - town->center.z) > 220) continue;
        shacks += asset.model_path.ends_with("Shack A.glb");
        trailers += asset.model_path.find("residential/trailers/") != std::string::npos;
    }
    std::cout << "Sovaĝejo: " << trailers << " trailers, " << shacks << " homesteads, " << runners << " cars that run, " << donors
              << " parts donors, roads " << dirt << " dirt / " << gravel << " gravel / " << paved << " paved, " << town->survivors << " people\n";
    check(paved == 0 && gravel == 0 && dirt >= 2, "dirt roads only out here");
    check(trailers == 12 && shacks >= 5, "trailers and homesteads");
    check(donors >= 4 && runners >= 8, "a car that runs and the parts donors beside it");
    check(town->survivors >= 40, "people live here");
    std::cout << "zone and map tests passed\n";
    return 0;
}
