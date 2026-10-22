#include "outland/game/career/Callouts.hpp"
#include "outland/game/ai/BattleRoyaleBots.hpp"
#include "outland/game/autos/Autos.hpp"
#include "outland/game/mafia/Mafio.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/physics/WorldCollision.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::career {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
std::uint64_t mix(std::uint64_t z) {
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
// The getaway cars: what the radio calls them.
struct Getaway { const char* definition; const char* name; };
constexpr Getaway getaways[]{{"sedan", "Sedano"}, {"hatchback", "hatchback"}, {"truck", "pickup"}};
std::string lower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
}

const char* Callouts::shirts(int gang) {
    switch (gang) {
        case 0: return "white shirts, the green hand";
        case 1: return "grey shirts";
        case 2: return "gold shirts";
        case 3: return "teal shirts";
        case 4: return "rust-red shirts";
        default: return "dark clothes";
    }
}

std::string Callouts::make_plate(std::uint64_t seed) {
    const auto h = mix(seed);
    constexpr const char* letters = "ABCDEFGHJKLMNPRSTUVZ";
    std::string plate = "VRD-";
    plate += std::to_string(100 + h % 900);
    plate += letters[(h >> 20) % 20];
    return plate;
}

bool Callouts::escorting(int bot) const {
    return std::any_of(escorts_.begin(), escorts_.end(), [&](const Escort& e) { return e.bot == bot; });
}

bool Callouts::suspect(int bot) const { return std::find(suspects_.begin(), suspects_.end(), bot) != suspects_.end(); }

void Callouts::hint(const std::string& text) {
    if (!first_) return;
    // What's happening now matters more than the rest of the lesson.
    lesson_.assign({{"Officer Novak", text}});
    line_time_ = 0;
}

bool Callouts::dispatch(const Frame& frame, const std::vector<CallScene>& scenes, Career& career, mafia::Mafio& mafio,
                        ai::BattleRoyaleBots& bots, vehicles::VehicleSystem& cars, world::VerdaRegion& region, std::uint64_t seed) {
    if (active_ || frame.now < next_call_ || !frame.alive || !mafio.active()) return false;
    next_call_ = frame.now + 8;   // not every frame, if nothing fits
    // Scenes a drive away, not on the doorstep.
    std::vector<const CallScene*> pool;
    for (const auto& s : scenes) {
        const float d = flat(s.door, frame.officer);
        if (d >= scene_near && d <= scene_far) pool.push_back(&s);
    }
    if (pool.empty()) return false;
    auto h = mix(seed ^ static_cast<std::uint64_t>(frame.now * 1000));
    for (int attempt = 0; attempt < 6 && !pool.empty(); ++attempt) {
        h = mix(h);
        const std::size_t pick = h % pool.size();
        const CallScene& scene = *pool[pick];
        pool.erase(pool.begin() + static_cast<std::ptrdiff_t>(pick));
        // The nearest crew still standing does it (not Verda City's casino crew: the Chief's to say when).
        int crew = -1;
        float best = crew_reach;
        for (std::size_t c = 0; c < mafio.crews().size(); ++c) {
            const auto& k = mafio.crews()[c];
            if (k.disbanded || k.casino || mafio.standing(static_cast<int>(c), bots) < 3) continue;
            const float d = flat(k.door, scene.door);
            if (d < best) { best = d; crew = static_cast<int>(c); }
        }
        if (crew < 0) continue;
        // The getaway car at the kerb, in a colour of its own.
        const auto& ride = getaways[(h >> 8) % std::size(getaways)];
        const Vector3 kerb = Vector3Add(scene.door, Vector3Scale(scene.out, 6));
        const float along = std::atan2(scene.out.x, scene.out.z) * RAD2DEG + 90;
        if (!cars.spawn(region, kerb, along, ride.definition)) continue;
        const std::string car = cars.last_spawned();
        const std::size_t colour = 1 + (h >> 16) % (autos::paints.size() - 1);
        if (auto* a = cars.asset(region, car)) {
            autos::apply({static_cast<int>(colour), 0, 1, 0}, a->vehicle);
            a->vehicle.marker = "getaway_" + car;   // nobody else's to drive off in
        }
        const Vector3 at = Vector3Add(scene.door, Vector3Scale(scene.out, 2.5F));
        const auto kind = scene.shop ? mafia::HeistKind::Robbery : mafia::HeistKind::DriveBy;
        if (!mafio.stage_heist(kind, crew, at, car, bots, frame.now)) { cars.remove(region, car); continue; }
        const int gang = mafio.crews()[static_cast<std::size_t>(crew)].gang;
        active_ = true;
        car_ = car;
        gang_ = gang;
        plate_ = make_plate(h);
        look_ = lower(autos::paints[colour].name) + " " + ride.name;
        suspects_.clear();
        for (const auto& heist : mafio.heists()) if (heist.car == car) suspects_ = heist.squad;
        const std::string gang_name(mafia::gangs[static_cast<std::size_t>(gang)].name);
        Job job;
        job.title = scene.shop ? "ROBBERY" : "DRIVE-BY";
        job.gang = gang;
        job.crew = crew;
        job.target = scene.door;
        job.needed = static_cast<int>(suspects_.size());
        job.detail = (scene.shop ? "Robbery at " + scene.place + " in " + scene.town : "Shots fired at a house in " + scene.town) + ": " +
                     std::to_string(job.needed) + " " + gang_name + " (" + shirts(gang) + "), a " + look_ + ", plate " + plate_;
        job.pay = scene.shop ? 650 : 600;
        job.respect = 10;
        job.deadline = frame.now + 900;
        first_ = career.state().callouts == 0;
        if (!career.callout(job)) {
            for (const int id : suspects_) mafio.settle(id);
            end(cars, region);
            return false;
        }
        next_call_ = frame.now + call_gap;
        lesson_.clear();
        line_time_ = 0;
        hinted_hands_ = hinted_cuffed_ = hinted_aboard_ = false;
        if (first_) {
            partner_wanted_ = true;
            const std::string what = scene.shop ? "a robbery at " + scene.place + " in " + scene.town : "shots fired at a house in " + scene.town;
            lesson_ = {
                {"Officer Novak", "Our first call, partner - " + what + ". Two " + gang_name + ". Let's go."},
                {"Officer Novak", "Look for the shirts: the " + gang_name + " wear " + shirts(gang) + ". And the car - a " + look_ + ", plate " + plate_ + "."},
                {"Officer Novak", "It's on the compass. When they see us coming they'll run for it."},
                {"Officer Novak", "Get on their bumper with the lights on (L) and stay there - they'll pull over."},
                {"Officer Novak", "Out of the car, most of them put their hands up. Walk up and USE to cuff them."},
                {"Officer Novak", "One runs? Chase him down - catch him and he gives up. Don't shoot unless they do."},
                {"Officer Novak", "Cuffed, they walk with you. Get in the car with them close and they climb in the back."},
                {"Officer Novak", "Then the state prison: drive up to the gate and the guards take them. Easy."},
            };
        }
        return true;
    }
    return false;
}

