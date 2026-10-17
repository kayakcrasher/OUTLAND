// Territory: towns start with their county's gang; kills, tags and raids wear a hold down and
// the town flips; own tags and contraband shore it up; families ally, pay or fight; the island.
#include "outland/game/mafia/Territory.hpp"
#include <cmath>
#include <iostream>
using namespace outland::game::mafia;
namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
Territory island() {
    Territory t;
    t.add_town("verda", "Verda", {0, 0, 0}, 200, 0, 0);
    t.add_town("roka", "Roka", {1000, 0, 0}, 150, 4, 1);
    t.add_town("pinejo", "Pinejo", {1300, 0, 300}, 120, 4, 2);
    t.add_town("espera", "Espera", {0, 0, -1500}, 150, 1, 3);
    for (int r = 0; r < family_count; ++r)
        t.add_ranch("ranch" + std::to_string(r), "Ranch " + std::to_string(r), {-800.0F + 400.0F * r, 0, 900}, 120, r,
                    "Family" + std::to_string(r), r % gang_count, 0);
    return t;
}
}

int main() {
    auto t = island();
    check(t.turf_at({10, 0, 10}) == 0 && t.turf_at({990, 0, 0}) == 1 && t.turf_at({5000, 0, 0}) == -1, "turf by place");
    check(t.towns_of(0) == 1 && t.towns_of(4) == 2, "towns by county");
    // Families: allied, fierce, open.
    int allied = 0, fierce = 0, open = 0;
    for (const auto& f : t.families()) {
        if (f.stance == Stance::Allied) ++allied;
        else if (f.fierce) ++fierce;
        else ++open;
    }
    check(allied == 2 && fierce == 2 && open == 1, "two allied, two fierce, one that deals");

    // La Verda Mano (0) moves on Roka (Feraj Hundoj, 4): kills, tags.
    t.killed(4, {1000, 0, 10}, 0);
    check(t.turfs()[1].hold < 100 && t.turfs()[1].pressure[0] > 0, "a kill on the owner's crew wears the hold");
    t.killed(2, {1000, 0, 10}, 0);
    check(t.turfs()[1].pressure[0] == Territory::kill_wear, "killing someone else's man there does nothing");
    const float before = t.turfs()[1].hold;
    check(t.tag({1010, 2, 5}, {0, 0, 1}, 0) == 1 && t.turfs()[1].hold < before, "a rival tag wears it");
    const float after = t.turfs()[1].hold;
    t.tag({1010.5F, 2, 5}, {0, 0, 1}, 0);
    check(t.turfs()[1].hold == after && t.tags().size() == 1, "re-spraying your own tag counts once");
    t.tag({1010.2F, 2, 5}, {0, 0, 1}, 4);
    check(t.tags().size() == 1 && t.tags()[0].gang == 4 && t.turfs()[1].hold > after, "the owner paints over it and shores up");
    // Keep at it: Roka flips to La Verda Mano.
    for (int i = 0; i < 20 && t.turfs()[1].owner == 4; ++i) t.killed(4, {1000, 0, 0}, 0);
    check(t.turfs()[1].owner == 0 && t.turfs()[1].hold == 50, "Roka falls");
    const auto flips = t.take_flips();
    check(flips.size() == 1 && flips[0].turf == 1 && flips[0].from == 4 && flips[0].to == 0 && t.take_flips().empty(), "the flip, once");
    check(t.towns_of(0) == 2 && t.towns_of(4) == 1, "La Verda Mano now two towns");
    // Contraband home shores up a town (yours only).
    t.killed(0, {1000, 0, 0}, 4);
    const float worn = t.turfs()[1].hold;
    t.delivered(1, 0);
    check(t.turfs()[1].hold > worn, "a delivery shores up");
    t.delivered(0, 4);
    check(t.turfs()[0].hold == 100, "not someone else's");

    // Families: extort the open one, the fierce fight first, the allied bring their gang.
    int open_ranch = -1, fierce_ranch = -1, allied_ranch = -1;
    for (int r = 0; r < family_count; ++r) {
        const auto& f = t.families()[static_cast<std::size_t>(r)];
        if (f.stance == Stance::Allied && allied_ranch < 0) allied_ranch = r;
        else if (f.stance == Stance::Independent && f.fierce && fierce_ranch < 0) fierce_ranch = r;
        else if (f.stance == Stance::Independent && !f.fierce) open_ranch = r;
    }
    check(t.extort(open_ranch, 0) && t.families()[static_cast<std::size_t>(open_ranch)].stance == Stance::Extorted, "the open family pays");
    check(!t.extort(fierce_ranch, 0), "the fierce fight");
    t.subdued(fierce_ranch, 0);
    check(t.families()[static_cast<std::size_t>(fierce_ranch)].gang == 0, "until beaten");
    const int their_gang = t.families()[static_cast<std::size_t>(allied_ranch)].gang;
    check(their_gang == 0 || !t.extort(allied_ranch, 0), "an allied family's gang stands by them");
    check(t.ranches_with(0) >= 2, "ranches with La Verda Mano");

    // AI raids: a push now and then from a gang with a town, never the player's gang.
    int raids = 0;
    for (int s = 0; s < 2000; ++s) {
        const auto raid = t.update(1, s, 0);
        if (raid.turf >= 0) {
            ++raids;
            check(raid.attacker != 0 && t.turfs()[static_cast<std::size_t>(raid.turf)].ranch < 0, "an AI gang raids a town");
        }
    }
    check(raids >= 8 && raids <= 25, "every couple of minutes");

    // The island: every town and every family.
    auto all = island();
    check(!all.rules_island(0), "not yet");
    for (int i = 0; i < 4; ++i) for (int k = 0; k < 30 && all.turfs()[static_cast<std::size_t>(i)].owner != 0; ++k)
        all.killed(all.turfs()[static_cast<std::size_t>(i)].owner, all.turfs()[static_cast<std::size_t>(i)].centre, 0);
    for (int r = 0; r < family_count; ++r) all.subdued(r, 0);
    check(all.rules_island(0) && all.ranking().front() == 0, "La Verda Mano rules Verda");

    // Saving.
    Territory back = island();
    check(back.parse(t.serialise()) && back.turfs()[1].owner == 0 && back.tags().size() == t.tags().size() &&
          back.families()[static_cast<std::size_t>(open_ranch)].stance == Stance::Extorted, "saved and loaded");
    check(!back.parse("territory 9") && !back.parse("territory 1 turf roka 99 50"), "bad saves refused");
    check(back.parse("territory 1\nturf roka 1 60\n") && back.turfs()[1].owner == 1 && !back.turfs()[1].cleared, "older saves load");

    // Honour and defence: a raider down on your own turf shores it up; a just gang's towns recover faster.
    {
        auto d = island();
        d.killed(0, {10, 0, 10}, 4);
        const float worn = d.turfs()[0].hold;
        d.defended(0, 0);
        check(d.turfs()[0].hold > worn, "defending the town shores up its hold");
        auto slow = d, quick = d;
        slow.update(30, 1, 0, 1);
        quick.update(30, 1, 0, 2);
        check(quick.turfs()[0].hold > slow.turfs()[0].hold, "an honourable gang's hold comes back faster");
    }
    // The Policio clears a town: nobody's turf, out of every count, saved.
    {
        auto p = island();
        p.clear_town(1);
        check(p.turfs()[1].cleared && p.towns_of(4) == 1 && !p.controls(1, 4) && p.cleared() == 1, "a cleared town is nobody's");
        p.killed(4, {1000, 0, 0}, 0);
        check(p.turfs()[1].owner == 4 && p.turfs()[1].hold == 100, "and nobody can fight over it");
        Territory q = island();
        check(q.parse(p.serialise()) && q.turfs()[1].cleared, "a cleared town stays cleared");
        // Yin and yang: a quarter of the towns cleared, a quarter La Verda Mano's.
        const auto b = p.balance(0);
        check(std::abs(b.law - .25F) < .01F && std::abs(b.crime - .25F) < .01F, "the law's share and the crew's");
        check(p.balance(-1).crime == 0, "no crew, no crime share");
        // The Chief is dead: the gang takes the island, cleared towns and families too.
        p.take_island(0);
        check(p.rules_island(0) && p.cleared() == 0 && p.balance(0).crime == 1 && p.balance(0).law == 0, "the gang takes Verda");
        check(!p.take_flips().empty(), "the towns that changed hands are told");
    }
    if (failures) return 1;
    std::cout << "[PASS] Territory: holds, kills, tags, flips, families, raids, the island\n";
    return 0;
}
