#include "outland/creator/CreatorMapIO.hpp"
#include "outland/game/culture/Esperanto.hpp"
#include "outland/game/culture/Signage.hpp"
#include "outland/game/life/LifePopulation.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/Zones.hpp"
#include "outland/world/physics/MeshCollision.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include <raymath.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
using namespace outland;
using namespace outland::game::culture;
namespace life = outland::game::life;
namespace {
void check(bool condition, const std::string& message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
bool same(Color a, Color b) { return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b) < 40; }
std::string all_text(const Sign& sign) {
    std::string text;
    for (const auto& line : sign.front) text += line.text + "|";
    return text;
}
}

int main() {
    // Esperanto helpers.
    check(utf8_codepoints("ĉu") == std::vector<int>({0x109, 'u'}), "UTF-8 decoding");
    check(printable("ĈĜĤĴŜŬ ĉĝĥĵŝŭ «VERDA» · 2,1 km"), "supersigned letters are in the sign font");
    check(!printable("日本"), "letters outside the font are caught");
    check(format_distance(840) == "850 m" && format_distance(2140) == "2,1 km" && format_distance(12960) == "13,0 km", "distances: " + format_distance(2140));
    check(place_sign(life::PlaceKind::Police, 1, "", 0, false).title == "POLICO", "police");
    check(place_sign(life::PlaceKind::Office, 1, "", 0, true).title == "REGISTARA PALACO", "the capital's first office is the government");
    check(place_sign(life::PlaceKind::Office, 1, "", 0, false).title == "URBODOMO", "a town's first office is its town hall");
    check(place_sign(life::PlaceKind::Shop, 5, "Novak", 0, false).subtitle == "ĈE NOVAK", "shops carry the owner's name");
    check(place_sign(life::PlaceKind::Home, 1, "", 0, false).title.empty(), "homes have no sign");
    check(upper("ĉe Ŝtono ĝis ŭ") == "ĈE ŜTONO ĜIS Ŭ", "uppercase with supersigns: " + upper("ĉe Ŝtono ĝis ŭ"));
    check(upper("Kovač Ružić") == "KOVAČ RUŽIĆ" && printable("KOVAČ RUŽIĆ Paŭlo"), "Slavic family names");
    check(place_sign(life::PlaceKind::Office, 3, "", 1, true).title != place_sign(life::PlaceKind::Office, 3, "", 2, true).title, "a town's offices differ");

    // The flag: Cuba's layout in Esperanto green and white.
    Image flag = flag_image(256);
    check(flag.width == 256 && flag.height == 128, "flag is 2:1");
    const auto at = [&](float x, float y) { return GetImageColor(flag, static_cast<int>(x * 255), static_cast<int>(y * 127)); };
    check(same(at(.9F, .1F), verda_green) && same(at(.9F, .5F), verda_green) && same(at(.9F, .9F), verda_green), "green stripes top, middle, bottom");
    check(same(at(.9F, .3F), WHITE) && same(at(.9F, .7F), WHITE), "white stripes between");
    check(same(at(.144F, .5F), verda_green), "green star in the triangle");
    check(same(at(.05F, .2F), WHITE) && same(at(.05F, .8F), WHITE), "white hoist triangle");
    check(same(at(.6F, .1F), verda_green), "stripes beyond the triangle");
    if (const char* out = std::getenv("OUTLAND_FLAG_PNG")) ExportImage(flag, out);
    UnloadImage(flag);
    {   // The Suda Kruco: a navy saltire edged white on green, white stars, the green star at the crossing.
        Image cross = suda_kruco_image(256);
        check(cross.width == 256 && cross.height == 128, "Suda Kruco is 2:1");
        const auto px = [&](float x, float y) { return GetImageColor(cross, static_cast<int>(x * 255), static_cast<int>(y * 127)); };
        const auto green = [](Color c) { return c.g > 120 && c.r < 60 && c.b < 60; };
        const auto navy = [](Color c) { return c.b > 70 && c.r < 50 && c.g < 70; };
        check(green(px(.5F, .06F)) && green(px(.06F, .5F)), "Suda Kruco field is green");
        check(navy(px(.05F, .07F)) || navy(px(.06F, .06F)), "Suda Kruco saltire is navy");
        check(green(px(.5F, .5F)), "Suda Kruco carries the green star at the crossing");
        if (const char* out = std::getenv("OUTLAND_SUDA_KRUCO_PNG")) ExportImage(cross, out);
        UnloadImage(cross);
    }

    // The published world.
    world::physics::MeshCollisionLibrary::add_root(OUTLAND_SOURCE_DIR);
    world::VerdaRegion region(true);
    check(creator::CreatorMapIO::load(region, std::string(OUTLAND_SOURCE_DIR) + "/maps/verda_world.map"), "load world map");
    world::physics::MeshCollisionLibrary::preload(region);
    const auto island = life::build_island(region, nullptr);
    check(island.residents.size() > 100, "the island is lived in");
    for (const auto& person : island.residents) check(printable(person.full_name()), "names print in the sign font: " + person.full_name());
    const auto started = std::chrono::steady_clock::now();
    const auto signage = build_signage(region, island);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::map<SignKind, int> kinds;
    for (const auto& sign : signage.signs) ++kinds[sign.kind];
    std::cout << signage.signs.size() << " signs (" << kinds[SignKind::Shopfront] << " shopfronts, " << kinds[SignKind::TownEntry]
              << " town entries, " << kinds[SignKind::Distance] << " distance, " << kinds[SignKind::Street] << " street, "
              << kinds[SignKind::BusStop] << " bus stop, " << kinds[SignKind::Notice] << " notices), " << signage.flags.size()
              << " flags in " << ms << " ms\n";

    if (std::getenv("OUTLAND_SIGN_LIST"))
        for (const auto& sign : signage.signs) std::cout << "  " << static_cast<int>(sign.kind) << " " << all_text(sign) << " @ " << sign.position.x << "," << sign.position.y << "," << sign.position.z << " yaw " << sign.yaw << '\n';
    for (const auto& sign : signage.signs) {
        check(!sign.front.empty(), "every sign says something: " + sign.place);
        for (const auto* side : {&sign.front, &sign.back}) for (const auto& line : *side)
            check(printable(line.text), "printable in the sign font: " + line.text);
        check(std::isfinite(sign.position.x) && std::isfinite(sign.position.y) && std::isfinite(sign.position.z), "finite: " + sign.place);
    }

    // Every working building has a sign over its door, flush on its facade.
    int working = 0, flush = 0;
    for (const auto& place : island.places) working += place.indoor && place.kind != life::PlaceKind::Home && place.kind != life::PlaceKind::Square;
    check(kinds[SignKind::Shopfront] == working, "a shopfront per workplace: " + std::to_string(kinds[SignKind::Shopfront]) + "/" + std::to_string(working));
    std::set<std::string> titles;
    for (const auto& sign : signage.signs) {
        if (sign.kind != SignKind::Shopfront) continue;
        titles.insert(sign.front.front().text);
        const Vector3 facing{std::sin(sign.yaw * DEG2RAD), 0, std::cos(sign.yaw * DEG2RAD)};
        // Behind the sign there is wall: at its centre, or beside the doorway under it.
        const Vector3 side{facing.z, 0, -facing.x};
        bool wall = false;
        for (const float offset : {0.0F, -1.1F, 1.1F}) {
            const Vector3 behind{sign.position.x - facing.x * .35F + side.x * offset, sign.position.y, sign.position.z - facing.z * .35F + side.z * offset};
            wall = wall || world::physics::WorldCollision::body_blocked(behind, sign.position.y - 1.3F, region, .12F);
        }
        const Vector3 front{sign.position.x + facing.x * .3F, sign.position.y, sign.position.z + facing.z * .3F};
        const bool open = !world::physics::WorldCollision::body_blocked(front, sign.position.y - 1.3F, region, .12F);
        flush += wall && open;
        if (!(wall && open) && std::getenv("OUTLAND_SIGN_DEBUG")) std::cout << "  off wall " << sign.place << " wall=" << wall << " open=" << open << " at " << sign.position.x << "," << sign.position.y << "," << sign.position.z << " yaw " << sign.yaw << '\n';
    }
    check(flush * 10 >= kinds[SignKind::Shopfront] * 9, "shopfront signs sit on their walls: " + std::to_string(flush) + "/" + std::to_string(kinds[SignKind::Shopfront]));
    for (const char* word : {"POLICO", "PREĜEJO", "KLINIKO", "REGISTARA PALACO"})
        check(titles.contains(word), std::string("downtown has a ") + word);

    // Town entries: every town, with its name, beside (not on) the road.
    for (const auto& town : region.settlements()) {
        int entries = 0;
        for (const auto& sign : signage.signs) {
            if (sign.kind != SignKind::TownEntry || sign.place != town.id) continue;
            ++entries;
            check(sign.front.size() == 3 && sign.front[0].text == "BONVENON AL" && sign.front[1].text == upper(town.name), "entry text: " + all_text(sign));
            check(sign.back.size() == 1 && sign.back[0].text == "ĜIS REVIDO!", "goodbye on the back");
            for (const auto& other : region.settlements()) for (const auto& road : other.roads) {
                const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, l2 = dx * dx + dz * dz;
                const float t = l2 < 1e-6F ? 0 : std::clamp(((sign.position.x - road.start.x) * dx + (sign.position.z - road.start.z) * dz) / l2, 0.0F, 1.0F);
                const float d = std::hypot(sign.position.x - road.start.x - dx * t, sign.position.z - road.start.z - dz * t);
                check(d > road.width * .5F + .5F, "entry sign off the road at " + town.name);
            }
        }
        check(entries >= 1, "every town is signed: " + town.name);
    }
    // Leaving the capital the signs point on to the other towns.
    std::set<std::string> onward;
    for (const auto& sign : signage.signs)
        if (sign.kind == SignKind::Distance && sign.place == "capital_verda")
            for (const auto& line : sign.front) {
                check(line.text.find(" km") != std::string::npos || line.text.find(" m") != std::string::npos, "distance: " + line.text);
                onward.insert(line.text.substr(0, line.text.find("  ")));
            }
    for (const char* name : {"Porto Luma", "Roka", "Suda Haveno", "Espera"}) check(onward.contains(name), std::string("the capital points to ") + name);

    // Street names on the downtown signals: two different streets at every signal.
    std::map<std::string, std::vector<std::string>> blades;
    for (const auto& sign : signage.signs) if (sign.kind == SignKind::Street) blades[sign.place].push_back(sign.front.front().text);
    std::set<std::string> streets;
    for (const auto& [light, names] : blades) {
        check(names.size() == 2 && names[0] != names[1], "two crossing streets at " + light);
        streets.insert(names.begin(), names.end());
    }
    check(blades.size() >= 18, "every downtown signal is named: " + std::to_string(blades.size()));
    check(streets.size() >= 6 && streets.contains("ZAMENHOF-STRATO"), "downtown has its streets: " + std::to_string(streets.size()));

    int stops = 0;
    int line_stops = 0;
    for (const auto& town : region.settlements()) for (const auto& asset : town.assets) {
        stops += asset.model_path.find("Bus stops/") != std::string::npos;
        line_stops += asset.model_path.ends_with("transit/bus_stop.glb") || asset.model_path.ends_with("transit/bus_stop_alt.glb");
    }
    check(kinds[SignKind::BusStop] == stops + line_stops, "every bus stop is signed");
    // Linio 1's stops carry their town's name.
    std::set<std::string> line_towns;
    for (const auto& sign : signage.signs)
        if (sign.kind == SignKind::BusStop && all_text(sign).find("LINIO 1 · INSULA RONDO") != std::string::npos) line_towns.insert(sign.front[1].text);
    check(line_stops == static_cast<int>(region.settlements().size()) && line_towns.size() == region.settlements().size(),
          "a named Linio 1 stop in every town: " + std::to_string(line_towns.size()));

    // Flags: town squares, police and town halls, the tallest towers.
    int square_flags = 0, wall_flags = 0, roof_flags = 0;
    for (const auto& flag : signage.flags) {
        check(flag.tip.y > flag.base.y && flag.width > 1, "flag staff points up: " + flag.place);
        if (flag.place.ends_with(":square")) ++square_flags;
        else if (flag.place.find(":police") != std::string::npos || flag.place.find(":town hall") != std::string::npos) ++wall_flags;
        else ++roof_flags;
        if (flag.place.ends_with(":square")) check(!world::physics::WorldCollision::body_blocked(flag.base, flag.base.y, region, .6F), "square flagpole stands clear: " + flag.place);
    }
    if (square_flags != static_cast<int>(region.settlements().size())) for (const auto& flag : signage.flags) std::cerr << "  flag " << flag.place << '\n';
    check(square_flags == static_cast<int>(region.settlements().size()), "a flag on every town square: " + std::to_string(square_flags));
    check(wall_flags >= 2 && roof_flags >= 2, "police, town hall and tower flags");
    int sovagejo_crosses = 0, crosses = 0;
    for (const auto& flag : signage.flags) if (flag.design == FlagDesign::SudaKruco) {
        ++crosses;
        if (std::hypot(flag.base.x + 1900, flag.base.z - 610) < 140) ++sovagejo_crosses;
        check(!world::physics::WorldCollision::body_blocked(flag.base, flag.base.y, region, .4F), "trailer flagpole stands clear: " + flag.place);
    }
    check(sovagejo_crosses >= 10 && sovagejo_crosses * 2 > crosses, "Sovaĝejo's trailer park flies the most Suda Kruco flags: " + std::to_string(sovagejo_crosses));

    // Ferarbaro's back roads carry hand-painted boards, and only Ferarbaro's.
    {
        int homemade = 0;
        std::set<std::string> said;
        for (const auto& sign : signage.signs) {
            if (sign.kind != SignKind::Homemade) continue;
            ++homemade;
            said.insert(all_text(sign));
            check(world::zones::zone_at(sign.position) == world::zones::ZoneId::Ferarbaro, "homemade signs stay in Ferarbaro: " + all_text(sign));
            check(sign.post > .5F && sign.front.size() <= 2, "a board on a post: " + all_text(sign));
        }
        std::cout << homemade << " homemade signs, " << said.size() << " different\n";
        check(homemade >= 8, "boards along the back roads: " + std::to_string(homemade));
        check(said.contains("VERDA ESTAS|LIBERA|") && said.contains("LASU NIN|VIVI|") && said.contains("NENIU|MASTRO|"),
              "VERDA ESTAS LIBERA, LASU NIN VIVI, NENIU MASTRO");
        bool entry = false;
        for (const auto& sign : signage.signs) entry |= sign.kind == SignKind::TownEntry && sign.place == "ferarbaro_sovagejo" && all_text(sign).find("SOVAĜEJO") != std::string::npos;
        check(entry, "BONVENON AL SOVAĜEJO");
    }

    // Deterministic.
    const auto again = build_signage(region, island);
    check(again.signs.size() == signage.signs.size() && again.flags.size() == signage.flags.size(), "same signs twice");
    for (std::size_t i = 0; i < again.signs.size(); ++i) check(all_text(again.signs[i]) == all_text(signage.signs[i]), "same text twice");

    std::cout << "signage tests passed\n";
    return 0;
}
