#include "outland/game/mafia/Territory.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace outland::game::mafia {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
}

float Territory::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / 16777216.0F;
}

void Territory::add_town(const std::string& id, const std::string& name, Vector3 centre, float radius, int gang, int crew) {
    Turf t;
    t.id = id; t.name = name; t.centre = centre; t.radius = radius;
    t.owner = std::clamp(gang, 0, gang_count - 1);
    t.crew = crew;
    turfs_.push_back(t);
}

void Territory::add_ranch(const std::string& id, const std::string& name, Vector3 centre, float radius, int ranch,
                          const std::string& family, int county_gang, std::uint64_t seed) {
    if (ranch < 0 || ranch >= family_count) return;
    Turf t;
    t.id = id; t.name = name; t.centre = centre; t.radius = radius;
    t.ranch = ranch;
    t.owner = family_faction(ranch);
    turfs_.push_back(t);
    // Who they ride with: two allied with their county's gang, two fiercely independent, one that
    // will deal (which is which turns with the island's seed).
    auto& f = families_[static_cast<std::size_t>(ranch)];
    f.name = family;
    switch ((ranch + static_cast<int>(seed % 5)) % 5) {
        case 0: case 2: f.stance = Stance::Allied; f.gang = county_gang; f.fierce = false; break;
        case 1: case 4: f.stance = Stance::Independent; f.gang = -1; f.fierce = true; break;
        default: f.stance = Stance::Independent; f.gang = -1; f.fierce = false; break;
    }
}

int Territory::turf_at(Vector3 p) const {
    int best = -1;
    float nearest = 1e9F;
    for (std::size_t i = 0; i < turfs_.size(); ++i) {
        const float d = flat(p, turfs_[i].centre);
        if (d < turfs_[i].radius && d < nearest) { nearest = d; best = static_cast<int>(i); }
    }
    return best;
}

void Territory::flip(int index, int to) {
    auto& t = turfs_[static_cast<std::size_t>(index)];
    if (to < 0 || to == t.owner) return;
    flips_.push_back({index, t.owner, to});
    t.owner = to;
    t.hold = 50;
    t.pressure.fill(0);
}

void Territory::wear(Turf& t, int by, float amount) {
    if (t.ranch >= 0 || t.cleared || by < 0 || by >= gang_count || by == t.owner || amount <= 0) return;
    t.hold -= amount;
    t.pressure[static_cast<std::size_t>(by)] += amount;
    if (t.hold > 0) return;
    // The town goes to whoever pushed hardest.
    int to = by;
    for (int g = 0; g < gang_count; ++g)
        if (g != t.owner && t.pressure[static_cast<std::size_t>(g)] > t.pressure[static_cast<std::size_t>(to)]) to = g;
    flip(static_cast<int>(&t - turfs_.data()), to);
}

void Territory::killed(int victim, Vector3 where, int by) {
    const int index = turf_at(where);
    if (index < 0) return;
    auto& t = turfs_[static_cast<std::size_t>(index)];
    if (victim == t.owner) wear(t, by, kill_wear);
}

int Territory::tag(Vector3 position, Vector3 normal, int gang) {
    if (gang < 0 || gang >= gang_count) return -1;
    // Paint over whatever is there; re-spraying your own tag changes nothing.
    bool own_there = false;
    std::erase_if(tags_, [&](const Tag& t) {
        if (Vector3Distance(t.position, position) >= tag_spacing) return false;
        own_there |= t.gang == gang;
        return true;
    });
    const int index = turf_at(position);
    tags_.push_back({position, normal, gang, index});
    if (index < 0 || own_there) return index;
    auto& t = turfs_[static_cast<std::size_t>(index)];
    if (t.ranch >= 0) return index;
    if (t.owner == gang) t.hold = std::min(100.0F, t.hold + own_tag);
    else wear(t, gang, tag_wear);
    return index;
}

void Territory::delivered(int index, int gang) {
    if (index < 0 || static_cast<std::size_t>(index) >= turfs_.size()) return;
    auto& t = turfs_[static_cast<std::size_t>(index)];
    if (t.owner == gang) t.hold = std::min(100.0F, t.hold + delivery);
}

void Territory::defended(int index, int gang) {
    if (index < 0 || static_cast<std::size_t>(index) >= turfs_.size()) return;
    auto& t = turfs_[static_cast<std::size_t>(index)];
    if (t.owner == gang && !t.cleared && t.ranch < 0) t.hold = std::min(100.0F, t.hold + defence);
}