void Callouts::end(vehicles::VehicleSystem& cars, world::VerdaRegion& region) {
    if (!car_.empty()) {
        if (!stale_car_.empty()) cars.remove(region, stale_car_);
        stale_car_ = car_;   // towed away once the officer is out of sight of it
    }
    active_ = false;
    car_.clear();
    suspects_.clear();
}

int Callouts::to_cuff(Vector3 feet, const mafia::Mafio& mafio, ai::BattleRoyaleBots& bots) const {
    int best = -1;
    float nearest = cuff_reach;
    for (const auto& heist : mafio.heists())
        for (const int id : heist.surrendered) {
            if (escorting(id)) continue;
            const auto* bot = bots.find(id);
            if (!bot || !bot->alive() || std::abs(bot->position().y - feet.y) > 1.8F) continue;
            const float d = flat(bot->position(), feet);
            if (d < nearest) { nearest = d; best = id; }
        }
    return best;
}

bool Callouts::cuff(int bot, int gang, mafia::Mafio& mafio, ai::BattleRoyaleBots& bots) {
    if (bot < 0 || escorting(bot)) return false;
    mafio.hold(bot, true, bots);
    escorts_.push_back({bot, gang, suspect(bot), false});
    if (!hinted_cuffed_) { hinted_cuffed_ = true; hint("Cuffed. Walk him to the car - he'll keep up. Get in and he climbs in the back."); }
    return true;
}

