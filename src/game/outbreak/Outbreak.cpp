#include "outland/game/outbreak/Outbreak.hpp"
#include "outland/world/VerdaRegion.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace outland::game::outbreak {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
constexpr float wake_fade = 2.5F;
constexpr float shots_pause = 2.4F;   // after the shots, before anyone speaks
}

const char* stage_name(Stage s) {
    switch (s) {
        case Stage::WakeUp: return "wake";
        case Stage::ToTruck: return "truck";
        case Stage::Drive: return "drive";
        case Stage::Standoff: return "standoff";
        case Stage::Clear: return "clear";
        case Stage::Free: return "free";
    }
    return "wake";
}

std::vector<Holdout> find_holdouts(const world::VerdaRegion& region) {
    std::vector<Holdout> out;
    const auto& towns = region.settlements();
    for (std::size_t k = 0; k < holdouts.size(); ++k) {
        const auto& spec = holdouts[k];
        Holdout h;
        h.spec = static_cast<int>(k);
        h.radius = spec.radius;
        bool found = false;
        if (spec.site == SiteKind::Town) {
            for (const auto& town : towns)
                if (town.id == spec.settlement) { h.centre = town.center; found = true; }
        } else {
            const char* model = spec.site == SiteKind::Prison ? "civic/state_prison.glb" : "civic/truck_depot.glb";
            Vector3 near{};
            for (const auto& town : towns) if (town.id == spec.settlement) near = town.center;
            float best = 1e30F;
            for (const auto& town : towns)
                for (const auto& asset : town.assets) {
                    if (!asset.model_path.ends_with(model)) continue;
                    const float d = spec.settlement.empty() ? 0 : flat(asset.position, near);
                    if (d < best) { best = d; h.centre = asset.position; found = true; }
                }
        }
        if (found) out.push_back(h);
    }
    return out;
}

Tower find_tower(const world::VerdaRegion& region) {
    Tower tower;
    float best = 1e30F;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            if (!asset.model_path.ends_with("towers/glass_dark_tower.glb")) continue;
            // Downtown: the one nearest the island's centre.
            const float d = std::hypot(asset.position.x, asset.position.z);
            if (d < best) { best = d; tower = {true, asset.position, asset.size}; }
        }
    return tower;
}

float initial_fuel(Vector3 at) {
    // Fixed by the place: from barely a can to a few tanks.
    std::uint32_t h = static_cast<std::uint32_t>(std::lround(at.x * 7.0F)) * 2654435761U ^ static_cast<std::uint32_t>(std::lround(at.z * 13.0F)) * 40503U;
    h ^= h >> 15; h *= 2246822519U; h ^= h >> 13;
    return .3F + static_cast<float>(h % 1000) / 1000.0F * 3.2F;
}

float fuel_left(const OutbreakState& state, int station, Vector3 at) {
    for (const auto& [index, left] : state.fuel) if (index == station) return left;
    return initial_fuel(at);
}

float draw_fuel(OutbreakState& state, int station, Vector3 at, float want) {
    const float left = fuel_left(state, station, at);
    const float taken = std::clamp(want, 0.0F, left);
    bool known = false;
    for (auto& [index, stock] : state.fuel) if (index == station) { stock = left - taken; known = true; }
    if (!known) state.fuel.emplace_back(station, left - taken);
    return taken;
}

std::vector<Clearing> clearings(const OutbreakState& state, Vector3 player, Vector3 hideout, const std::vector<Holdout>& found) {
    std::vector<Clearing> out{{player, sight_radius}};
    if (state.stage == Stage::Free) out.push_back({hideout, 220});
    for (const auto& h : found)
        if (h.spec >= 0 && std::find(state.found.begin(), state.found.end(), holdouts[static_cast<std::size_t>(h.spec)].id) != state.found.end())
            out.push_back({h.centre, h.radius * 1.6F});
    return out;
}

std::string serialise(const OutbreakState& s) {
    std::ostringstream out;
    out.precision(9);
    out << "stage=" << stage_name(s.stage) << "\n";
    if (s.has_position) out << "position=" << s.position.x << "," << s.position.y << "," << s.position.z << "," << s.yaw << "\n";
    if (!s.truck.empty()) out << "truck=" << s.truck << "\n";
    for (const auto& id : s.found) out << "found=" << id << "\n";
    for (const auto& [index, left] : s.fuel) out << "fuel=" << index << "," << left << "\n";
    out << "talks=" << s.talks << "\n";
    out << "tower=" << (s.tower_seen ? 1 : 0) << "\n";
    for (const auto& rig : s.rigs) out << "rig=" << rig << "\n";
    out << "played=" << static_cast<long long>(s.played) << "\n";
    return out.str();
}