void Territory::clear_town(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= turfs_.size()) return;
    auto& t = turfs_[static_cast<std::size_t>(index)];
    if (t.ranch >= 0) return;
    t.cleared = true;
    t.pressure.fill(0);
}

void Territory::take_island(int gang) {
    if (gang < 0 || gang >= gang_count) return;
    for (std::size_t i = 0; i < turfs_.size(); ++i) {
        auto& t = turfs_[i];
        if (t.ranch >= 0) continue;
        if (t.owner != gang) flips_.push_back({static_cast<int>(i), t.owner, gang});
        t.owner = gang; t.hold = 100; t.cleared = false; t.pressure.fill(0);
    }
    for (auto& f : families_) if (!f.name.empty()) { f.stance = Stance::Allied; f.gang = gang; }
}

Balance Territory::balance(int player_gang) const {
    int towns = 0, clean = 0, ours = 0;
    for (const auto& t : turfs_) {
        if (t.ranch >= 0) continue;
        ++towns;
        if (t.cleared) ++clean;
        else if (player_gang >= 0 && t.owner == player_gang) ++ours;
    }
    if (towns == 0) return {};
    return {static_cast<float>(clean) / static_cast<float>(towns), static_cast<float>(ours) / static_cast<float>(towns)};
}

int Territory::cleared() const {
    return static_cast<int>(std::count_if(turfs_.begin(), turfs_.end(), [](const Turf& t) { return t.cleared; }));
}

bool Territory::extort(int ranch, int gang) {
    if (ranch < 0 || ranch >= family_count || gang < 0) return false;
    auto& f = families_[static_cast<std::size_t>(ranch)];
    if (f.stance != Stance::Independent && f.gang == gang) return true;   // already yours
    if (f.fierce || f.stance == Stance::Allied) return false;             // they fight (or their gang does)
    f.stance = Stance::Extorted;
    f.gang = gang;
    return true;
}

void Territory::subdued(int ranch, int gang) {
    if (ranch < 0 || ranch >= family_count || gang < 0) return;
    auto& f = families_[static_cast<std::size_t>(ranch)];
    f.stance = Stance::Extorted;
    f.gang = gang;
}

void Territory::ally(int ranch, int gang) {
    if (ranch < 0 || ranch >= family_count || gang < 0) return;
    auto& f = families_[static_cast<std::size_t>(ranch)];
    f.stance = Stance::Allied;
    f.gang = gang;
}

Raid Territory::update(float dt, double now, int player_gang, float player_regain, float calm) {
    Raid raid;
    if (!std::isfinite(dt) || dt <= 0 || turfs_.empty()) return raid;
    for (auto& t : turfs_) {
        if (t.ranch >= 0 || t.cleared) continue;
        t.hold = std::min(100.0F, t.hold + regain_per_second * dt * (t.owner == player_gang ? player_regain : 1.0F));
        for (auto& p : t.pressure) p *= std::exp(-dt / 900.0F);
    }
    if (next_raid_ < 0) next_raid_ = now + raid_every * .6;
    if (now < next_raid_) return raid;
    next_raid_ = now + raid_every * (.7 + .6 * random()) * (1 + 3 * std::clamp(calm, 0.0F, 1.0F));
    // An AI gang with a town pushes into the nearest town held by someone else.
    std::vector<int> attackers;
    for (int g = 0; g < gang_count; ++g)
        if (g != player_gang && towns_of(g) > 0) attackers.push_back(g);
    if (attackers.empty()) return raid;
    const int attacker = attackers[std::min(attackers.size() - 1, static_cast<std::size_t>(random() * static_cast<float>(attackers.size())))];
    float best = 1e9F;
    for (std::size_t i = 0; i < turfs_.size(); ++i) {
        const auto& target = turfs_[i];
        if (target.ranch >= 0 || target.cleared || target.owner == attacker) continue;
        for (const auto& home : turfs_) {
            if (home.ranch >= 0 || home.cleared || home.owner != attacker) continue;
            const float d = flat(home.centre, target.centre) * (.8F + .4F * random());
            if (d < best) { best = d; raid = {attacker, static_cast<int>(i)}; }
        }
    }
    if (raid.turf >= 0) wear(turfs_[static_cast<std::size_t>(raid.turf)], attacker, raid_wear);
    return raid;
}

