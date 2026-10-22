#include "outland/game/transit/BusLine.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/roads/Turnaround.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::transit {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
bool is_shelter(const std::string& path) {
    return path.ends_with("transit/bus_stop.glb") || path.ends_with("transit/bus_stop_alt.glb");
}
}

float BusLine::wrap(float distance) const {
    if (loop_.length <= 0) return 0;
    distance = std::fmod(distance, loop_.length);
    return distance < 0 ? distance + loop_.length : distance;
}

bool BusLine::build(const world::VerdaRegion& region, const vehicles::VehicleSystem& vehicles,
                    const characters::CharacterRegistry* characters, std::uint64_t seed) {
    stops_.clear(); buses_.clear(); loop_ = {}; running_ = false; turnarounds_ = 0;
    const world::roads::RoadGraph graph(region);
    if (graph.empty()) return false;
    // Stops: every shelter, named for its town. The capital's comes first; the rest go clockwise
    // from the north.
    for (const auto& town : region.settlements()) for (const auto& asset : town.assets) {
        if (!is_shelter(asset.model_path)) continue;
        const world::Settlement* nearest = nullptr;
        for (const auto& other : region.settlements())
            if (!nearest || flat(other.center, asset.position) < flat(nearest->center, asset.position)) nearest = &other;
        const auto spot = graph.nearest(asset.position);
        if (spot.edge < 0 || spot.distance > 15) continue;
        stops_.push_back({asset.id, nearest ? nearest->name : std::string(), asset.position, spot.point, 0});
    }
    if (stops_.size() < 2) {stops_.clear(); return false;}
    const auto clockwise = [](const BusStop& s) {
        const float r = std::hypot(s.kerb.x, s.kerb.z);
        return r < 600 ? -1.0F : std::fmod(std::atan2(s.kerb.x, -s.kerb.z) + 2 * PI, 2 * PI);
    };
    std::sort(stops_.begin(), stops_.end(), [&](const BusStop& a, const BusStop& b) {return clockwise(a) < clockwise(b);});
    // The loop: each stop to the next along the roads, back to the first. Each stop sits where
    // its leg begins. Where the line has to come back the way it came, the leg goes round the
    // stop's turnaround (world::roads::bus_turnaround) instead of reversing on the spot.
    const std::size_t n = stops_.size();
    std::vector<world::roads::Route> plain(n);
    for (std::size_t i = 0; i < n; ++i) plain[i] = graph.route(stops_[i].kerb, stops_[(i + 1) % n].kerb);
    const auto heading = [](const world::roads::Route& r, bool at_end, Vector3& d) {
        const auto& p = r.points;
        if (p.size() < 2) return false;
        if (at_end) {
            for (std::size_t k = p.size(); k-- > 1;) if (flat(p[k], p[k - 1]) > 2) {d = Vector3Normalize({p[k].x - p[k - 1].x, 0, p[k].z - p[k - 1].z}); return true;}
        } else {
            for (std::size_t k = 1; k < p.size(); ++k) if (flat(p[k], p[0]) > 2) {d = Vector3Normalize({p[k].x - p[0].x, 0, p[k].z - p[0].z}); return true;}
        }
        return false;
    };
    std::vector<BusStop> served;
    float walked = 0;
    const auto append = [&](const world::roads::Route& leg) {
        if (leg.points.empty()) return;
        if (loop_.points.empty()) loop_.points.push_back(leg.points.front());
        else if (flat(loop_.points.back(), leg.points.front()) > .01F) {
            walked += flat(loop_.points.back(), leg.points.front());
            loop_.points.push_back(leg.points.front()); loop_.lane.push_back(0);
        }
        for (std::size_t k = 1; k < leg.points.size(); ++k) {
            walked += flat(loop_.points.back(), leg.points[k]);
            loop_.points.push_back(leg.points[k]);
            loop_.lane.push_back(k - 1 < leg.lane.size() ? leg.lane[k - 1] : 0);
        }
    };
    for (std::size_t i = 0; i < n; ++i) {
        const auto& from = stops_[i];
        const auto& to = stops_[(i + 1) % n];
        if (!plain[i].complete || plain[i].points.size() < 2) continue;
        Vector3 arrive{}, leave{};
        const bool reverses = heading(plain[(i + n - 1) % n], true, arrive) && heading(plain[i], false, leave) &&
                              arrive.x * leave.x + arrive.z * leave.z < -.5F;
        std::vector<world::roads::Route> legs{plain[i]};
        if (reverses) {
            // Round the stop's turnaround: its ring (roads of Turnaround::width) near the stop;
            // in at the vertex joined by the stem, round past the far side, and back out.
            std::vector<Vector3> ring_points;
            for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
                if (!world::roads::turnaround_road(road.width)) continue;
                for (const auto& p : {road.start, road.end})
                    if (flat(p, from.kerb) < 260 &&
                        std::none_of(ring_points.begin(), ring_points.end(), [&](Vector3 q) {return flat(p, q) < .5F;}))
                        ring_points.push_back(p);
            }
            // Group into rings by distance; take the nearest ring to the stop.
            Vector3 centre{};
            std::vector<Vector3> ring;
            for (const auto& seed : ring_points) {
                std::vector<Vector3> group;
                for (const auto& p : ring_points) if (flat(p, seed) < 2 * world::roads::Turnaround::radius + 1) group.push_back(p);
                if (group.size() < 8) continue;
                Vector3 c{};
                for (const auto& p : group) c = Vector3Add(c, p);
                c = Vector3Scale(c, 1.0F / static_cast<float>(group.size()));
                std::vector<Vector3> on_ring;
                for (const auto& p : group) if (std::abs(flat(p, c) - world::roads::Turnaround::radius) < 1.5F) on_ring.push_back(p);
                if (on_ring.size() != 8) continue;
                if (ring.empty() || flat(c, from.kerb) < flat(centre, from.kerb)) {centre = c; ring = on_ring;}
            }
            if (ring.size() == 8) {
                // Order round the centre from the vertex nearest the stop (the stem's).
                std::sort(ring.begin(), ring.end(), [&](Vector3 a, Vector3 b) {
                    return std::atan2(a.z - centre.z, a.x - centre.x) < std::atan2(b.z - centre.z, b.x - centre.x);
                });
                std::size_t entry = 0;
                for (std::size_t k = 1; k < 8; ++k) if (flat(ring[k], from.kerb) < flat(ring[entry], from.kerb)) entry = k;
                const auto far_a = ring[(entry + 3) % 8], far_b = ring[(entry + 5) % 8];
                const auto a = graph.route(from.kerb, far_a), b = graph.route(far_a, far_b), c = graph.route(far_b, to.kerb);
                if (a.complete && b.complete && c.complete) {legs = {a, b, c}; ++turnarounds_;}
            }
        }
        served.push_back(from);
        served.back().at = walked;
        for (const auto& leg : legs) append(leg);
    }
    if (served.size() < 2 || loop_.points.size() < 3) {stops_.clear(); loop_ = {}; return false;}
    loop_.length = walked;
    loop_.complete = true;
    stops_ = std::move(served);
    // Faces for the people who ride the bus.
    faces_.clear();
    rng_.seed(static_cast<std::uint32_t>(seed * 2654435761U + 11));
    if (characters)
        for (int k = 0; k < 24; ++k)
            if (const auto* person = characters->choose(characters::CharacterPool::Civilian, seed * 31 + static_cast<std::uint64_t>(k) * 7727 + 3))
                if (std::find(faces_.begin(), faces_.end(), person->id) == faces_.end()) faces_.push_back(person->id);
    // A few already waiting at every stop.
    for (auto& stop : stops_)
        for (int k = 0, n = 1 + static_cast<int>(rng_() % 5); k < n; ++k) {++stop.waiting; stop.crowd.push_back(face());}
    // The buses, each with a driver.
    for (const auto& v : vehicles.vehicles()) {
        if (v.id.find("vehicle_spawn_bus_") == std::string::npos) continue;
        Bus bus;
        bus.vehicle = v.id;
        if (characters) {
            const auto* person = characters->choose(characters::CharacterPool::Civilian, seed * 7919 + buses_.size() * 104729 + 17);
            if (person) bus.driver = person->id;
        }
        // Already under way with people aboard.
        for (int k = 0, n = 6 + static_cast<int>(rng_() % 14); k < n; ++k)
            bus.riders.push_back({face(), static_cast<int>(rng_() % stops_.size())});
        buses_.push_back(std::move(bus));
    }
    std::sort(buses_.begin(), buses_.end(), [](const Bus& a, const Bus& b) {return a.vehicle < b.vehicle;});
    return !buses_.empty();
}

