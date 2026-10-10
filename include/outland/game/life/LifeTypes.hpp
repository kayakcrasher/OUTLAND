#pragma once
#include <raylib.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Verdan civilian life: who lives where, works where and does what at each hour.
// This layer sits ABOVE characters::NpcBehavior (Idle/Wander/Alert/Chase/Attack/Flee/Dead).
// It never moves bodies itself; it only tells a physical NPC where it would like to be.
namespace outland::game::life {

enum class Weekday : std::uint8_t { Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday };

// Island time in minutes since Monday 00:00 of week zero.
struct WorldClock {
    double minutes{7*60+30};
    static WorldClock at(int day,int hour,int minute=0);
    int day() const;
    Weekday weekday() const;
    int minute_of_day() const;
    float hour() const;
    bool night() const;
    void advance(float real_seconds,float time_scale);
    std::string label() const; // "SUN 09:14"
};

enum class PlaceKind : std::uint8_t {
    Home, Garage, Industrial, Dock, Shop, Pub, Church, Police, Clinic, Office, Farm, Square
};
const char* place_name(PlaceKind kind);

struct Place {
    std::string id, building_id;
    PlaceKind kind{PlaceKind::Home};
    int settlement{0};
    Vector3 interior{}, door{}; // door is outside the doorway; equal for outdoor places
    float building_yaw{0};
    Vector3 building_size{};
    bool indoor{true};
    bool sealed{false};     // solid shell (office tower): people go in and out of the door, never walk inside
    bool mixed_home{false}; // shop/pub with a flat above: also a home
    int home_capacity{0}, job_capacity{0};
};

enum class Occupation : std::uint8_t {
    Mechanic, Dockworker, FactoryWorker, Shopkeeper, Publican, Pastor, PoliceOfficer, Doctor,
    Clerk, Farmer, Retired, Unemployed
};
const char* occupation_name(Occupation occupation);

enum class Faction : std::uint8_t { None, Police, Krimulo, GreenStar };
const char* faction_name(Faction faction);

enum class Activity : std::uint8_t {
    Sleep, Home, Work, Lunch, Socialize, Worship, Shopping, Visit, Stroll, Patrol, FactionMeet, Shelter
};
const char* activity_name(Activity activity);

// One block of a day: from `minute` until the next block's minute.
struct Block { int minute; Activity activity; };
enum class DayType : std::uint8_t { Weekday, Saturday, Sunday };
DayType day_type(Weekday day);

struct Resident {
    int id{0};
    std::string first_name, family_name, character_id;
    bool female{false};
    Occupation occupation{Occupation::Unemployed};
    Faction faction{Faction::None};
    int settlement{0}, home{-1}, work{-1}, pub{-1}, church{-1}, shop{-1}, lunch{-1}, hangout{-1};
    int patrol_settlement{-1}, shift{0};
    std::vector<int> relatives;        // other resident ids in related households
    std::vector<int> household;        // resident ids sharing this home (including self)
    int visit_home{-1};                // relatives' or a friend's home
    bool churchgoer{false}, pub_regular{false}, works_weekends{false};
    std::array<std::vector<Block>,7> days; // indexed by Weekday

    // Runtime state, session only.
    bool alive{true}, physical{false};
    float health{100}, fear{0};
    double shelter_until{-1};
    Activity activity{Activity::Sleep};
    int place{-1};
    Vector3 from{}, body{};          // abstract trip start; last physical position
    double depart{0}, arrive{0};     // island minutes
    int patrol_step{0};
    std::string full_name() const { return first_name+" "+family_name; }
};

}