bool parse(const std::string& text, OutbreakState& s) {
    s = {};
    std::istringstream in(text);
    std::string line;
    bool staged = false;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "stage") {
            for (const auto st : {Stage::WakeUp, Stage::ToTruck, Stage::Drive, Stage::Standoff, Stage::Clear, Stage::Free})
                if (value == stage_name(st)) { s.stage = st; staged = true; }
        } else if (key == "position") {
            float x = 0, y = 0, z = 0, yaw = 0;
            if (std::sscanf(value.c_str(), "%f,%f,%f,%f", &x, &y, &z, &yaw) == 4 && std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(yaw)) {
                s.has_position = true;
                s.position = {x, y, z};
                s.yaw = yaw;
            }
        } else if (key == "truck") s.truck = value;
        else if (key == "found") {
            for (const auto& spec : holdouts)
                if (value == spec.id && std::find(s.found.begin(), s.found.end(), value) == s.found.end()) s.found.push_back(value);
        } else if (key == "fuel") {
            int index = -1;
            float left = 0;
            if (std::sscanf(value.c_str(), "%d,%f", &index, &left) == 2 && index >= 0 && index < 1000 && std::isfinite(left))
                s.fuel.emplace_back(index, std::clamp(left, 0.0F, 10.0F));
        } else if (key == "talks") s.talks = std::clamp(std::atoi(value.c_str()), 0, 1 << 20);
        else if (key == "tower") s.tower_seen = value == "1";
        else if (key == "rig" && !value.empty() && s.rigs.size() < 8) s.rigs.push_back(value);
        else if (key == "played") s.played = std::max(0.0, std::atof(value.c_str()));
    }
    // A save from mid-scene starts that scene again from where it can.
    if (s.stage == Stage::Standoff) s.stage = Stage::Drive;
    return staged;
}

// ------------------------------------------------------------------ the story

const std::vector<Line>& Story::wake_lines() {
    static const std::vector<Line> lines{
        {ron, "Wake up. WAKE UP. Boots on - now."},
        {ron, "It's the city. People are... they're eating each other out there. I watched the Kovač girl go down in the street."},
        {ron, "Dario called. He's the only one of the Green Hand that made it. They hit his bar first - the whole place gone in minutes."},
        {ron, "He's holed up at the Garza place, Ranĉo Verdaj Kampoj, north of the city. In the barn. We go to him."},
        {ron, "My pickup's on the drive. You drive - my hands won't stop shaking. Hurry."},
    };
    return lines;
}

const std::vector<Line>& Story::arrive_lines() {
    static const std::vector<Line> lines{
        {dario, "Ron! Thank God. And the nephew - good. Inside, quick, before they hear the engine."},
        {rancher, "That's far enough! Nobody else comes on this land. Not after what we've seen."},
        {dario, "Easy, Rafael. It's family. One night in the barn, then we're gone."},
        {rancher, "Your kind brought this on Verda. Turn that truck round or we shoot - all of you."},
        {ron, "Dario... don't."},
    };
    return lines;
}

const std::vector<Line>& Story::after_lines() {
    static const std::vector<Line> lines{
        {ron, "Dario! What did you DO?"},
        {dario, "They were going to shoot. You saw the guns, Ron. You saw them."},
        {dario, "...Wait. Rafael's moving."},
        {ron, "Oh God. They're getting up. The hands too - from the house!"},
    };
    return lines;
}

const std::vector<Line>& Story::claim_lines() {
    static const std::vector<Line> lines{
        {dario, "That's the last of them. The barn's ours now. Our hideout."},
        {dario, "Here - Garza's hunting rifle. He won't be needing it. Keep it clean, keep it close."},
        {ron, "We'll make this place hold. Sleep when you can - and mind the fuel. A pump that runs dry stays dry."},
        {dario, "The map's red, nevo. Everywhere you haven't been is theirs. Go and see who's left out there."},
    };
    return lines;
}