std::string BusLine::face() {
    return faces_.empty() ? std::string() : faces_[rng_() % faces_.size()];
}

int BusLine::aboard(int bus) const {
    if (bus < 0 || bus >= static_cast<int>(buses_.size())) return 0;
    const auto& b = buses_[static_cast<std::size_t>(bus)];
    return static_cast<int>(b.riders.size()) + (b.player ? 1 : 0);
}

void BusLine::call_at(Bus& bus, int stop) {
    // Off first: everyone whose stop this is. Then on, as many as there is room for, each for
    // another stop on the line.
    auto& here = stops_[static_cast<std::size_t>(stop)];
    const auto before = bus.riders.size();
    std::erase_if(bus.riders, [&](const Rider& r) { return r.alight == stop; });
    bus.got_off = static_cast<int>(before - bus.riders.size());
    bus.got_on = 0;
    const int room = config_.capacity - static_cast<int>(bus.riders.size()) - (bus.player ? 1 : 0);
    const int boarding = std::max(0, std::min(room, here.waiting));
    for (int k = 0; k < boarding; ++k) {
        int to = static_cast<int>(rng_() % stops_.size());
        if (to == stop) to = (to + 1) % static_cast<int>(stops_.size());
        bus.riders.push_back({here.crowd.empty() ? face() : here.crowd.back(), to});
        if (!here.crowd.empty()) here.crowd.pop_back();
    }
    here.waiting -= boarding;
    bus.got_on = boarding;
}

