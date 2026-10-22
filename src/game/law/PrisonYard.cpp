#include "outland/game/law/PrisonYard.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::law {
namespace {
// The yard: between the admin block and the intake bay (z < -20) and the cell block (z > 8).
constexpr float yard_x0 = -36, yard_x1 = 36, yard_z0 = -17, yard_z1 = 5;
constexpr float tower_floor = 9.2F;
}

float PrisonYard::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / 16777216.0F;
}

Vector3 PrisonYard::yard_spot() {
    return {yard_x0 + (yard_x1 - yard_x0) * random(), 0, yard_z0 + (yard_z1 - yard_z0) * random()};
}

void PrisonYard::start(Vector3 centre, float yaw, std::uint64_t seed, const Chooser& choose) {
    people_.clear();
    snitch_ = -1;
    centre_ = centre;
    yaw_ = yaw;
    seed_ = seed;
    rng_ = (seed ^ 0x9e3779b97f4a7c15ULL) | 1;
    next_bot_ = first_bot;
    const auto guard = [&](Vector3 at, float facing) {
        PrisonPerson p;
        p.bot = next_bot_++;
        p.guard = true;
        p.character = choose ? choose(true, false, seed + static_cast<std::uint64_t>(p.bot)) : std::string();
        p.local = at;
        p.yaw = facing;
        p.target = at;
        people_.push_back(std::move(p));
        return &people_.back();
    };
    // The towers (looking into the yard), the gate (looking out), and two on the yard's beat.
    for (const float sx : {-1.0F, 1.0F})
        for (const float sz : {-1.0F, 1.0F})
            guard({sx * 40.2F, tower_floor, sz * 30.2F}, std::atan2(-sx, -sz) * RAD2DEG);
    for (const float sx : {-1.0F, 1.0F}) guard({sx * 3.6F, 0, -36.5F}, 180);
    guard({4.1F, 0, 10.4F}, 180);   // the desk inside the cell block's door
    for (const float z : {-12.0F, 0.0F}) {
        auto* p = guard({-30, 0, z}, 90);
        p->patrol_a = {-32, 0, z};
        p->patrol_b = {32, 0, z};
        p->target = p->patrol_b;
        p->walking = true;
    }
    // The yard's own: law breakers, and a few gangsters already doing time.
    for (int k = 0; k < 9; ++k) admit(k % 3 == 0, choose);
}

int PrisonYard::admit(bool gang, const Chooser& choose) {
    if (inmates() >= max_inmates) return -1;
    PrisonPerson p;
    p.bot = next_bot_++;
    p.gang = gang;
    p.character = choose ? choose(false, gang, seed_ + static_cast<std::uint64_t>(p.bot) * 31ULL) : std::string();
    p.local = yard_spot();
    p.yaw = random() * 360;
    p.target = p.local;
    p.wait = random() * 4;
    people_.push_back(std::move(p));
    return people_.back().bot;
}

int PrisonYard::mark_snitch() {
    if (snitch_ >= 0 && find(snitch_)) return snitch_;
    snitch_ = -1;
    std::vector<int> pool;
    for (const auto& p : people_) if (!p.guard && !p.gang) pool.push_back(p.bot);
    if (pool.empty()) for (const auto& p : people_) if (!p.guard) pool.push_back(p.bot);
    if (!pool.empty()) snitch_ = pool[static_cast<std::size_t>(random() * static_cast<float>(pool.size())) % pool.size()];
    return snitch_;
}

void PrisonYard::pause(int bot, float seconds) {
    for (auto& p : people_)
        if (p.bot == bot && !p.guard) {
            p.walking = false;
            p.wait = std::max(p.wait, seconds);
        }
}

bool PrisonYard::remove(int bot) {
    const auto before = people_.size();
    std::erase_if(people_, [&](const PrisonPerson& p) { return p.bot == bot && !p.guard; });
    if (bot == snitch_) snitch_ = -1;
    return people_.size() != before;
}

const PrisonPerson* PrisonYard::find(int bot) const {
    for (const auto& p : people_) if (p.bot == bot) return &p;
    return nullptr;
}

int PrisonYard::inmates() const {
    return static_cast<int>(std::count_if(people_.begin(), people_.end(), [](const PrisonPerson& p) { return !p.guard; }));
}

void PrisonYard::update(float dt) {
    if (!(dt > 0) || !std::isfinite(dt)) return;
    dt = std::min(dt, .25F);
    for (auto& p : people_) {
        const bool beat = p.guard && (p.patrol_a.x != 0 || p.patrol_b.x != 0);
        if (p.guard && !beat) continue;   // at a post
        if (!p.walking) {
            p.wait -= dt;
            if (p.wait > 0) continue;
            p.target = beat ? (Vector3Distance(p.local, p.patrol_a) < 1 ? p.patrol_b : p.patrol_a) : yard_spot();
            p.walking = true;
        }
        Vector3 to = Vector3Subtract(p.target, p.local);
        to.y = 0;
        const float left = Vector3Length(to);
        const float step = (beat ? patrol_speed : walk_speed) * dt;
        if (left <= step) {
            p.local = {p.target.x, p.local.y, p.target.z};
            p.walking = false;
            p.wait = beat ? 3 + random() * 2 : 2 + random() * 7;   // a guard looks round; an inmate loiters
            continue;
        }
        p.local = Vector3Add(p.local, Vector3Scale(to, step / left));
        p.yaw = std::atan2(to.x, to.z) * RAD2DEG;
    }
}

Vector3 PrisonYard::world(const PrisonPerson& p) const {
    const float a = yaw_ * DEG2RAD;
    return {centre_.x + p.local.x * std::cos(a) + p.local.z * std::sin(a), centre_.y + p.local.y + .12F,
            centre_.z - p.local.x * std::sin(a) + p.local.z * std::cos(a)};
}

bool PrisonYard::inside(Vector3 w) const {
    const float a = yaw_ * DEG2RAD, dx = w.x - centre_.x, dz = w.z - centre_.z;
    const float x = dx * std::cos(a) - dz * std::sin(a), z = dx * std::sin(a) + dz * std::cos(a);
    return std::abs(x) < 41.4F && std::abs(z) < 31.4F;
}

} // namespace outland::game::law
