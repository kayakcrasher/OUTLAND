#pragma once
#include <array>
#include <cstdint>
#include "outland/game/life/LifePopulation.hpp"
#include "outland/world/roads/RoadGraph.hpp"
#include "outland/world/roads/Signals.hpp"
#include <string_view>

namespace outland::game::sound { class SoundBus; }
namespace outland::characters { class NpcSystem; }
namespace outland::game::vehicles { class VehicleSystem; }

namespace outland::game::life {
struct LifeConfig {
    float time_scale{30};           // island seconds per real second (48 real minutes per day)
    float materialize_radius{110};  // outdoor residents become bodies inside this
    float dematerialize_radius{150};
    float indoor_radius{18};        // residents indoors only matter when the player is at the building
    int max_physical{24};           // phone budget for simultaneous resident bodies
    float hearing_radius{260};      // gunfire sends residents in this radius home
    float shelter_minutes{150};
    float replan_period{.5F};       // real seconds for every resident to be re-planned once
    // Cars (when bound to the world's vehicles): trips longer than drive_distance are driven if
    // the resident's car is within car_reach. Near the player the real vehicle is driven.
    float drive_distance{110};
    float car_reach{320};
    float car_speed{11};            // m/s of island time off-screen
    float car_radius{220};          // driven for real inside this
    float car_release{290};         // and handed back to the abstract tier beyond this
    int max_driven{14};
    void sanitize();
};
struct CarState {
    Vector3 position{};             // parked, or where it is on its route
    float yaw{0};
    bool lost{false};               // taken by the player or wrecked: out of island life
    bool driving{false};            // on a trip, its driver at the wheel
    bool physical{false};           // the real vehicle is under autopilot
    world::roads::Route route;
    float progress{0};              // metres along route
    int destination{-1};            // place
    Vector3 park{};                 // where this trip ends
    float park_yaw{0};
    float stuck{0}, reverse{0}, waiting{0};
    float passing{0};               // seconds left going round something standing in the lane
    int attempts{0};
};
struct LifeStats { int population{0}, alive{0}, physical{0}, travelling{0}, indoors{0}, sheltering{0}; };

// Two tiers. Abstract: every resident is a schedule plus a straight-line trip between places,
// O(1) to evaluate, no pathfinding. Physical: the nearest residents become NpcSystem actors
// that walk to their schedule anchor. Fear/combat in NpcBehavior overrides the schedule;
// once it settles, the schedule resumes (via Shelter first if the resident was frightened).
class LifeSimulation {
public:
    void build(const world::VerdaRegion& region,const characters::CharacterRegistry* registry,PopulationOptions options={});
    void configure(LifeConfig config) {config.sanitize();config_=config;}
    // Newcomers move into a player-built home: a family of `members` steps off at `arrive_from`
    // (a bus stop) and walks home, or is already in (`arrive_from` = nullptr: a saved household).
    // Returns the new residents' ids.
    std::vector<int> move_in(const world::Building& home,int members,std::uint64_t seed,const Vector3* arrive_from=nullptr);
    const LifeConfig& config() const {return config_;}
    // New play session: everyone alive at home-plan positions, no bodies, clock at `start`.
    void reset(characters::NpcSystem& npcs,WorldClock start=WorldClock::at(0,7,30));
    void clear(characters::NpcSystem& npcs);
    void update(float real_dt,Vector3 player,characters::NpcSystem& npcs,bool paused=false);
    void report_gunfire(Vector3 position);
    // Reads new events from the island sound bus: every gunshot anyone fires frightens residents.
    void hear(const sound::SoundBus& sounds);

    const WorldClock& clock() const {return clock_;}
    void set_clock(WorldClock clock) {clock_=clock;}
    const std::vector<Resident>& residents() const {return island_.residents;}
    const std::vector<Place>& places() const {return island_.places;}
    const Island& island() const {return island_;}
    bool empty() const {return island_.residents.empty();}

