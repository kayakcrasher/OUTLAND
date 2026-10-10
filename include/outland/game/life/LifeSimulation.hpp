#pragma once
#include <cstdint>
#include "outland/game/life/LifePopulation.hpp"

namespace outland::game::sound { class SoundBus; }
namespace outland::characters { class NpcSystem; }

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
    void sanitize();
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
    bool travelling(const Resident& resident) const {return clock_.minutes<resident.arrive;}
    std::string describe(const Resident& resident) const; // "Ivan Kovac, mechanic - working at the garage in Roka"
    const Resident* nearest(Vector3 position,float max_distance,bool physical_only=true) const;
    LifeStats stats() const;
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
    LifeConfig config_{};
    WorldClock clock_{};
    std::size_t cursor_{0};
    float replan_budget_{0}, bridge_clock_{0};
};
}
