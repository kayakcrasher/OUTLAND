#include "outland/game/feel/Haptics.hpp"
#include <cassert>
#include <iostream>
#include <vector>

// The phone's vibration: one buzz per frame (the strongest asked for), a weaker one never cuts a
// stronger one short, and the settings switch turns it all off.
using outland::game::feel::Haptics;
using outland::game::combat::WeaponId;
struct Pulse { int ms; float strength; };

int main() {
    std::vector<Pulse> sent;
    Haptics haptics([&](int ms, float strength) { sent.push_back({ms, strength}); });
    // Every shot kicks: light for the SMG, heavy for the shotgun.
    haptics.shot(WeaponId::Smg); haptics.update(.02F);
    assert(sent.size() == 1 && sent[0].ms < 20 && sent[0].strength < .5F);
    haptics.update(.02F);
    haptics.shot(WeaponId::Shotgun); haptics.update(.02F);
    assert(sent.size() == 2 && sent[1].ms >= 40 && sent[1].strength > .8F);
    // A shot and a hit in the same frame: the hit (stronger) goes out.
    haptics.update(.1F);
    haptics.shot(WeaponId::Smg); haptics.hit_taken(30); haptics.update(.02F);
    assert(sent.size() == 3 && sent[2].ms > 100);
    // While that jolt runs, light shots do not interrupt it; afterwards they kick again.
    haptics.shot(WeaponId::Smg); haptics.update(.02F);
    assert(sent.size() == 3);
    haptics.update(.2F);
    haptics.shot(WeaponId::Smg); haptics.update(.02F);
    assert(sent.size() == 4);
    // Crashes and blasts by how hard and how close; bumps, far blasts and small hops do nothing.
    haptics.update(.1F);
    haptics.crash(2); haptics.explosion(60); haptics.land(8); haptics.update(.02F);
    assert(sent.size() == 4);
    haptics.crash(25); haptics.update(.02F);
    assert(sent.size() == 5 && sent[4].ms > 200 && sent[4].strength == 1.0F);
    haptics.update(.5F);
    haptics.explosion(5); haptics.update(.02F);
    const auto close = sent.back();
    haptics.update(.5F);
    haptics.explosion(30); haptics.update(.02F);
    assert(sent.back().ms < close.ms && sent.back().strength < close.strength);
    haptics.update(.5F);
    haptics.hit_given(false); haptics.update(.02F);
    assert(sent.size() == 7); // plain hits stay silent
    haptics.hit_given(true); haptics.land(15); haptics.update(.02F);
    assert(sent.size() == 8 && sent.back().ms == 70);
    // Off in the settings: nothing at all.
    haptics.update(.5F);
    haptics.set_enabled(false);
    haptics.shot(WeaponId::Sniper); haptics.crash(30); haptics.update(.02F);
    assert(sent.size() == 8);
    std::cout << "[PASS] Haptics: a kick per shot by gun, hits, kills, crashes, blasts and landings; strongest wins; switch off\n";
}
