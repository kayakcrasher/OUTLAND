#include "outland/game/freight/Trucking.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace outland::game::freight {
namespace {
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
constexpr const char* goods[]{"machine parts", "coffee", "timber", "fertiliser", "furniture", "olive oil", "tinned fish",
                              "building supplies", "electronics", "tyres", "paper", "wine"};
}

const char* haul_name(HaulKind kind) {
    switch (kind) {
        case HaulKind::Import: return "IMPORT";
        case HaulKind::Export: return "EXPORT";
        case HaulKind::Delivery: return "DELIVERY";
        case HaulKind::Cattle: return "CATTLE HAUL";
        default: return "";
    }
}

float Trucking::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
    return static_cast<float>(rng_ >> 40) / 16777216.0F;
}

Haul Trucking::next_job(const HaulWorld& world) {
    std::vector<HaulKind> kinds;
    if (!world.towns.empty()) kinds.push_back(HaulKind::Delivery);
    if (world.terminal && !world.towns.empty()) kinds.insert(kinds.end(), {HaulKind::Import, HaulKind::Import, HaulKind::Export});
    if (world.terminal && !world.ranches.empty()) kinds.insert(kinds.end(), {HaulKind::Cattle, HaulKind::Cattle});
    Haul job;
    if (kinds.empty()) return job;
    const auto pick = [&](std::size_t n) { return std::min(n - 1, static_cast<std::size_t>(random() * static_cast<float>(n))); };
    job.kind = kinds[pick(kinds.size())];
    job.stage = HaulStage::Pickup;
    job.cargo = goods[pick(std::size(goods))];
    const auto town = [&] { return world.towns[pick(world.towns.size())]; };
    switch (job.kind) {
        case HaulKind::Import: {
            const auto to = town();
            job.from_name = "the cargo terminal at Porto Luma"; job.from = world.cargo_quay;
            job.to_name = to.name; job.to = to.point;
            job.cargo = "a container of " + job.cargo;
            break;
        }
        case HaulKind::Export:
            job.from_name = "the depot"; job.from = world.depot;
            job.to_name = "the cargo terminal at Porto Luma"; job.to = world.cargo_quay;
            break;
        case HaulKind::Delivery: {
            const auto to = town();
            job.from_name = "the depot"; job.from = world.depot;
            job.to_name = to.name; job.to = to.point;
            break;
        }
        case HaulKind::Cattle: {
            const auto& ranch = world.ranches[pick(world.ranches.size())];
            job.from_name = ranch.name; job.from = ranch.point; job.ranch = ranch.ranch;
            job.to_name = "the cattle quay at Porto Luma"; job.to = world.cattle_quay;
            job.cargo = "cattle";
            call_pending_ = true;
            cattle_tries_ = 0;
            break;
        }
        default: break;
    }
    // By the trip: the run from the yard to the pickup counts for half.
    const float km = (flat(job.from, job.to) + .5F * flat(world.depot, job.from)) / 1000;
    job.pay = base_pay + static_cast<int>(std::lround(km * pay_per_km / 10)) * 10;
    return job;
}

std::string Trucking::talk(bool cop, double now, const HaulWorld& world) {
    depot_ = world.depot;
    if (cop) return "Boss: \"Verda Transporto doesn't hire serving officers. Hand in your badge first, then come and drive for us.\"";
    const bool confirming = now < offer_until_;
    if (!hired_) {
        if (confirming && !offer_quit_) {
            hired_ = true;
            offer_until_ = -1;
            job_ = next_job(world);
            return "Boss: \"Welcome to Verda Transporto. Take a rig from the yard. First load: " + detail() + ".\"";
        }
        offer_until_ = now + confirm_seconds;
        offer_quit_ = false;
        return "Boss: \"Looking for work? We haul for the ship at Porto Luma, the towns and the ranches, and pay by the trip. USE again to sign on.\"";
    }
    switch (job_.stage) {
        case HaulStage::Report: {
            const int paid = job_.pay;
            pay_ += paid;
            earned_ += paid;
            ++trips_;
            job_ = next_job(world);
            return "Boss: \"Good run - " + std::to_string(paid) + " kr. Next load: " + detail() + ".\"";
        }
        case HaulStage::None:
            job_ = next_job(world);
            return "Boss: \"Here's a load for you: " + detail() + ".\"";
        default:
            if (confirming && offer_quit_) {
                job_ = {};
                offer_until_ = -1;
                return "Boss: \"Fine, someone else will take it. Come back when you want another load.\"";
            }
            offer_until_ = now + confirm_seconds;
            offer_quit_ = true;
            return "Boss: \"You've a load to move - " + detail() + ". (USE again to give it up.)\"";
    }
}

Vector3 Trucking::target() const {
    switch (job_.stage) {
        case HaulStage::Pickup: return job_.from;
        case HaulStage::Deliver: return job_.to;
        case HaulStage::Report: return depot_;
        default: return {};
    }
}

std::string Trucking::title() const { return haul_name(job_.kind); }

