#pragma once
#include "outland/game/Settings.hpp"
#include <raylib.h>

// SETTINGS > Touch layout > EDIT: the on-screen buttons laid out over the screen, to drag where the
// thumbs want them and make bigger or smaller (- / +), one set at a time (on foot, at the wheel).
// RESET puts the chosen button back, RESET ALL every one; DONE keeps the layout (saved with the
// settings), CANCEL leaves it as it was.
namespace outland::game {

class TouchLayoutEditor {
public:
    enum class Result { None, Done, Cancel };
    void open(const GameSettings& settings);
    [[nodiscard]] bool is_open() const { return open_; }
    Result update(GameSettings& settings, int width, int height, bool focused);
    void draw(const GameSettings& settings, int width, int height, const Font* font) const;

    // Layout (screen space), shared with tests: the tool bar's buttons, left to right.
    static constexpr int tool_count = 7;   // set, smaller, bigger, reset, reset all, cancel, done
    static Rectangle tool(int index, int width, int height);
    // Where a button of the layout is drawn, and how big.
    static Vector2 centre(const input::TouchElementInfo& element, const input::TouchLayout& layout, int width, int height);
    static float radius(const input::TouchElementInfo& element, const input::TouchLayout& layout, float button_size, int width, int height);

private:
    bool open_{false}, driving_{false}, warm_{true};
    input::TouchOverrides work_;
    int selected_{-1}, dragging_{-1};
    Vector2 grab_{};
    bool was_down_{false};
};

} // namespace outland::game
