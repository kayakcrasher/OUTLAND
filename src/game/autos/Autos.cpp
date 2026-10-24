#include "outland/game/autos/Autos.hpp"
#include "outland/world/WorldAsset.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace outland::game::autos {

bool valid(const Mods& m) {
    return m.paint >= -1 && m.paint < static_cast<int>(paints.size()) && m.engine >= 0 && m.engine < static_cast<int>(engines.size()) &&
           m.tires >= 0 && m.tires < static_cast<int>(tires.size()) && m.bumper >= 0 && m.bumper < static_cast<int>(bumpers.size()) &&
           m.plating >= 0 && m.plating < static_cast<int>(platings.size()) && m.gun >= 0 && m.gun < static_cast<int>(guns.size());
}

Tuning tuning(const Mods& m) {
    Tuning t;
    if (!valid(m)) return t;
    static constexpr std::array<float, 3> power{1, 1.3F, 1.65F}, top{1, 1.05F, 1.15F};
    static constexpr std::array<float, 4> wheel{.85F, 1, 1.25F, 1.6F}, slope{-2, 0, 6, 14}, grip{1.15F, 1, .95F, .85F};
    static constexpr std::array<float, 3> armour{1, .65F, .4F};
    t.power = power[static_cast<std::size_t>(m.engine)];
    t.top = top[static_cast<std::size_t>(m.engine)];
    t.wheel = wheel[static_cast<std::size_t>(m.tires)];
    t.slope = slope[static_cast<std::size_t>(m.tires)];
    t.grip = grip[static_cast<std::size_t>(m.tires)];
    static constexpr std::array<float, 3> plating{1, .5F, .25F};
    t.plating = plating[static_cast<std::size_t>(m.plating)];
    t.armour = armour[static_cast<std::size_t>(m.bumper)] * (m.plating > 0 ? .7F : 1.0F);
    t.gun = m.gun > 0;
    return t;
}

void apply(const Mods& m, world::VehiclePlacementState& car) {
    if (!valid(m)) return;
    const auto t = tuning(m);
    car.mods = {m.paint, m.engine, m.tires, m.bumper, m.plating, m.gun};
    car.power = t.power; car.top = t.top; car.wheel = t.wheel; car.armour = t.armour; car.grip = t.grip; car.slope = t.slope;
    car.plating = t.plating; car.gun = t.gun;
    car.painted = m.paint >= 0;
    if (car.painted) car.paint = paints[static_cast<std::size_t>(m.paint)].color;
}

Mods mods_of(const world::VehiclePlacementState& car) {
    Mods m{car.mods[0], car.mods[1], car.mods[2], car.mods[3], car.mods[4], car.mods[5]};
    return valid(m) ? m : Mods{};
}

std::string serialise(const Mods& m) {
    return std::to_string(m.paint) + ' ' + std::to_string(m.engine) + ' ' + std::to_string(m.tires) + ' ' + std::to_string(m.bumper) + ' ' +
           std::to_string(m.plating) + ' ' + std::to_string(m.gun);
}

bool parse(std::string_view text, Mods& out) {
    std::istringstream in{std::string(text)};
    Mods m;
    if (!(in >> m.paint >> m.engine >> m.tires >> m.bumper)) return false;
    // (Older saves stop at the bumper: no plating, no gun.)
    if (in >> m.plating) in >> m.gun;
    if (!valid(m)) return false;
    out = m;
    return true;
}

int repair_cost(const world::VehiclePlacementState& car) {
    float damage = (100 - car.health) + (100 - car.engine);
    for (const float t : car.tires) damage += (100 - t) * .25F;
    return static_cast<int>(std::ceil(std::max(0.0F, damage) * 6));
}

void repair(world::VehiclePlacementState& car) {
    car.health = car.engine = 100;
    car.tires = {100, 100, 100, 100};
    car.destroyed = false;
}

int fill_cost(float fuel) {
    return static_cast<int>(std::ceil((1 - std::clamp(fuel, 0.0F, 1.0F)) * tank_litres * static_cast<float>(litre_price)));
}

float fill(float fuel, std::int64_t& money) {
    fuel = std::clamp(fuel, 0.0F, 1.0F);
    const float litres = std::min((1 - fuel) * tank_litres, static_cast<float>(std::max<std::int64_t>(0, money)) / litre_price);
    const auto paid = static_cast<std::int64_t>(std::ceil(litres * static_cast<float>(litre_price)));
    money -= std::min(money, paid);
    return std::clamp(fuel + litres / tank_litres, 0.0F, 1.0F);
}

std::string serialise(const OwnedCar& c) {
    std::ostringstream out;
    out << c.definition << ' ' << serialise(c.mods) << ' ' << c.at.x << ' ' << c.at.y << ' ' << c.at.z << ' ' << c.yaw << ' ' << c.fuel;
    return out.str();
}

bool parse(std::string_view text, OwnedCar& out) {
    std::istringstream in{std::string(text)};
    OwnedCar c;
    std::vector<std::string> words;
    for (std::string w; in >> w;) words.push_back(w);
    // definition, the mods (4 in older saves; 6 with the depot's plating and roof gun), where, fuel.
    if (words.size() != 10 && words.size() != 12) return false;
    std::istringstream fields;
    {
        std::string joined;
        for (std::size_t i = 1; i < words.size(); ++i) joined += words[i] + ' ';
        fields.str(joined);
    }
    c.definition = words[0];
    if (!(fields >> c.mods.paint >> c.mods.engine >> c.mods.tires >> c.mods.bumper)) return false;
    if (words.size() == 12 && !(fields >> c.mods.plating >> c.mods.gun)) return false;
    if (!(fields >> c.at.x >> c.at.y >> c.at.z >> c.yaw >> c.fuel)) return false;
    if (c.definition.empty() || c.definition.size() > 64 || !valid(c.mods) || !std::isfinite(c.at.x) || !std::isfinite(c.at.y) ||
        !std::isfinite(c.at.z) || !std::isfinite(c.yaw) || !std::isfinite(c.fuel) || c.fuel < 0 || c.fuel > 1)
        return false;
    for (const char ch : c.definition) if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) return false;
    out = c;
    return true;
}

} // namespace outland::game::autos
