#include "outland/game/player/PlayerStats.hpp"
#include <cmath>
#include <iostream>
#include <string>
using namespace outland::game::player;
namespace {
int failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
} // namespace
int main() {
    PlayerStats stats;
    for (int i = 0; i < 1000; ++i) stats.drove(2.5F, 25);   // 2.5 km at 90 km/h
    stats.drove(400, 30);                                   // a respawn: not travel
    check(std::abs(stats.km_driven() - 2.5) < 1e-6, "driving adds up, a teleport doesn't: " + std::to_string(stats.km_driven()));
    check(std::abs(stats.top_kmh() - 108) < .01F, "the top speed is the fastest seen");
    stats.drove(1, 500);
    check(stats.top_kmh() < 200, "a glitch's speed isn't a record");
    for (int i = 0; i < 400; ++i) stats.walked(1.25F);
    check(std::abs(stats.km_walked() - .5) < 1e-6, "walking adds up");
    stats.kill(); stats.kill(); stats.died(); stats.arrests(3); stats.arrests(-1); stats.busted(); stats.hauled(); stats.race_won();
    stats.money(100, 350); stats.money(350, 300);
    for (int i = 0; i < 3600; ++i) stats.played(1);
    stats.played(60);   // a pause: not play
    check(stats.kills() == 2 && stats.deaths() == 1 && stats.arrests() == 3 && stats.times_busted() == 1, "the counts");
    check(stats.loads() == 1 && stats.races() == 1, "loads and races");
    check(stats.earned() == 250 && stats.spent() == 50, "money in and out");
    check(std::abs(stats.hours_played() - 1) < 1e-9, "time played");
    // Saved and loaded: the same record.
    PlayerStats loaded;
    check(loaded.parse(stats.serialise()), "the record loads");
    check(loaded.serialise() == stats.serialise(), "and is the same record");
    check(loaded.lines().size() == 12 && loaded.lines().front().label == "KM DRIVEN" && loaded.lines().front().value == "2.5", "the bag's lines");
    PlayerStats junk;
    check(!junk.parse("career 8 0") && junk.kills() == 0, "something else isn't a record");
    check(junk.parse("stats 2 kills=4 future=9 driven=-5"), "a newer record loads what it knows");
    check(junk.kills() == 4 && junk.km_driven() == 0, "and skips what it doesn't");
    if (failures == 0) std::cout << "player stats ok\n";
    return failures == 0 ? 0 : 1;
}
