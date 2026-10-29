#pragma once
#include "outland/world/VerdaRegion.hpp"
#include <raylib.h>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Verda's churches. The west of the island is Baptist (the Anglo side of its people: white
// clapboard chapels with a spire); the east is Esperanto Orthodox (the Slavic side: onion domes,
// icons and candles); Verda City has both. La Verda Mano are Baptists - the Ulster banner's people -
// and the city's chapel on the west boulevard is theirs.
//
// Verdan Baptists are not like Baptists anywhere else: centuries beside the Orthodox have left them
// keeping the saints - each chapel has its patron - and the same feasts on the same days, Easter to
// Saint Nicholas. What they hold to is the Book: la Reĝo-Jakoba Biblio, the King James in Esperanto,
// open on every lectern. Both churches keep Zamenhof Day.
namespace outland::game::culture {

enum class Denomination : std::uint8_t { Baptist, Orthodox };
const char* denomination_name(Denomination denomination);   // "BAPTISTA", "ORTODOKSA"

// A church found in the world (city kit civic/church_baptist.glb or church_orthodox.glb).
struct Church {
    Denomination denomination{Denomination::Baptist};
    Vector3 centre{}, door{};     // the door: in front of the porch, on the street
    float yaw{0};
    std::string settlement;
    const char* saint{""};        // its patron
    bool green_hand{false};       // La Verda Mano's (Verda City's Baptist chapel)
};
std::vector<Church> find_churches(const world::VerdaRegion& region);

// The feasts both churches keep, in turn through the island's year: one every feast_days days.
struct Feast { const char* name; const char* what; };
inline constexpr std::array<Feast, 8> feasts{{
    {"Kristnasko", "Christmas"},
    {"Teofanio", "the Theophany: the waters blessed, the young men dive for the cross"},
    {"Pasko", "Easter: bells all night, red eggs at dawn"},
    {"Pentekosto", "Pentecost: green branches over every door"},
    {"Sankta Petro kaj Paŭlo", "the fishermen's feast: the boats blessed in the harbours"},
    {"Sankta Mikaelo", "Michaelmas: the harvest in"},
    {"Sankta Nikolao", "Saint Nicholas: gifts for the children"},
    {"Zamenhof-Tago", "Zamenhof Day: the Esperantists' own, kept by both churches"},
}};
inline constexpr int feast_days = 4;
// The feast on island day `day` (-1: none), and the next one after it: which, and in how many days.
int feast_on(int day);
int next_feast(int day, int& in_days);

// What the player reads arriving at a church's door.
std::string church_notice(const Church& church, int day);

// Sundays and feast days. `sunday`: the island's weekday is Sunday; `day`: the island day;
// `minute`: of the day.
// The bells: on a Sunday before the service (9:10-9:30) and after it (11:30-11:45); on a feast at
// noon (12:00-12:20); and on Pasko through the first half hour of the night.
bool bells(bool sunday, int day, int minute);
// Who stands outside a church: the faithful through the Sunday service (9:15-11:45) and all of a
// feast day (10:00-18:00), with candles on Pasko night (0:00-1:30); on a Sunday, La Verda Mano's
// men in their suits outside their chapel. No gathering: people 0.
struct Gathering { int people{0}; int green_hand{0}; bool candles{false}; };
Gathering gathering(const Church& church, bool sunday, int day, int minute);
// A feast day is a day of peace: the gangs start no wars on it.
inline bool day_of_peace(int day) { return feast_on(day) >= 0; }
// The morning's word on a feast day ("" on other days).
std::string feast_morning(int day);

} // namespace outland::game::culture
