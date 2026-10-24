#include "outland/game/freight/Freight.hpp"
#include "outland/characters/CharacterRegistry.hpp"
#include "outland/characters/NpcSystem.hpp"
#include "outland/game/ranch/Ranches.hpp"
#include "outland/game/transit/BusLine.hpp"
#include "outland/game/vehicles/VehicleSystem.hpp"
#include "outland/world/Ranches.hpp"
#include "outland/world/VerdaLayout.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::freight {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
float terrain(Vector3 p) { return world::terrain::TerrainHeight::sample(p.x, p.z); }
constexpr float front_axle = 2.36F, rear_axle = -2.0F;
// The stop's place on the loop: the rig's centre this far past the stop point puts the trailer's
// middle beside it (the chute).
constexpr float trailer_alongside = -fifth_wheel + kingpin_to_centre;
bool heading(const world::roads::Route& r, bool at_end, Vector3& d) {
    const auto& p = r.points;
    if (p.size() < 2) return false;
    if (at_end) {
        for (std::size_t k = p.size(); k-- > 1;) if (flat(p[k], p[k - 1]) > 2) { d = Vector3Normalize({p[k].x - p[k - 1].x, 0, p[k].z - p[k - 1].z}); return true; }
    } else {
        for (std::size_t k = 1; k < p.size(); ++k) if (flat(p[k], p[0]) > 2) { d = Vector3Normalize({p[k].x - p[0].x, 0, p[k].z - p[0].z}); return true; }
    }
    return false;
}
}

float Freight::wrap(const Rig& rig, float distance) const {
    if (rig.loop.length <= 0) return 0;
    distance = std::fmod(distance, rig.loop.length);
    return distance < 0 ? distance + rig.loop.length : distance;
}

namespace {
// A loop through `stops` in order by road: each leg routed on the road graph, each stop's `at`
// where the trailer's middle comes beside its point. False if a leg cannot be routed.
bool lay_loop(const world::roads::RoadGraph& graph, const std::vector<FreightStop>& stops, Rig& rig, int& reversals) {
    if (stops.size() < 2) return false;
    float walked = 0;
    std::vector<world::roads::Route> legs;
    for (std::size_t i = 0; i < stops.size(); ++i) legs.push_back(graph.route(stops[i].point, stops[(i + 1) % stops.size()].point));
    for (std::size_t i = 0; i < stops.size(); ++i) {
        const auto& leg = legs[i];
        if (!leg.complete || leg.points.size() < 2) return false;
        Vector3 arrive{}, leave{};
        if (heading(legs[(i + stops.size() - 1) % stops.size()], true, arrive) && heading(leg, false, leave) &&
            arrive.x * leave.x + arrive.z * leave.z < -.5F) ++reversals;
        auto stop = stops[i];
        stop.at = walked;
        rig.stops.push_back(stop);
        if (rig.loop.points.empty()) rig.loop.points.push_back(leg.points.front());
        else if (flat(rig.loop.points.back(), leg.points.front()) > .01F) {
            walked += flat(rig.loop.points.back(), leg.points.front());
            rig.loop.points.push_back(leg.points.front());
            rig.loop.lane.push_back(0);
        }
        for (std::size_t k = 1; k < leg.points.size(); ++k) {
            walked += flat(rig.loop.points.back(), leg.points[k]);
            rig.loop.points.push_back(leg.points[k]);
            rig.loop.lane.push_back(k - 1 < leg.lane.size() ? leg.lane[k - 1] : 0);
        }
    }
    rig.loop.length = walked;
    rig.loop.complete = walked > 0;
    for (auto& stop : rig.stops) stop.at = std::fmod(stop.at + trailer_alongside, walked);
    std::sort(rig.stops.begin(), rig.stops.end(), [](const FreightStop& a, const FreightStop& b) { return a.at < b.at; });
    return rig.loop.complete;
}
}