    Vector3 position(const Resident& resident) const;   // body if physical, else abstract
    bool indoors(const Resident& resident) const;
    bool travelling(const Resident& resident) const {return clock_.minutes<resident.arrive || resident.trip!=Trip::Walk;}
    std::string describe(const Resident& resident) const; // "Ivan Kovac, mechanic - working at the garage in Roka"
    const Resident* nearest(Vector3 position,float max_distance,bool physical_only=true) const;
    // Police response: send a resident (an officer) to a location at car speed while off-screen,
    // running once a body; release returns them to their schedule.
    static constexpr float response_speed=14; // m/s while abstract (patrol car)
    void dispatch(int resident,Vector3 where);
    // A wounded resident taken to hospital (their body already gone): treated for `minutes`
    // of island time, then out of `door`. False if they are not alive.
    bool hospitalise(int resident,Vector3 door,double minutes);
    bool in_hospital(const Resident& resident) const {return resident.hospital_until>clock_.minutes;}
    void release(int resident);
    // Put away (their body already gone): off the island until let out at `gate` - by `until`
    // (island minutes), or sooner with set_free(). False if they are not alive.
    bool imprison(int resident,Vector3 gate,double until);
    void set_free(int resident);
    bool in_jail(const Resident& resident) const {return resident.jail_until>clock_.minutes;}
    // The ballparks (one per club, world::ballpark::parks order): places; -1 not on the map.
    int ballpark(int team) const {return team>=0 && team<5 ? ballparks_[static_cast<std::size_t>(team)] : -1;}
    // Off to a ball game at `place` (a ballpark): leaving at `from`, home again after `until`
    // (island minutes). False for someone who can't go (dead, in hospital or jail, on a call).
    bool attend(int resident,int place,double from,double until);
    // In the stands now (arrived, not a body in the world).
    bool in_the_stands(const Resident& resident) const;
    LifeStats stats() const;

    // Cars: bind the world's vehicles (Explore) before reset(); nullptr unbinds. Without them,
    // long trips are timed as before (a lift or the bus) and cars stay parked.
    void bind_vehicles(vehicles::VehicleSystem* vehicles,world::VerdaRegion* region);
    const std::vector<CarState>& cars() const {return cars_;}
    const world::roads::RoadGraph& roads() const {return roads_;}
    int car_of(std::string_view vehicle) const;           // Island::cars index, or -1
    bool car_moving(std::string_view vehicle) const;      // a resident is driving it
    // The traffic signals island traffic obeys, and their clock (seconds of play).
    const std::vector<world::roads::Signal>& signals() const {return signals_;}
    double signal_clock() const {return signal_clock_;}
    struct Taken {int owner{-1};bool occupied{false};Vector3 where{};};
    // The player took this vehicle. It leaves island life; a driver inside is pulled out.
    Taken take_car(std::string_view vehicle);
private:
    vehicles::VehicleSystem* vehicles_{nullptr};
    world::VerdaRegion* region_{nullptr};
    std::vector<world::roads::Signal> signals_;
    double signal_clock_{0};
    world::roads::RoadGraph roads_;
    std::vector<CarState> cars_;
    float traffic_clock_{0};
    bool wants_car(const Resident& resident,Vector3 from) const;
    Vector3 car_door(int car) const;
    Vector3 trip_target(const Resident& resident) const;
    void parking(int car,int place,Vector3& spot,float& yaw) const;
    void start_drive(Resident& resident);
    void route_car(Resident& resident);
    void park_car(int car,bool where_it_stands);
    void leave_car(Resident& resident,bool frightened);
    void drive(int car,float real_dt,Vector3 player,const characters::NpcSystem& npcs,int& driven);
    void update_traffic(float real_dt,Vector3 player,const characters::NpcSystem& npcs);
    void reset_cars();
private:
    std::uint64_t heard_up_to_{0};
    struct Anchor {Vector3 point;float radius;};
    void replan(Resident& resident);
    int resolve(const Resident& resident,Activity activity) const;
    bool outdoors_now(const Resident& resident) const;
    Vector3 abstract_position(const Resident& resident) const;
    Anchor physical_anchor(const Resident& resident,Vector3 body) const;
    void sync_bodies(Vector3 player,characters::NpcSystem& npcs);
    Island island_;
    std::array<int,5> ballparks_{{-1,-1,-1,-1,-1}};
    // Nightlife: where cruisers stop (squares, shops, pubs and garages all over the island), each
    // town's pubs (bar hoppers go round them), and the pub in the biggest town (the party).
    std::vector<int> cruise_spots_;
    std::vector<std::vector<int>> town_pubs_;
    int party_pub_{-1};
    void index_nightlife();
    const characters::CharacterRegistry* registry_{nullptr};
    LifeConfig config_{};
    WorldClock clock_{};
    std::size_t cursor_{0};
    float replan_budget_{0}, bridge_clock_{0};
};
}
