#pragma once
#include <cstdint>
#include <vector>

// Time inside Ŝtata Malliberejo for the island's people (Explore). Anyone the Policio books in -
// a hit-and-run driver pulled over, a Krimulo an officer brings to the gate - does one game week.
// Inside there are two ways it goes: an inmate joins one of the gangs for protection, or he
// doesn't, and somebody puts a shank in him. A shanked inmate goes to the hospital and is let out
// from there; the rest walk out of the gate when the week is up. A gangster booked in is with his
// gang already.
//
// Times are island minutes (game::life's clock). The rules only; the game moves the bodies
// (game::law::PrisonYard) and the residents (game::life::LifeSimulation).
namespace outland::game::law {

class JailTerms {
public:
    static constexpr double week = 7 * 24 * 60;         // island minutes
    static constexpr double decide_min = 6 * 60, decide_max = 48 * 60;   // when inside it comes to it
    static constexpr float join_chance = .6F;            // ...joins a gang (else: the shank)
    static constexpr int gangs = 5;

    enum class Fate : std::uint8_t { Undecided, Joined, Shanked, Released };
    struct Term {
        int inmate{-1};       // the yard's body (PrisonYard bot id), -1 none (the yard was full)
        int resident{-1};     // island life's resident, -1 none (a gangster)
        double admitted{0}, until{0}, decide_at{0};
        Fate fate{Fate::Undecided};
        int gang{-1};
    };
    struct Event { Fate fate{Fate::Undecided}; Term term; };

    void seed(std::uint64_t seed) { rng_ = seed | 1; }
    // Booked in at `now`: a week from now he's out. A `gangster` is with his gang already.
    void book(int inmate, int resident, double now, bool gangster = false) {
        Term t;
        t.inmate = inmate;
        t.resident = resident;
        t.admitted = now;
        t.until = now + week;
        t.decide_at = now + decide_min + (decide_max - decide_min) * random();
        if (gangster) t.fate = Fate::Joined;
        terms_.push_back(t);
    }
    // What happened since the last call: gangs joined, shankings (out of the term list, to the
    // hospital) and releases (out of the gate).
    std::vector<Event> update(double now) {
        std::vector<Event> out;
        for (auto& t : terms_) {
            if (t.fate == Fate::Undecided && now >= t.decide_at) {
                if (random() < join_chance) {
                    t.fate = Fate::Joined;
                    t.gang = static_cast<int>(random() * gangs) % gangs;
                } else {
                    t.fate = Fate::Shanked;
                }
                out.push_back({t.fate, t});
            }
            if (t.fate != Fate::Shanked && now >= t.until) {
                t.fate = Fate::Released;
                out.push_back({t.fate, t});
            }
        }
        std::erase_if(terms_, [](const Term& t) { return t.fate == Fate::Shanked || t.fate == Fate::Released; });
        return out;
    }
    // The yard gave him a new body (in his gang's colours).
    void moved(int inmate, int to) { for (auto& t : terms_) if (t.inmate == inmate) t.inmate = to; }
    void clear() { terms_.clear(); }

    [[nodiscard]] const std::vector<Term>& terms() const { return terms_; }
    [[nodiscard]] const Term* of_resident(int resident) const {
        for (const auto& t : terms_) if (t.resident == resident) return &t;
        return nullptr;
    }

private:
    float random() {
        rng_ ^= rng_ << 13; rng_ ^= rng_ >> 7; rng_ ^= rng_ << 17;
        return static_cast<float>(rng_ >> 40) / static_cast<float>(1ULL << 24);
    }
    std::vector<Term> terms_;
    std::uint64_t rng_{0x2545f4914f6cdd1dULL};
};

} // namespace outland::game::law