bool Freight::build(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars, const transit::BusLine& buses,
                    const ranch::Ranches* ranches, const characters::CharacterRegistry* characters, std::uint64_t seed) {
    rigs_.clear();
    hitches_.clear();
    running_ = false;
    reversals_ = 0;
    std::string supply, cattle;
    std::vector<std::string> haul(3);
    for (const auto& car : cars.vehicles()) {
        if (car.id.find("vehicle_spawn_semi_freight_") != std::string::npos) supply = car.id;
        if (car.id.find("vehicle_spawn_semi_cattle_") != std::string::npos) cattle = car.id;
        for (std::size_t k = 0; k < haul.size(); ++k)
            if (car.id.ends_with("vehicle_spawn_semi_haul_" + std::to_string(k + 1))) haul[k] = car.id;
    }
    const auto driver = [&](std::uint64_t k) -> std::string {
        if (!characters) return {};
        std::vector<std::string> men;
        for (const auto* d : characters->pool(characters::CharacterPool::Civilian))
            if (d->id.find("female") == std::string::npos && d->id.find("police") == std::string::npos) men.push_back(d->id);
        return men.empty() ? std::string() : men[(seed * 131 + k * 7919) % men.size()];
    };
    bool terminal = false, depot = false;
    for (const auto& town : region.settlements())
        for (const auto& asset : town.assets) {
            terminal |= asset.model_path.ends_with("civic/cargo_ship.glb");
            depot |= asset.model_path.ends_with("civic/truck_depot.glb");
        }
    const world::roads::RoadGraph graph(region);
    const auto stop = [&](std::string name, Vector3 at, int kind) {
        FreightStop s;
        s.name = std::move(name);
        s.point = graph.nearest(at).point;
        s.ranch = kind;
        return s;
    };
    // The supply run: the bus loop, pulling up a little short of each town's bus stop. (Verda
    // Transporto's second supply rig runs it too, starting halfway round.)
    const auto supply_run = [&](const std::string& vehicle, std::uint64_t who, bool halfway) {
        if (vehicle.empty() || !buses.loop().complete || buses.loop().length <= 0) return;
        Rig rig;
        rig.vehicle = vehicle;
        rig.driver = driver(who);
        rig.trailer = box_trailer;
        rig.loop = buses.loop();
        std::vector<std::string> towns;
        for (const auto& town : region.settlements())
            if (!town.id.starts_with("hamlet_") && !town.id.starts_with("ranch_")) towns.push_back(town.name);
        for (const auto& bus_stop : buses.stops()) {
            if (std::find(towns.begin(), towns.end(), bus_stop.town) == towns.end()) continue;
            FreightStop s;
            s.name = bus_stop.town;
            s.point = bus_stop.kerb;
            s.at = wrap(rig, bus_stop.at - 45);
            rig.stops.push_back(s);
        }
        std::sort(rig.stops.begin(), rig.stops.end(), [](const FreightStop& a, const FreightStop& b) { return a.at < b.at; });
        rig.first_stop = halfway ? static_cast<int>(rig.stops.size() / 2) : 0;
        if (!rig.stops.empty()) rigs_.push_back(std::move(rig));
    };
    supply_run(supply, 1, false);
    // The cattle run: every ranch's gate, then the cattle quay (or the stockyard), and round again.
    if (!cattle.empty() && ranches && ranches->active() && (ranches->cattle_quay().found || ranches->stockyard().found)) {
        std::vector<FreightStop> all;
        if (ranches->cattle_quay().found)
            all.push_back(stop(std::string(world::ranches::cattle_quay_name), ranches->cattle_quay().frame.at(world::ranches::cattle_quay_chute.x, -3), stop_cattle_quay));
        else
            all.push_back(stop(std::string(world::ranches::stockyard_name), ranches->stockyard().frame.at(world::ranches::stockyard_chute.x, -3), stop_stockyard));
        for (std::size_t i = 0; i < ranches->ranches().size(); ++i) {
            const auto& r = ranches->ranches()[i];
            all.push_back(stop(std::string(r.spec->name), r.frame.at(world::ranches::chute.x, -3), static_cast<int>(i)));
        }
        // The order of calls: fewest turnabouts on the road (a rig cannot turn on the spot), then
        // the shortest run.
        const auto reverses = [&](const world::roads::Route& in, const world::roads::Route& out) {
            Vector3 arrive{}, leave{};
            return heading(in, true, arrive) && heading(out, false, leave) && arrive.x * leave.x + arrive.z * leave.z < -.5F;
        };
        std::vector<std::vector<world::roads::Route>> leg(all.size(), std::vector<world::roads::Route>(all.size()));
        for (std::size_t i = 0; i < all.size(); ++i)
            for (std::size_t j = 0; j < all.size(); ++j)
                if (i != j) leg[i][j] = graph.route(all[i].point, all[j].point);
        std::vector<std::size_t> order(all.size() - 1), best_order;
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i + 1;
        float best_score = 1e30F;
        do {
            std::vector<std::size_t> visit{0};
            visit.insert(visit.end(), order.begin(), order.end());
            float length = 0;
            int turns = 0;
            bool ok = true;
            for (std::size_t k = 0; k < visit.size() && ok; ++k) {
                const auto& in = leg[visit[(k + visit.size() - 1) % visit.size()]][visit[k]];
                const auto& out = leg[visit[k]][visit[(k + 1) % visit.size()]];
                ok = out.complete;
                length += out.length;
                turns += reverses(in, out);
            }
            const float score = static_cast<float>(turns) * 1e6F + length;
            if (ok && score < best_score) { best_score = score; best_order = visit; }
        } while (std::next_permutation(order.begin(), order.end()));
        std::vector<FreightStop> stops;
        for (const auto k : best_order) stops.push_back(all[k]);
        Rig rig;
        rig.vehicle = cattle;
        rig.driver = driver(2);
        rig.trailer = cattle_trailer;
        rig.cattle = true;
        rig.aboard.assign(ranches->ranches().size(), 0);
        if (lay_loop(graph, stops, rig, reversals_)) rigs_.push_back(std::move(rig));
    }
    // Verda Transporto's own drivers.
    if (depot) {
        const auto haul_run = [&](const std::string& vehicle, std::uint64_t who, std::vector<FreightStop> stops) {
            if (vehicle.empty()) return;
            Rig rig;
            rig.vehicle = vehicle;
            rig.driver = driver(who);
            rig.trailer = box_trailer;
            if (lay_loop(graph, stops, rig, reversals_)) rigs_.push_back(std::move(rig));
        };
        const auto home = stop("Verda Transporto", world::layout::freight_depot_gate, stop_depot);
        const auto quay = stop("Kargohaveno de Porto Luma", world::layout::cargo_loading, stop_cargo_quay);
        if (terminal) haul_run(haul[0], 3, {home, quay});
        supply_run(haul[1], 4, true);
        const auto* roka = world::layout::find("west_roka");
        if (terminal && roka) haul_run(haul[2], 5, {quay, stop("Roka", roka->center, stop_town), home});
    }
    return !rigs_.empty();
}

