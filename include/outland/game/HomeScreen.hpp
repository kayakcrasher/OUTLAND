#pragma once

#include "outland/game/GameMode.hpp"
#include "outland/game/ai/CombatBot.hpp"
#include "outland/game/campaign/Campaign.hpp"
#include <array>
#include <unordered_set>

namespace outland::game {

class HomeScreen {
public:
    GameMode update(
        int screen_width,
        int screen_height
    );

    void draw(
        int screen_width,
        int screen_height
    ) const;

    static Rectangle mode_button(int index,int width,int height);
    static Rectangle difficulty_button(int width,int height);
    static Rectangle lan_button(int width,int height);
    // LAN PLAY was pressed (the LAN screen opens); read once.
    bool take_lan() { const bool lan=lan_; lan_=false; return lan; }

    // CAMPAIGN opens the three save slots: pick one to play (a used one carries on, an empty one
    // starts the story); ERASE (tapped twice) clears one; BACK closes them.
    static constexpr int slot_count = 3;
    static Rectangle slot_button(int index, int width, int height);
    static Rectangle erase_button(int index, int width, int height);
    static Rectangle back_button(int width, int height);
    [[nodiscard]] bool slots_open() const { return slots_open_; }
    void set_slots(const std::array<campaign::SlotInfo, slot_count>& slots) { slots_ = slots; }
    // The slot chosen with CAMPAIGN (1-3), and a slot to erase (read once; 0 if none).
    [[nodiscard]] int slot() const { return slot_; }
    int take_erase() { const int e = erase_; erase_ = 0; return e; }
    // Dev builds: pick this slot straight away when CAMPAIGN is chosen (screenshots).
    void set_auto_slot(int slot) { auto_slot_ = slot; }

    // The language of the screen (SETTINGS > Language), and the font for Esperanto's letters.
    static void set_language(const Font* font, bool esperanto);

    // Battle Royale opponent skill, cycled from the menu.
    [[nodiscard]] ai::BotLevel bot_level() const { return bot_level_; }
    void set_bot_level(ai::BotLevel level) { bot_level_ = level; }

private:
    std::unordered_set<int> previous_;
    ai::BotLevel bot_level_{ai::BotLevel::Medium};
    bool lan_{false};
    bool slots_open_{false};
    std::array<campaign::SlotInfo, slot_count> slots_{};
    int slot_{1}, erase_{0}, erase_armed_{0}, auto_slot_{0};
    double erase_until_{0};
    GameMode selected_{
        GameMode::Home
    };
};

} // namespace outland::game