std::string Trucking::detail() const {
    switch (job_.stage) {
        case HaulStage::Pickup:
            return job_.kind == HaulKind::Cattle ? "pick up cattle at " + job_.from_name + " (the stock-trailer rig) for " + job_.to_name
                                                 : "pick up " + job_.cargo + " at " + job_.from_name + " for " + job_.to_name;
        case HaulStage::Deliver: return "take " + job_.cargo + " to " + job_.to_name;
        case HaulStage::Report: return "back to the boss at the depot for your pay (" + std::to_string(job_.pay) + " kr)";
        default: return "";
    }
}

TruckEvent Trucking::update(const TruckFrame& frame) {
    message_time_ = std::max(0.0F, message_time_ - frame.dt);
    nag_ = std::max(0.0F, nag_ - frame.dt);
    if (call_pending_) { call_pending_ = false; return TruckEvent::CallCattle; }
    if (!hired_ || (job_.stage != HaulStage::Pickup && job_.stage != HaulStage::Deliver)) return TruckEvent::None;
    const bool pickup = job_.stage == HaulStage::Pickup;
    if (flat(frame.player, target()) > stop_radius) { job_.work = 0; return TruckEvent::None; }
    const auto nag = [&](std::string text) { if (nag_ <= 0) { say(std::move(text)); nag_ = 5; } };
    if (frame.driving.empty()) { nag("Bring a Verda Transporto rig here to " + std::string(pickup ? "load" : "unload")); return TruckEvent::None; }
    if (!right_rig(frame.driving)) {
        nag(job_.kind == HaulKind::Cattle ? "Cattle go in the company's stock-trailer rig - it's in the yard at the depot"
                                          : "Take one of Verda Transporto's box-trailer rigs from the yard at the depot");
        return TruckEvent::None;
    }
    if (std::abs(frame.speed) > still) { job_.work = 0; nag("Stop here to " + std::string(pickup ? "load" : "unload")); return TruckEvent::None; }
    job_.work += frame.dt;
    if (job_.work < work_seconds) {
        say(pickup ? (job_.kind == HaulKind::Cattle ? "Loading cattle up the chute..." : job_.kind == HaulKind::Import ? "The crane is loading a container..." : "Loading...")
                   : (job_.kind == HaulKind::Cattle ? "Unloading the cattle..." : "Unloading..."), .4F);
        return TruckEvent::None;
    }
    job_.work = 0;
    if (pickup) {
        if (job_.kind == HaulKind::Cattle) { job_.work = work_seconds - 3; return TruckEvent::LoadCattle; }   // (again in 3 s if none)
        job_.stage = HaulStage::Deliver;
        say("Loaded: " + job_.cargo + ". Take it to " + job_.to_name, 5);
        return TruckEvent::Loaded;
    }
    job_.stage = HaulStage::Report;
    if (job_.kind == HaulKind::Cattle) {
        job_.pay += job_.head * head_pay;
        say(std::to_string(job_.head) + " head off for the ship. Back to the boss at Verda Transporto for your pay", 6);
        return TruckEvent::ShipCattle;
    }
    say("Delivered. Back to the boss at Verda Transporto for your pay", 6);
    return TruckEvent::Delivered;
}

void Trucking::cattle_loaded(int head) {
    if (job_.kind != HaulKind::Cattle || job_.stage != HaulStage::Pickup) return;
    if (head > 0) {
        job_.head = head;
        job_.cargo = std::to_string(head) + " head of cattle";
        job_.stage = HaulStage::Deliver;
        job_.work = 0;
        say(job_.cargo + " up the chute. Take them to " + job_.to_name, 5);
        return;
    }
    // None in the corral yet: the family is bringing them (ask again now and then).
    if (++cattle_tries_ % 5 == 0) call_pending_ = true;
    say("No cattle in the corral yet - the family is bringing them in. Wait here", 3);
}

std::int64_t Diesel::fill(float& fuel, float dt, bool company, std::int64_t& money, float& owed, float tanks) {
    if (fuel >= 1 || dt <= 0) return 0;
    tanks = std::max(1.0F, tanks);
    const float price = tank_price * tanks;
    float add = std::min(1 - fuel, fill_per_second / tanks * dt);
    if (company) { fuel = std::min(1.0F, fuel + add); return 0; }
    add = std::min(add, std::max(0.0F, (static_cast<float>(money) - owed) / price));
    if (add <= 0) return 0;
    fuel = std::min(1.0F, fuel + add);
    owed += add * price;
    const auto whole = std::min(money, static_cast<std::int64_t>(owed));
    owed -= static_cast<float>(whole);
    money -= whole;
    return whole;
}

std::string Trucking::serialise() const {
    std::ostringstream out;
    out << "hired=" << (hired_ ? 1 : 0) << "\ntrips=" << trips_ << "\nearned=" << earned_ << "\n";
    return out.str();
}

bool Trucking::parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    bool any = false;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        try {
            if (key == "hired") { hired_ = std::stoi(value) != 0; any = true; }
            else if (key == "trips") trips_ = std::max(0, std::stoi(value));
            else if (key == "earned") earned_ = std::max<std::int64_t>(0, std::stoll(value));
        } catch (...) { return false; }
    }
    job_ = {};
    return any;
}

} // namespace outland::game::freight
