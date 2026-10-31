#include "outland/game/player/PlayerStats.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace outland::game::player {

void PlayerStats::drove(float metres, float speed) {
    if (std::isfinite(metres) && metres > 0 && metres < teleport) driven_ += metres;
    // (A top speed from a glitch - a car flung by a crash - would stand forever: within reason.)
    if (std::isfinite(speed) && speed < 120) top_ = std::max(top_, std::abs(speed));
}

void PlayerStats::walked(float metres) {
    if (std::isfinite(metres) && metres > 0 && metres < teleport) walked_ += metres;
}

void PlayerStats::played(float seconds) {
    if (std::isfinite(seconds) && seconds > 0 && seconds < 5) seconds_ += seconds;
}

void PlayerStats::money(std::int64_t before, std::int64_t after) {
    if (after > before) earned_ += after - before;
    else spent_ += before - after;
}

std::vector<StatLine> PlayerStats::lines() const {
    const auto number = [](double value, int decimals) {
        char text[32];
        std::snprintf(text, sizeof text, "%.*f", decimals, value);
        return std::string(text);
    };
    const auto count = [](std::int64_t value) { return std::to_string(value); };
    const int minutes = static_cast<int>(seconds_ / 60);
    return {
        {"KM DRIVEN", number(km_driven(), 1)},
        {"KM WALKED", number(km_walked(), 1)},
        {"TOP SPEED", number(top_kmh(), 0) + " km/h"},
        {"KILLS", count(kills_)},
        {"DEATHS", count(deaths_)},
        {"ARRESTS MADE", count(arrests_)},
        {"TIMES BUSTED", count(busted_)},
        {"LOADS HAULED", count(loads_)},
        {"RACES WON", count(races_)},
        {"EARNED", count(earned_) + " kr"},
        {"SPENT", count(spent_) + " kr"},
        {"TIME PLAYED", std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min"},
    };
}

std::string PlayerStats::serialise() const {
    std::ostringstream out;
    out.precision(12);
    out << "stats 1 driven=" << driven_ << " walked=" << walked_ << " seconds=" << seconds_ << " top=" << top_
        << " kills=" << kills_ << " deaths=" << deaths_ << " arrests=" << arrests_ << " busted=" << busted_
        << " loads=" << loads_ << " races=" << races_ << " earned=" << earned_ << " spent=" << spent_ << '\n';
    return out.str();
}

bool PlayerStats::parse(const std::string& text) {
    std::istringstream in(text);
    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "stats" || version < 1) return false;
    PlayerStats s;
    std::string field;
    while (in >> field) {
        const auto eq = field.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = field.substr(0, eq);
        const double value = std::strtod(field.c_str() + eq + 1, nullptr);
        if (!std::isfinite(value) || value < 0) continue;
        const auto whole = [&] { return static_cast<int>(std::min(value, 2e9)); };
        if (key == "driven") s.driven_ = value;
        else if (key == "walked") s.walked_ = value;
        else if (key == "seconds") s.seconds_ = value;
        else if (key == "top") s.top_ = static_cast<float>(std::min(value, 120.0));
        else if (key == "kills") s.kills_ = whole();
        else if (key == "deaths") s.deaths_ = whole();
        else if (key == "arrests") s.arrests_ = whole();
        else if (key == "busted") s.busted_ = whole();
        else if (key == "loads") s.loads_ = whole();
        else if (key == "races") s.races_ = whole();
        else if (key == "earned") s.earned_ = static_cast<std::int64_t>(value);
        else if (key == "spent") s.spent_ = static_cast<std::int64_t>(value);
    }
    *this = s;
    return true;
}

} // namespace outland::game::player