void Freight::pose(const Rig& rig, float progress, Vector3& centre, float& yaw, float& steering) const {
    float front_yaw = 0;
    const Vector3 front = world::roads::route_point(rig.loop, wrap(rig, progress + front_axle), &front_yaw);
    const Vector3 rear = world::roads::route_point(rig.loop, wrap(rig, progress + rear_axle));
    centre = Vector3Lerp(rear, front, -rear_axle / (front_axle - rear_axle));
    yaw = std::atan2(front.x - rear.x, front.z - rear.z) * RAD2DEG;
    steering = std::remainder(front_yaw - yaw, 360.0F);
}

void Freight::start(world::VerdaRegion& region, vehicles::VehicleSystem& cars) {
    running_ = !rigs_.empty();
    for (auto& rig : rigs_) {
        // Just short of the first stop.
        rig.next_stop = std::clamp(rig.first_stop, 0, static_cast<int>(rig.stops.size()) - 1);
        rig.progress = wrap(rig, rig.stops[static_cast<std::size_t>(rig.next_stop)].at - 80);
        rig.speed = rig.dwell = 0;
        rig.head = 0;
        std::fill(rig.aboard.begin(), rig.aboard.end(), 0);
        rig.called = false;
        float steering = 0;
        pose(rig, rig.progress, rig.position, rig.yaw, steering);
        rig.running = cars.glide(region, rig.vehicle, rig.position, rig.yaw, 0, steering, 0);
        hitches_.erase(rig.vehicle); // the trailer lines up behind afresh
    }
}

