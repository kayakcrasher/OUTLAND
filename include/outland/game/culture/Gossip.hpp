#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Word gets around (Explore). What happens on the island - a town changing hands, a hit and run,
// a ballgame, a race on the ring - is heard by the island, and the people the player stops to talk
// to (USE beside a resident) pass it on, their own town's news first and the freshest first. What
// the player did is told too, with the car they did it in: "Somebody in a racing red Verda Tempesto
// ran a man down in Roka and never stopped." With nothing new, it's small talk: the weather, the
// next feast, the ballgame.
namespace outland::game::culture {

struct Rumour {
    std::string text;       // as told: "the Iron Dogs took Roka from the Frogs"
    double at{0};           // island minutes
    std::string town;       // settlement id it happened in ("" anywhere)
    bool about_you{false};  // the player did it
    int told{0};            // how many times it has been passed on
};

class Gossip {
public:
    static constexpr double keep_minutes = 3 * 24 * 60;   // three island days, then it's old news
    static constexpr std::size_t keep = 32;

    // Something happened. A headline ("HIT AND RUN: a car hit someone") is turned into talk; the
    // same story twice within an hour is heard once.
    void hear(std::string_view text, double now, std::string town = {}, bool about_you = false);
    // What a resident says (`job` their occupation, e.g. "mechanic"; `town` where they are; `seed`
    // picks among the ways of saying it). `small_talk` is used when there is no news.
    std::string tell(double now, std::string_view town, std::uint32_t seed, std::string_view small_talk);
    [[nodiscard]] const std::vector<Rumour>& rumours() const { return rumours_; }

    // "HIT AND RUN: a car hit someone and drove on" -> "a car hit someone and drove on".
    static std::string as_talk(std::string_view headline);
    // How the island describes the player's car: "a racing red Verda Tempesto" (paint name lower-case).
    static std::string car(std::string_view paint, std::string_view model);

private:
    void forget(double now);
    std::vector<Rumour> rumours_;
};

} // namespace outland::game::culture
