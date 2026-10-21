#pragma once
#include "outland/game/Settings.hpp"
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <vector>

// The pause menu, as in other games: ESC (or the MENU button) stops play and offers RESUME,
// SETTINGS (look sensitivity, aiming, touch controls, sound, graphics), CONTROLS (every key and
// mouse button, rebindable), SAVE GAME, RESTART (tap twice: saves, then starts the mode afresh),
// QUIT TO MENU and, on a PC, QUIT GAME.
namespace outland::game {

class PauseMenu {
public:
    enum class Page { Main, Settings, Controls };
    enum class Result { None, Resume, SettingsChanged, Save, Restart, QuitHome, QuitGame, EditTouchLayout };
    // The settings rows, top to bottom.
    enum class Row {
        Mouse, Touch, AimSensitivity, InvertY, AimToggle, AimAssist, TouchControls, ButtonSize, TouchLayout, Vibration, Volume, RadioVolume, Sound, Graphics,
        Count
    };
    static constexpr int row_count = static_cast<int>(Row::Count);

    [[nodiscard]] bool is_open() const { return open_; }
    void open();
    void close() { open_ = false; capturing_ = -1; }
    [[nodiscard]] Page page() const { return page_; }
    [[nodiscard]] bool capturing() const { return capturing_ >= 0; }
    void set_status(std::string status, float seconds = 2.5F) { status_ = std::move(status); status_time_ = seconds; }

    // The top bar's MENU button (where AUDIO was), for touch screens and mice alike.
    static Rectangle menu_button(int width, int height);
    bool tapped_menu_button(int width, int height, bool focused);
    // While open: clicks and taps on its buttons, ESC goes back a page (or resumes), and on the
    // CONTROLS page the next key or mouse button after a click on a slot is bound to it.
    Result update(GameSettings& settings, int width, int height, bool focused, float dt, bool can_quit_game);
    void draw(const GameSettings& settings, int width, int height, const Font* font, bool can_quit_game) const;

    // Layout (screen space), shared by drawing, hit testing and tests.
    static Rectangle panel(int width, int height, Page page);
    static Rectangle main_button(int index, int width, int height);   // 0 resume .. 4 restart .. 6 quit game
    static Rectangle row_button(Row row, int side, int width, int height); // side 0 "-", 1 "+" (or the toggle as 1)
    static Rectangle slot(int action, int slot, int width, int height);
    static Rectangle footer_button(int index, Page page, int width, int height); // 0 back, 1 reset

private:
    std::vector<Vector2> clicks(bool focused);
    bool open_{false}, warm_{true};
    Page page_{Page::Main};
    int capturing_{-1}, capture_slot_{0}, capture_wait_{0};
    std::string status_;
    float status_time_{0};
    float restart_armed_{0};   // RESTART was tapped once; a second tap within this time restarts
    std::unordered_set<int> previous_;
};

} // namespace outland::game