const Line& Story::free_line(int turn) {
    static const std::vector<Line> lines{
        {dario, "Espera's still the Wolves' town. Porto Luma, the Anchor holds the docks. Suda Haveno, the Frogs. Roka, the Iron Dogs. Gangs outlast governments."},
        {ron, "The Ĉefo got the last of his officers into the state prison. Walls built to keep people in."},
        {dario, "Stay out of Verda City unless you have to. That's where it started. Ground zero."},
        {ron, "The Brotherhood truckers still run the ring at night. They keep a camp in the city. Who's paying for their diesel, I'd like to know."},
        {dario, "That black tower downtown - no windows, no doors. Never saw a soul go in. Whatever this is, it came out of there."},
        {ron, "Families from the Esperanto club hid in the forest by Sovaĝejo. La Verda Stelo, they call themselves."},
        {dario, "Fuel's gold now. A pump runs dry, it stays dry. Siphon what you find."},
        {ron, "You eat anything today? ...Me neither."},
    };
    return lines[static_cast<std::size_t>(std::max(0, turn)) % lines.size()];
}

void Story::start(Stage stage) {
    *this = Story{};
    stage_ = stage;
    if (stage == Stage::Drive) { /* Ron is aboard already, or waiting by the pickup */ }
}

void Story::play(const std::vector<Line>& lines, bool timed) {
    lines_ = &lines;
    line_ = 0;
    timed_ = timed;
    clock_ = 0;
}

const Line* Story::line() const {
    if (!lines_ || line_ < 0 || line_ >= static_cast<int>(lines_->size())) return nullptr;
    return &(*lines_)[static_cast<std::size_t>(line_)];
}

bool Story::talk() {
    if (stage_ == Stage::WakeUp && line_ < 0 && wake_ >= wake_fade * .6F) { play(wake_lines(), false); return true; }
    if (line_ < 0 || !lines_) return false;
    ++line_;
    clock_ = 0;
    if (line_ < static_cast<int>(lines_->size())) return true;
    // The end of a talk: what it leads to.
    const auto* done = lines_;
    line_ = -1;
    lines_ = nullptr;
    if (done == &wake_lines() && stage_ == Stage::WakeUp) set(Stage::ToTruck);
    else if (done == &arrive_lines() && stage_ == Stage::Standoff && scene_ == 0) { scene_ = 1; after_ = 0; cue_ = Cue::Fire; }
    else if (done == &after_lines() && stage_ == Stage::Standoff && scene_ == 1) { scene_ = 2; cue_ = Cue::Rise; set(Stage::Clear); }
    return true;
}

void Story::boarded() {
    if (stage_ == Stage::ToTruck || stage_ == Stage::WakeUp) { line_ = -1; lines_ = nullptr; set(Stage::Drive); }
}

void Story::arrived() {
    if (stage_ != Stage::Drive) return;
    set(Stage::Standoff);
    scene_ = 0;
    play(arrive_lines(), true);
}

void Story::cleared() {
    if (stage_ != Stage::Clear) return;
    set(Stage::Free);
    cue_ = Cue::Claimed;
    play(claim_lines(), false);
}

void Story::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    if (stage_ == Stage::WakeUp) {
        wake_ += dt;
        if (line_ < 0 && wake_ >= wake_fade) play(wake_lines(), false);   // Ron starts talking as the eyes open
    }
    if (stage_ == Stage::Standoff && scene_ == 1 && line_ < 0) {
        after_ += dt;
        if (after_ >= shots_pause) play(after_lines(), true);
    }
    if (line_ >= 0 && timed_) {
        clock_ += dt;
        if (clock_ >= line_seconds) talk();
    }
}

float Story::fade() const {
    if (stage_ != Stage::WakeUp) return 0;
    return std::clamp(1 - wake_ / wake_fade, 0.0F, 1.0F);
}

std::string Story::objective(int infected_left) const {
    switch (stage_) {
        case Stage::WakeUp: return line_ >= 0 ? "Listen to Uncle Ron (USE)" : "";
        case Stage::ToTruck: return "Get in Uncle Ron's pickup on the drive";
        case Stage::Drive: return "Drive Uncle Ron to the barn at Ranĉo Verdaj Kampoj";
        case Stage::Standoff: return "";
        case Stage::Clear: return "Clear the farm: " + std::to_string(std::max(0, infected_left)) + " infected left";
        case Stage::Free: return "";
    }
    return "";
}

} // namespace outland::game::outbreak
