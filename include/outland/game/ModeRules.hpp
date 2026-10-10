#pragma once
#include "outland/game/GameMode.hpp"

namespace outland::game {
// The three player modes are interpretations of the same Verda: same roads, towns, buildings,
// vehicles and weapons. Rules only decide which layers of island life are switched on.
struct ModeRules {
    bool civilian_life{false};    // scheduled residents (homes, jobs, Sundays) - Explore
    bool survivor_factions{false}; // Esperantiza families, Green Star Federation - Zombie
    bool zombie_ecology{false};   // infected spawn sources, hordes, noise attraction - Zombie
    bool combat_bots{false};      // ranged AI opponents hunting the player - Battle Royale
    float ammo_scarcity{1};       // loot ammunition multiplier; <1 is scarcer
};

constexpr ModeRules rules_for(GameMode mode) {
    switch(mode) {
        case GameMode::Explore:return {true,false,false,false,1};
        case GameMode::ZombieSurvival:return {false,true,true,false,.65F};
        case GameMode::BattleRoyale:return {false,false,false,true,1};
        default:return {};
    }
}
}