bool BusLine::board(int bus) {
    if (bus < 0 || bus >= static_cast<int>(buses_.size())) return false;
    auto& b = buses_[static_cast<std::size_t>(bus)];
    if (!b.running || b.dwell <= 0 || b.player || aboard(bus) >= config_.capacity) return false;
    b.player = true;
    return true;
}

bool BusLine::alight(int bus) {
    if (bus < 0 || bus >= static_cast<int>(buses_.size())) return false;
    auto& b = buses_[static_cast<std::size_t>(bus)];
    if (!b.player || (b.running && b.dwell <= 0)) return false;
    b.player = false;
    return true;
}

int BusLine::bus_at_stop(Vector3 at, float reach) const {
    for (std::size_t i = 0; i < buses_.size(); ++i)
        if (buses_[i].running && buses_[i].dwell > 0 && flat(buses_[i].position, at) < reach) return static_cast<int>(i);
    return -1;
}

void BusLine::pose(Bus& bus, Vector3& centre, float& yaw, float& steering) const {
    // The body spans its axles: each axle follows the lane, and the body lies between them.
    float front_yaw = 0;
    const Vector3 front = world::roads::route_point(loop_, wrap(bus.progress + config_.front_axle), &front_yaw);
    const Vector3 rear = world::roads::route_point(loop_, wrap(bus.progress + config_.rear_axle));
    const float t = -config_.rear_axle / (config_.front_axle - config_.rear_axle);
    centre = Vector3Lerp(rear, front, t);
    yaw = std::atan2(front.x - rear.x, front.z - rear.z) * RAD2DEG;
    steering = std::remainder(front_yaw - yaw, 360.0F);
}

void BusLine::start(world::VerdaRegion& region, vehicles::VehicleSystem& vehicles) {
    if (buses_.empty() || loop_.length <= 0) return;
    running_ = true;
    for (std::size_t i = 0; i < buses_.size(); ++i) {
        auto& bus = buses_[i];
        bus.progress = wrap(stops_.front().at + loop_.length * static_cast<float>(i) / static_cast<float>(buses_.size()) - 20);
        bus.speed = 0; bus.dwell = 0; bus.running = true;
        bus.next_stop = 0;
        for (std::size_t s = 0; s < stops_.size(); ++s)
            if (stops_[s].at > bus.progress) {bus.next_stop = static_cast<int>(s); break;}
        float steering = 0;
        pose(bus, bus.position, bus.yaw, steering);
        bus.running = vehicles.glide(region, bus.vehicle, bus.position, bus.yaw, 0, steering, 0);
    }
}

void BusLine::stop(world::VerdaRegion& region, vehicles::VehicleSystem& vehicles) {
    running_ = false;
    for (auto& bus : buses_) {
        vehicles.release(bus.vehicle);
        bus.running = false;
        if (const auto* asset = vehicles.asset(region, bus.vehicle))
            if (!asset->vehicle.destroyed) vehicles.place(region, bus.vehicle, asset->vehicle.home, asset->vehicle.home_yaw);
    }
}

