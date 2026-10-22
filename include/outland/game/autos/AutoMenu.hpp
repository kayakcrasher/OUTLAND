#pragma once
#include <raylib.h>
#include <string>
#include <unordered_set>
#include <vector>

// The motor trade's counter (game::autos): a list down the right of the screen - the mechanic's
// jobs, the parts on offer, the cars for sale, the pump - with a title, the player's cash and a
// line of news. Mouse, touch or keys: UP/DOWN (W/S) to choose, ENTER (E) to take it,
// BACKSPACE/ESC to go back or leave.
namespace outland::game::autos {

struct MenuRow {
    std::string label, detail;   // "V8 big block", "+65% power, +15% top speed"
    std::string price;           // "7000 kr", "Fitted", ""
    bool enabled{true};
    bool current{false};         // what the car has now (marked)
};

struct MenuAction {
    int chosen{-1};              // a row was taken
    bool back{false};            // back (or leave, from the top)
};

class AutoMenu {
public:
    void open(std::string title, std::string subtitle, std::vector<MenuRow> rows, std::string back_label = "LEAVE", int selected = 0);
    void set_rows(std::vector<MenuRow> rows);
    void close() { open_ = false; }
    [[nodiscard]] bool is_open() const { return open_; }
    [[nodiscard]] int selected() const { return selected_; }
    void set_status(std::string status) { status_ = std::move(status); }
    void set_font(const Font& font) { font_ = font; }   // one with the Esperanto letters

    MenuAction input(int width, int height, bool focused);
    void draw(long long money, int width, int height) const;

private:
    [[nodiscard]] Rectangle row_rect(int index, int width, int height) const;
    [[nodiscard]] Rectangle back_rect(int width, int height) const;
    std::vector<Vector2> clicks(bool focused);
    Font font_{};
    bool open_{false}, warm_{true};
    std::string title_, subtitle_, back_label_, status_;
    std::vector<MenuRow> rows_;
    int selected_{0}, first_{0};
    std::unordered_set<int> previous_;
};

} // namespace outland::game::autos