void Freight::stop(world::VerdaRegion& region, vehicles::VehicleSystem& cars) {
    running_ = false;
    for (auto& rig : rigs_) {
        cars.release(rig.vehicle);
        rig.running = false;
        if (const auto* asset = cars.asset(region, rig.vehicle))
            if (!asset->vehicle.destroyed) cars.place(region, rig.vehicle, asset->vehicle.home, asset->vehicle.home_yaw);
        hitches_.erase(rig.vehicle);
    }
}

void Freight::update(float dt, world::VerdaRegion& region, vehicles::VehicleSystem& cars, Vector3 player,
                     const characters::NpcSystem* npcs, ranch::Ranches* ranches) {
    message_time_ = std::max(0.0F, message_time_ - dt);
    if (!running_ || !std::isfinite(dt) || dt <= 0) return;
    dt = std::min(dt, .1F);
    const auto* taken = cars.driver();
    for (auto& rig : rigs_) {
        if (!rig.running) continue;
        const auto* asset = cars.asset(region, rig.vehicle);
        if (!asset || asset->vehicle.destroyed || (taken && taken->id == rig.vehicle)) {
            cars.release(rig.vehicle);
            rig.running = false;
            continue;
        }
        auto& stop = rig.stops[static_cast<std::size_t>(rig.next_stop)];
        float to_stop = wrap(rig, stop.at - rig.progress);
        const bool overran = to_stop > rig.loop.length - 4;
        if (overran) to_stop = 0;
        // The cattle run sends word ahead: the family brings a few head to the corral.
        if (rig.cattle && ranches && stop.ranch >= 0 && !rig.called && to_stop < 700) {
            ranches->call_cattle(stop.ranch, 3);
            rig.called = true;
        }
        float limit = cruise;
        for (const auto& s : rig.stops) if (flat(s.point, rig.position) < 220) limit = std::min(limit, town_speed);
        float near_yaw = 0, far_yaw = 0;
        world::roads::route_point(rig.loop, wrap(rig, rig.progress + 7), &near_yaw);
        world::roads::route_point(rig.loop, wrap(rig, rig.progress + 20 + rig.speed * 2.5F), &far_yaw);
        const float turn = std::abs(std::remainder(far_yaw - near_yaw, 360.0F));
        if (turn > 60) limit = std::min(limit, 3.0F);
        else if (turn > 20) limit = std::min(limit, corner_speed);
        limit = std::min(limit, std::sqrt(2 * brake * std::max(0.0F, to_stop - .3F)));
        // Anything in the lane ahead: hold back (a loaded rig needs room).
        const Vector3 forward{std::sin(rig.yaw * DEG2RAD), 0, std::cos(rig.yaw * DEG2RAD)};
        const float reach = 5 + rig.speed * rig.speed / (2 * brake) + 12;
        float nearest = 1e9F;
        const auto look = [&](Vector3 p, float width) {
            const float x = p.x - rig.position.x, z = p.z - rig.position.z;
            const float ahead = x * forward.x + z * forward.z, lateral = std::abs(x * forward.z - z * forward.x);
            if (ahead > 4.5F && ahead < reach && lateral < width) nearest = std::min(nearest, ahead);
        };
        look(player, 2.0F);
        if (npcs) for (const auto& actor : npcs->actors())
            if (actor.active && actor.state != characters::NpcState::Dead && !actor.hidden) look(actor.position, 2.0F);
        // Cars: a car parked at the kerb may stand a hand's width into the lane - the rig squeezes
        // past; one stopped in the lane for long, it edges round.
        for (const auto& other : cars.vehicles())
            if (other.id != rig.vehicle && (rig.passing <= 0 || std::abs(other.speed) > .3F)) look(other.position, std::abs(other.speed) > .3F ? 2.0F : 1.7F);
        rig.blocked = nearest < 1e8F;
        rig.waited = rig.blocked && rig.speed < .3F ? rig.waited + dt : 0;
        if (rig.waited > 20) { rig.passing = 10; rig.waited = 0; }
        rig.passing = std::max(0.0F, rig.passing - dt);
        if (rig.blocked) limit = std::min(limit, std::sqrt(2 * brake * std::max(0.0F, nearest - 9)));
        if (rig.dwell > 0) {
            rig.dwell -= dt;
            limit = 0;
            if (rig.dwell <= 0) {
                rig.next_stop = (rig.next_stop + 1) % static_cast<int>(rig.stops.size());
                rig.called = false;
            }
        } else if (to_stop < .6F && rig.speed < .4F) {
            // Arrived: deliver, load or unload.
            const bool seen = flat(player, rig.position) < 160;
            if (!rig.cattle) {
                rig.dwell = town_dwell;
                ++rig.deliveries;
                if (seen)
                    say(stop.ranch == stop_cargo_quay ? "The crane swings a container onto the rig at the cargo terminal" :
                        stop.ranch == stop_depot ? "A Verda Transporto rig is unloading at the depot" :
                        "The big rig is unloading supplies for " + stop.name);
            } else if (stop.ranch >= 0 && ranches) {
                rig.dwell = ranch_dwell;
                const int loaded = ranches->load_cattle(stop.ranch, 4);
                rig.head += loaded;
                if (static_cast<std::size_t>(stop.ranch) < rig.aboard.size()) rig.aboard[static_cast<std::size_t>(stop.ranch)] += loaded;
                if (seen) say(loaded > 0 ? "Cattle truck: " + std::to_string(loaded) + " head up the chute at " + stop.name
                                         : "Cattle truck at " + stop.name + " - no cattle ready");
            } else if (ranches && stop.ranch == stop_cattle_quay) {
                // Off at the quay and shipped: each family paid for its own.
                rig.dwell = stockyard_dwell;
                for (std::size_t r = 0; r < rig.aboard.size(); ++r)
                    if (rig.aboard[r] > 0) ranches->ship_cattle(static_cast<int>(r), rig.aboard[r]);
                std::fill(rig.aboard.begin(), rig.aboard.end(), 0);
                if (seen && rig.head > 0) say("Cattle truck: " + std::to_string(rig.head) + " head off at the cattle quay, for the ship");
                rig.head = 0;
            } else if (ranches) {
                rig.dwell = stockyard_dwell;
                if (rig.head > 0) ranches->unload_cattle(rig.head);
                if (seen && rig.head > 0) say("Cattle truck: " + std::to_string(rig.head) + " head off at the stockyard");
                rig.head = 0;
                std::fill(rig.aboard.begin(), rig.aboard.end(), 0);
            }
        }
        if (rig.speed < limit) rig.speed = std::min(limit, rig.speed + accel * dt);
        else rig.speed = std::max(limit, rig.speed - (rig.blocked && nearest < 12 ? 6.0F : brake * 1.3F) * dt);
        rig.speed = std::max(0.0F, rig.speed);
        rig.progress = wrap(rig, rig.progress + rig.speed * dt);
        float steering = 0;
        pose(rig, rig.progress, rig.position, rig.yaw, steering);
        if (!cars.glide(region, rig.vehicle, rig.position, rig.yaw, rig.speed, steering, dt)) {
            cars.release(rig.vehicle);
            rig.running = false;
        }
    }
}