void Callouts::update(const Frame& frame, Career& career, mafia::Mafio& mafio, ai::BattleRoyaleBots& bots,
                      vehicles::VehicleSystem& cars, world::VerdaRegion& region, std::string& said) {
    // The partner's lesson, a line at a time.
    if (!lesson_.empty() && (line_time_ += frame.dt) >= line_seconds) { lesson_.erase(lesson_.begin()); line_time_ = 0; }
    // An old getaway car is towed once nobody is looking.
    if (!stale_car_.empty()) {
        const int v = cars.find(stale_car_);
        if (v < 0) stale_car_.clear();
        else if (flat(cars.vehicles()[static_cast<std::size_t>(v)].position, frame.officer) > 150) { cars.remove(region, stale_car_); stale_car_.clear(); }
    }
    if (!career.on_duty() || !frame.alive) {
        // Off the force or down: the cuffed are let go, the call is off.
        for (const auto& e : escorts_) mafio.hold(e.bot, false, bots);
        escorts_.clear();
        if (active_) end(cars, region);
        return;
    }

    // The cuffed walk behind the officer, in a line; with the officer at the wheel of a police car
    // close by, they get in the back.
    const Vector3 behind{-std::sin(frame.yaw), 0, -std::cos(frame.yaw)};   // (the player faces sin, cos)
    for (std::size_t i = 0; i < escorts_.size();) {
        auto& e = escorts_[i];
        auto* bot = bots.find(e.bot);
        if (!bot || !bot->alive()) {
            if (e.suspect) career.suspect_down(false);
            std::erase(suspects_, e.bot);
            mafio.settle(e.bot);
            escorts_.erase(escorts_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        e.walking = false;
        if (!frame.police_car.empty() && flat(bot->position(), frame.car) < board_reach) {
            if (career.arrested(e.gang)) {
                if (e.suspect) career.suspect_down(true);
                std::erase(suspects_, e.bot);
                mafio.settle(e.bot);
                bots.retire(e.bot);
                bots.set_hidden(e.bot, true);
                escorts_.erase(escorts_.begin() + static_cast<std::ptrdiff_t>(i));
                if (escorts_.empty() && !hinted_aboard_) {
                    hinted_aboard_ = true;
                    hint("They're in the back. Now the state prison - it's on the compass. Up to the gate.");
                }
                said = "In the back of the car: " + std::to_string(career.custody()) + " in custody - drive to the state prison";
                continue;
            }
            said = career.message();
        }
        const float d = flat(bot->position(), frame.officer);
        if (d > escort_lost) {
            mafio.hold(e.bot, false, bots);
            said = "Your prisoner slipped away";
            escorts_.erase(escorts_.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (frame.police_car.empty() && d > 1.2F) {
            const Vector3 goal = Vector3Add(frame.officer, Vector3Scale(behind, escort_gap * static_cast<float>(i + 1)));
            Vector3 to{goal.x - bot->position().x, 0, goal.z - bot->position().z};
            const float gap = Vector3Length(to);
            if (gap > .3F) {
                const float step = std::min(gap, (gap > 6 ? 4.2F : 2.4F) * frame.dt);
                Vector3 p = Vector3Add(bot->position(), Vector3Scale(Vector3Normalize(to), step));
                // Far behind (the officer drove off a little way): he catches up out of the way.
                if (gap > 40) p = goal;
                const float terrain = world::terrain::TerrainHeight::sample(p.x, p.z);
                const float probe = std::max(terrain, frame.officer.y) + 1.2F;
                p.y = world::physics::WorldCollision::ground_height({p.x, probe, p.z}, probe, region);
                bot->set_position(p);
                bot->set_yaw(std::atan2(to.x, to.z) * RAD2DEG);
                e.walking = true;
            }
        }
        ++i;
    }

    if (!active_) return;
    if (career.job().kind != JobKind::Callout) {
        // Done (or failed, or dropped): what's left of the heist goes home.
        end(cars, region);
        return;
    }
    // Suspects shot dead count as stopped.
    for (std::size_t i = 0; i < suspects_.size();) {
        const auto* bot = bots.find(suspects_[i]);
        if ((!bot || !bot->alive()) && !escorting(suspects_[i])) {
            mafio.settle(suspects_[i]);
            suspects_.erase(suspects_.begin() + static_cast<std::ptrdiff_t>(i));
            career.suspect_down(false);
            if (career.job().kind != JobKind::Callout) { end(cars, region); return; }
            continue;
        }
        ++i;
    }
    // Follow the heist: the scene, the fleeing car, the men on foot.
    const mafia::Heist* heist = nullptr;
    for (const auto& h : mafio.heists()) if (h.car == car_) heist = &h;
    if (!heist) {
        if (suspects_.empty()) return;
        career.callout_failed(career.job().title + ": the suspects got away");
        end(cars, region);
        return;
    }
    if (heist->escaped) {
        career.callout_failed(career.job().title + ": they made it home - the suspects got away");
        end(cars, region);
        return;
    }
    Vector3 target = career.job().target;
    const int v = cars.find(car_);
    if (heist->stage == mafia::HeistStage::Fleeing && v >= 0) target = cars.vehicles()[static_cast<std::size_t>(v)].position;
    else if (!heist->squad.empty()) {
        const int lead = [&] { for (const int id : heist->squad) if (!escorting(id)) return id; return -1; }();
        if (const auto* bot = lead >= 0 ? bots.find(lead) : nullptr) target = bot->position();
    }
    career.track(target);
    if (!hinted_hands_ && !heist->surrendered.empty()) {
        hinted_hands_ = true;
        said = "Hands up! A suspect gave up - walk up and USE to cuff him";
        hint("Hands up - good. Walk up to him and USE to cuff him.");
    }
}

} // namespace outland::game::career