void BusLine::update(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& vehicles, Vector3 player,
                     const characters::NpcSystem* npcs) {
    if (!running_ || !std::isfinite(dt) || dt <= 0) return;
    dt = std::min(dt, .1F);
    // Islanders turn up at the stops (twice as many in town), until the queue is long enough
    // that newcomers walk instead.
    for (auto& stop : stops_) {
        float rate = config_.arrivals / 60;
        for (const auto& town : region.settlements())
            if (town.name == stop.town && flat(town.center, stop.shelter) < 400) { rate *= 2; break; }
        stop.arriving += dt * rate;
        while (stop.arriving >= 1) {
            stop.arriving -= 1;
            if (stop.waiting >= config_.crowd_max) continue;
            ++stop.waiting;
            if (stop.crowd.size() < 8) stop.crowd.push_back(face());
        }
    }
    const auto* taken = vehicles.driver();
    for (auto& bus : buses_) {
        if (!bus.running) continue;
        const auto* asset = vehicles.asset(region, bus.vehicle);
        if (!asset || asset->vehicle.destroyed || (taken && taken->id == bus.vehicle)) {
            // Wrecked, or the player has it: off the line.
            vehicles.release(bus.vehicle);
            bus.running = false;
            continue;
        }
        float to_stop = wrap(stops_[static_cast<std::size_t>(bus.next_stop)].at - bus.progress);
        // A little past the stop counts as there (braking can run on half a metre).
        const bool overran = to_stop > loop_.length - 4;
        if (overran) to_stop = 0;
        else if (to_stop > loop_.length - 12 && bus.dwell <= 0) {
            // Well past it (started there): head for the one after.
            bus.next_stop = (bus.next_stop + 1) % static_cast<int>(stops_.size());
            to_stop = wrap(stops_[static_cast<std::size_t>(bus.next_stop)].at - bus.progress);
        }
        float limit = config_.cruise;
        // Towns and corners.
        for (const auto& stop : stops_) if (flat(stop.kerb, bus.position) < 250) limit = std::min(limit, config_.town_speed);
        float near_yaw = 0, far_yaw = 0;
        world::roads::route_point(loop_, wrap(bus.progress + 6), &near_yaw);
        world::roads::route_point(loop_, wrap(bus.progress + 6 + 12 + bus.speed * 2), &far_yaw);
        const float turn = std::abs(std::remainder(far_yaw - near_yaw, 360.0F));
        if (turn > 60) limit = std::min(limit, 3.5F);
        else if (turn > 20) limit = std::min(limit, config_.corner_speed);
        // Pull up at the stop.
        limit = std::min(limit, std::sqrt(2 * config_.brake * std::max(0.0F, to_stop - .3F)));
        // Anyone or anything in the lane ahead: wait.
        const Vector3 forward{std::sin(bus.yaw * DEG2RAD), 0, std::cos(bus.yaw * DEG2RAD)};
        // Looks as far ahead as it needs to stop, plus a margin.
        const float reach = 8.5F + bus.speed * bus.speed / (2 * config_.brake) + 8;
        float nearest = 1e9F;
        const auto look = [&](Vector3 p) {
            const float x = p.x - bus.position.x, z = p.z - bus.position.z;
            const float ahead = x * forward.x + z * forward.z, lateral = std::abs(x * forward.z - z * forward.x);
            if (ahead > 5.5F && ahead < reach && lateral < 2.0F) nearest = std::min(nearest, ahead);
        };
        look(player);
        if (npcs) for (const auto& actor : npcs->actors())
            if (actor.active && actor.state != characters::NpcState::Dead) look(actor.position);
        for (const auto& other : vehicles.vehicles()) if (other.id != bus.vehicle) look(other.position);
        bus.blocked = nearest < 1e8F;
        // Ease to a stop with the nose a couple of metres short of them.
        if (bus.blocked) limit = std::min(limit, std::sqrt(2 * config_.brake * std::max(0.0F, nearest - 8.5F)));
        if (bus.dwell > 0) {
            bus.dwell -= dt;
            limit = 0;
            if (bus.dwell <= 0) bus.next_stop = (bus.next_stop + 1) % static_cast<int>(stops_.size());
        } else if (to_stop < .6F && bus.speed < .4F) {
            bus.dwell = config_.dwell;
            call_at(bus, bus.next_stop);
        }
        // Throttle and brakes; harder braking when something is close.
        if (bus.speed < limit) bus.speed = std::min(limit, bus.speed + config_.accel * dt);
        else bus.speed = std::max(limit, bus.speed - (bus.blocked && nearest < 10 ? 6.0F : config_.brake * 1.3F) * dt);
        bus.speed = std::max(0.0F, bus.speed);
        bus.progress = wrap(bus.progress + bus.speed * dt);
        float steering = 0;
        pose(bus, bus.position, bus.yaw, steering);
        if (!vehicles.glide(region, bus.vehicle, bus.position, bus.yaw, bus.speed, steering, dt)) {
            vehicles.release(bus.vehicle);
            bus.running = false;
        }
    }
}

}