void Freight::follow_trailers(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars) {
    for (const auto& car : cars.vehicles()) {
        const auto* asset = cars.asset(region, car.id);
        if (!asset || asset->vehicle.definition != "semi") continue;
        auto& hitch = hitches_[car.id];
        if (hitch.dropped) continue;          // left standing where it was dropped
        if (hitch.model.empty()) hitch.model = car.id.find("cattle") != std::string::npos ? cattle_trailer : box_trailer;   // (the depot's stock rig too)
        const float a = asset->rotation_y * DEG2RAD;
        const Vector3 forward{std::sin(a), 0, std::cos(a)};
        hitch.kingpin = Vector3Add(asset->position, Vector3Scale(forward, fifth_wheel));
        if (!hitch.placed || flat(hitch.kingpin, hitch.axles) > kingpin_to_axles + 4) {
            hitch.axles = Vector3Subtract(hitch.kingpin, Vector3Scale(forward, kingpin_to_axles));
            hitch.placed = true;
        }
        // The tandem trails the kingpin: it is pulled along the line between them.
        Vector3 d{hitch.kingpin.x - hitch.axles.x, 0, hitch.kingpin.z - hitch.axles.z};
        const float length = Vector3Length(d);
        if (length > 1e-3F) {
            d = Vector3Scale(d, 1 / length);
            // Never folded past 80 degrees to the tractor (a jack-knife), however sharp the corner.
            const float fold = std::atan2(forward.x * d.z - forward.z * d.x, forward.x * d.x + forward.z * d.z);
            constexpr float limit = 80 * DEG2RAD;
            if (std::abs(fold) > limit) {
                const float a = std::copysign(limit, fold), c = std::cos(a), s = std::sin(a);
                d = {forward.x * c - forward.z * s, 0, forward.x * s + forward.z * c};
            }
            hitch.axles = {hitch.kingpin.x - d.x * kingpin_to_axles, 0, hitch.kingpin.z - d.z * kingpin_to_axles};
        }
    }
    std::erase_if(hitches_, [&](const auto& entry) { return cars.find(entry.first) < 0; });
}