int Territory::towns_of(int faction) const {
    return static_cast<int>(std::count_if(turfs_.begin(), turfs_.end(), [&](const Turf& t) { return t.ranch < 0 && !t.cleared && t.owner == faction; }));
}

int Territory::ranches_with(int gang) const {
    return static_cast<int>(std::count_if(families_.begin(), families_.end(),
        [&](const Family& f) { return f.stance != Stance::Independent && f.gang == gang && !f.name.empty(); }));
}

bool Territory::controls(int index, int gang) const {
    if (index < 0 || static_cast<std::size_t>(index) >= turfs_.size()) return false;
    const auto& t = turfs_[static_cast<std::size_t>(index)];
    if (t.cleared) return false;
    if (t.ranch < 0) return t.owner == gang;
    const auto& f = families_[static_cast<std::size_t>(t.ranch)];
    return f.stance != Stance::Independent && f.gang == gang;
}

bool Territory::rules_island(int gang) const {
    if (turfs_.empty() || gang < 0) return false;
    for (std::size_t i = 0; i < turfs_.size(); ++i)
        if (!turfs_[i].cleared && !controls(static_cast<int>(i), gang)) return false;
    return true;
}

std::vector<int> Territory::ranking() const {
    std::vector<int> out;
    for (int f = 0; f < faction_count; ++f)
        if (f < gang_count || !families_[static_cast<std::size_t>(f - gang_count)].name.empty()) out.push_back(f);
    const auto score = [&](int f) {
        if (is_family(f)) {
            const auto& family = families_[static_cast<std::size_t>(f - gang_count)];
            return family.stance == Stance::Independent ? 1.0F : .5F;   // their own ranch (shared when tied to a gang)
        }
        return static_cast<float>(towns_of(f)) + .5F * static_cast<float>(ranches_with(f));
    };
    std::stable_sort(out.begin(), out.end(), [&](int a, int b) { return score(a) > score(b); });
    return out;
}

std::string Territory::serialise() const {
    std::ostringstream out;
    out << "territory 2\n";
    for (const auto& t : turfs_) out << "turf " << t.id << ' ' << t.owner << ' ' << t.hold << ' ' << (t.cleared ? 1 : 0) << '\n';
    for (std::size_t i = 0; i < families_.size(); ++i)
        if (!families_[i].name.empty())
            out << "family " << i << ' ' << static_cast<int>(families_[i].stance) << ' ' << families_[i].gang << '\n';
    for (const auto& t : tags_)
        out << "tag " << t.position.x << ' ' << t.position.y << ' ' << t.position.z << ' ' << t.normal.x << ' ' << t.normal.y << ' '
            << t.normal.z << ' ' << t.gang << '\n';
    return out.str();
}

bool Territory::parse(const std::string& text) {
    std::istringstream in(text);
    std::string word;
    int version = 0;
    if (!(in >> word >> version) || word != "territory" || version < 1 || version > 2) return false;
    auto turfs = turfs_;
    auto families = families_;
    std::vector<Tag> tags;
    while (in >> word) {
        if (word == "turf") {
            std::string id; int owner = 0, cleared = 0; float hold = 0;
            if (!(in >> id >> owner >> hold) || owner < 0 || owner >= faction_count || !std::isfinite(hold)) return false;
            if (version >= 2 && !(in >> cleared)) return false;
            for (auto& t : turfs)
                if (t.id == id && (t.ranch >= 0) == is_family(owner)) {
                    t.owner = owner; t.hold = std::clamp(hold, 1.0F, 100.0F); t.cleared = cleared != 0 && t.ranch < 0;
                }
        } else if (word == "family") {
            std::size_t i = 0; int stance = 0, gang = -1;
            if (!(in >> i >> stance >> gang) || i >= families.size() || stance < 0 || stance > 2 || gang < -1 || gang >= gang_count) return false;
            families[i].stance = static_cast<Stance>(stance);
            families[i].gang = families[i].stance == Stance::Independent ? -1 : gang;
        } else if (word == "tag") {
            Tag t;
            if (!(in >> t.position.x >> t.position.y >> t.position.z >> t.normal.x >> t.normal.y >> t.normal.z >> t.gang) ||
                t.gang < 0 || t.gang >= gang_count)
                return false;
            tags.push_back(t);
        } else {
            return false;
        }
    }
    turfs_ = std::move(turfs);
    families_ = families;
    tags_ = std::move(tags);
    for (auto& t : tags_) t.turf = turf_at(t.position);
    return true;
}

} // namespace outland::game::mafia
