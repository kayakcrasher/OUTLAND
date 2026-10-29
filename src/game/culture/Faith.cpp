#include "outland/game/culture/Faith.hpp"
#include <cmath>

namespace outland::game::culture {

namespace {
constexpr std::array<const char*, 4> baptist_saints{"PETRO", "ANDREO", "PATRIKO", "KOLUMBO"};
constexpr std::array<const char*, 4> orthodox_saints{"NIKOLAO", "SERGIO", "CIRILO KAJ METODIO", "SOFIA"};
} // namespace

const char* denomination_name(Denomination d) { return d == Denomination::Baptist ? "BAPTISTA" : "ORTODOKSA"; }

std::vector<Church> find_churches(const world::VerdaRegion& region) {
    std::vector<Church> out;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            const bool baptist = asset.model_path.find("church_baptist") != std::string::npos;
            if (!baptist && asset.model_path.find("church_orthodox") == std::string::npos) continue;
            Church c;
            c.denomination = baptist ? Denomination::Baptist : Denomination::Orthodox;
            c.centre = asset.position;
            c.yaw = asset.rotation_y;
            // (The world kit keeps its pieces in one settlement's list: the town is the nearest one.)
            float best = 1e30F;
            for (const auto& other : region.settlements()) {
                if (other.id.starts_with("ranch_")) continue;
                const float d = std::hypot(other.center.x - asset.position.x, other.center.z - asset.position.z);
                if (d < best) { best = d; c.settlement = other.id; }
            }
            if (c.settlement.empty()) c.settlement = town.id;
            // The model's front (-Z) to the street: the door is just outside the porch.
            const float a = asset.rotation_y * DEG2RAD, reach = baptist ? 12.4F : 11.8F;
            c.door = {asset.position.x - std::sin(a) * reach, asset.position.y, asset.position.z - std::cos(a) * reach};
            const auto seed = static_cast<std::uint32_t>(std::abs(static_cast<int>(asset.position.x) * 31 + static_cast<int>(asset.position.z) * 17));
            c.saint = baptist ? baptist_saints[seed % baptist_saints.size()] : orthodox_saints[seed % orthodox_saints.size()];
            c.green_hand = baptist && c.settlement == "capital_verda";
            out.push_back(c);
        }
    return out;
}

int feast_on(int day) {
    if (day < 0 || day % feast_days != 0) return -1;
    return (day / feast_days) % static_cast<int>(feasts.size());
}

int next_feast(int day, int& in_days) {
    const int from = std::max(0, day + 1);
    const int next = (from + feast_days - 1) / feast_days * feast_days;
    in_days = next - day;
    return feast_on(next);
}

std::string church_notice(const Church& c, int day) {
    const bool baptist = c.denomination == Denomination::Baptist;
    std::string text = std::string(denomination_name(c.denomination)) + " PREĜEJO DE SANKTA " + c.saint + ". ";
    text += baptist ? "Verdan Baptists: they keep the saints and the same feasts as the Orthodox, and read the Esperanto King James. "
                    : "Esperanto Orthodox: icons, candles and the liturgy sung in Esperanto; the faithful stand. ";
    if (c.green_hand) text += "La Verda Mano worships here. ";
    if (const int today = feast_on(day); today >= 0) {
        text += std::string("Today: ") + feasts[static_cast<std::size_t>(today)].name + " - " + feasts[static_cast<std::size_t>(today)].what + ".";
    } else {
        int in = 0;
        const int next = next_feast(day, in);
        text += std::string("Next feast: ") + feasts[static_cast<std::size_t>(next)].name + (in == 1 ? " tomorrow." : " in " + std::to_string(in) + " days.");
    }
    return text;
}

bool bells(bool sunday, int day, int minute) {
    if (sunday && ((minute >= 9 * 60 + 10 && minute < 9 * 60 + 30) || (minute >= 11 * 60 + 30 && minute < 11 * 60 + 45))) return true;
    const int feast = feast_on(day);
    if (feast >= 0 && minute >= 12 * 60 && minute < 12 * 60 + 20) return true;
    return feast == 2 && minute < 30;   // Pasko: the bells at midnight
}

Gathering gathering(const Church& church, bool sunday, int day, int minute) {
    Gathering g;
    const int feast = feast_on(day);
    if (feast == 2 && minute < 90) { g.people = 14; g.candles = true; }                                // Pasko night
    else if (feast >= 0 && minute >= 10 * 60 && minute < 18 * 60) g.people = feast == 1 && church.denomination == Denomination::Orthodox ? 16 : 12;
    else if (sunday && minute >= 9 * 60 + 15 && minute < 11 * 60 + 45) g.people = 8;
    if (g.people > 0 && church.green_hand && (sunday || feast >= 0)) g.green_hand = std::min(4, g.people / 2);
    return g;
}

std::string feast_morning(int day) {
    const int feast = feast_on(day);
    if (feast < 0) return "";
    return std::string("TODAY: ") + feasts[static_cast<std::size_t>(feast)].name + " - " + feasts[static_cast<std::size_t>(feast)].what +
           ". The churches are full and the gangs keep the peace";
}

} // namespace outland::game::culture