bool Freight::hitched(const std::string& vehicle) const {
    const auto found = hitches_.find(vehicle);
    return found == hitches_.end() || !found->second.dropped;
}

bool Freight::dropped_at(const std::string& vehicle, Vector3& kingpin) const {
    const auto found = hitches_.find(vehicle);
    if (found == hitches_.end() || !found->second.dropped) return false;
    kingpin = found->second.kingpin;
    return true;
}

std::string Freight::drop_or_hitch(const world::VerdaRegion& region, const vehicles::VehicleSystem& cars, const std::string& vehicle) {
    const auto found = hitches_.find(vehicle);
    const int index = cars.find(vehicle);
    if (found == hitches_.end() || index < 0) return "No trailer to drop";
    auto& hitch = found->second;
    const auto* asset = cars.asset(region, vehicle);
    if (!asset) return "No trailer to drop";
    if (!hitch.dropped) {
        hitch.dropped = true;
        return "Trailer dropped - back up under its nose and press HITCH to take it again";
    }
    const float a = asset->rotation_y * DEG2RAD;
    const Vector3 forward{std::sin(a), 0, std::cos(a)};
    const Vector3 fifth = Vector3Add(asset->position, Vector3Scale(forward, fifth_wheel));
    Vector3 along{hitch.kingpin.x - hitch.axles.x, 0, hitch.kingpin.z - hitch.axles.z};
    const float length = Vector3Length(along);
    if (length > 1e-3F) along = Vector3Scale(along, 1 / length);
    const float angle = std::acos(std::clamp(forward.x * along.x + forward.z * along.z, -1.0F, 1.0F)) * RAD2DEG;
    if (flat(fifth, hitch.kingpin) > hitch_reach) return "Back the rig up under the trailer's nose to hitch it";
    if (angle > hitch_angle) return "Line the rig up with the trailer to hitch it";
    hitch.dropped = false;
    return "Trailer hitched";
}

std::vector<TrailerPose> Freight::trailers() const {
    std::vector<TrailerPose> out;
    for (const auto& [id, hitch] : hitches_) {
        if (!hitch.placed) continue;
        Vector3 d{hitch.kingpin.x - hitch.axles.x, 0, hitch.kingpin.z - hitch.axles.z};
        if (Vector3Length(d) < 1e-3F) continue;
        d = Vector3Normalize(d);
        TrailerPose pose;
        pose.vehicle = id;
        pose.model = hitch.model;
        const float front = terrain(hitch.kingpin), back = terrain(hitch.axles);
        pose.centre = Vector3Subtract(hitch.kingpin, Vector3Scale(d, kingpin_to_centre));
        pose.centre.y = (front + back) * .5F;
        pose.yaw = std::atan2(d.x, d.z) * RAD2DEG;
        pose.pitch = std::atan2(front - back, kingpin_to_axles) * RAD2DEG;
        out.push_back(pose);
    }
    return out;
}

std::vector<RigDriver> Freight::drivers() const {
    std::vector<RigDriver> out;
    for (const auto& rig : rigs_) if (rig.running && !rig.driver.empty()) out.push_back({rig.vehicle, rig.driver, rig.cattle});
    return out;
}

} // namespace outland::game::freight
