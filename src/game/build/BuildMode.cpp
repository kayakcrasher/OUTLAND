#include "outland/game/build/BuildMode.hpp"
#include "outland/game/MapScreen.hpp"
#include "outland/input/PointerEvents.hpp"
#include "outland/world/VerdaRegion.hpp"
#include "outland/world/terrain/TerrainHeight.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>

namespace outland::game::build {

namespace {
float ground(float x, float z) { return world::terrain::TerrainHeight::sample(x, z); }
float flat(Vector3 a, Vector3 b) { return std::hypot(a.x - b.x, a.z - b.z); }
constexpr float reach = 45;
}

void BuildMode::set_active(bool active) {
    active_ = active;
    road_started_ = false;
    warm_ = true;
}

Rectangle BuildMode::toggle_button(int width, int height) {
    const auto map = MapScreen::map_button(width, height);
    return {map.x - 112, map.y, 104, map.height};
}

Rectangle BuildMode::button(Button which, int width, int) {
    // A bar across the top, under the status line: < PIECE >  TURN  PLACE  REMOVE  DONE
    const float h = 46, y = 172, gap = 8;
    const float widths[]{46, 46, 96, 120, 110, 96};
    float total = 190 + gap; // the piece name between < and >
    for (const float w : widths) total += w + gap;
    float x = std::max(8.0F, (width - total) * .5F);
    const auto at = [&](int index) {
        float left = x;
        for (int i = 0; i < index; ++i) left += widths[i] + gap + (i == 0 ? 190 + gap : 0);
        return Rectangle{left, y, widths[index], h};
    };
    return at(static_cast<int>(which));
}

bool BuildMode::owns_point(Vector2 point, int width, int height) const {
    if (CheckCollisionPointRec(point, toggle_button(width, height))) return true;
    if (!active_) return false;
    const auto first = button(Button::Previous, width, height), last = button(Button::Done, width, height);
    return CheckCollisionPointRec(point, {first.x, first.y, last.x + last.width - first.x, first.height + 34});
}

std::vector<Vector2> BuildMode::clicks(bool focused) {
    std::vector<Vector2> out;
    std::unordered_set<int> current;
    const auto buffered = input::PointerEvents::presses();
    if (focused && !warm_) for (const auto& p : buffered) out.push_back(p);
    const int count = std::max(0, GetTouchPointCount());
    for (int i = 0; i < count; ++i) {
        const int id = GetTouchPointId(i);
        if (id < 0 || !current.insert(id).second) continue;
        if (focused && !warm_ && buffered.empty() && !previous_touches_.contains(id)) out.push_back(GetTouchPosition(i));
    }
    if (focused && !warm_ && buffered.empty() && count == 0 && previous_touches_.empty() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        out.push_back(GetMousePosition());
    previous_touches_ = std::move(current);
    warm_ = false;
    return out;
}

bool BuildMode::toggled(int width, int height, bool focused) {
    bool hit = focused && IsKeyPressed(KEY_K);
    if (!active_) for (const auto& p : clicks(focused)) hit |= CheckCollisionPointRec(p, toggle_button(width, height));
    return hit;
}

Vector3 BuildMode::aim_ground(Vector3 camera, Vector3 look, Vector3 feet) const {
    const Vector3 direction = Vector3Normalize(look);
    for (float t = 2; t < 80; t += .5F) {
        const Vector3 p = Vector3Add(camera, Vector3Scale(direction, t));
        if (p.y <= ground(p.x, p.z)) {
            if (flat(p, feet) > reach) break;
            return {p.x, ground(p.x, p.z), p.z};
        }
    }
    // Looking at the sky or too far: a spot ahead, within reach.
    Vector3 ahead{direction.x, 0, direction.z};
    if (Vector3Length(ahead) < .01F) ahead = {0, 0, 1};
    ahead = Vector3Normalize(ahead);
    const float distance = std::min(reach, 16.0F);
    const Vector3 p{feet.x + ahead.x * distance, 0, feet.z + ahead.z * distance};
    return {p.x, ground(p.x, p.z), p.z};
}

// Road ends snap onto a road within 6 m (its nearest point), so new streets join the network.
Vector3 BuildMode::snap_road_end(const world::VerdaRegion& region, Vector3 p) const {
    float best = 6;
    Vector3 out = p;
    for (const auto& town : region.settlements()) for (const auto& road : town.roads) {
        for (const Vector3 end : {road.start, road.end})
            if (const float d = flat(end, p); d < best * .8F) { best = d / .8F; out = {end.x, 0, end.z}; }
        const float dx = road.end.x - road.start.x, dz = road.end.z - road.start.z, length2 = dx * dx + dz * dz;
        if (length2 < 1e-4F) continue;
        const float t = std::clamp(((p.x - road.start.x) * dx + (p.z - road.start.z) * dz) / length2, 0.0F, 1.0F);
        const Vector3 q{road.start.x + dx * t, 0, road.start.z + dz * t};
        if (const float d = flat(q, p); d < best) { best = d; out = q; }
    }
    out.y = ground(out.x, out.z);
    return out;
}

BuildMode::Result BuildMode::update(world::VerdaRegion& region, PlayerBuilds& builds, Vector3 camera, Vector3 look, Vector3 feet,
                                    int width, int height, bool focused) {
    Result result;
    bool previous = false, next = false, turn = false, place = false, remove = false, done = false;
    for (const auto& p : clicks(focused)) {
        if (CheckCollisionPointRec(p, button(Button::Previous, width, height))) previous = true;
        if (CheckCollisionPointRec(p, button(Button::Next, width, height))) next = true;
        if (CheckCollisionPointRec(p, button(Button::Turn, width, height))) turn = true;
        if (CheckCollisionPointRec(p, button(Button::Place, width, height))) place = true;
        if (CheckCollisionPointRec(p, button(Button::Remove, width, height))) remove = true;
        if (CheckCollisionPointRec(p, button(Button::Done, width, height)) || CheckCollisionPointRec(p, toggle_button(width, height))) done = true;
    }
    if (focused) {
        next |= IsKeyPressed(KEY_TAB);
        turn |= IsKeyPressed(KEY_R);
        place |= IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ENTER);
        remove |= IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE);
        done |= IsKeyPressed(KEY_K) || IsKeyPressed(KEY_ESCAPE);
        for (int k = 0; k < piece_count; ++k)
            if (IsKeyPressed(KEY_ONE + k)) { tool_ = static_cast<Piece>(k); road_started_ = false; }
    }
    if (done) { set_active(false); return result; }
    if (previous || next) {
        tool_ = static_cast<Piece>((static_cast<int>(tool_) + (next ? 1 : piece_count - 1)) % piece_count);
        road_started_ = false;
    }
    if (turn) { yaw_ = std::fmod(yaw_ + 45, 360); turned_ = true; }

    // The ghost: where the camera looks, on the ground.
    const Vector3 aim = aim_ground(camera, look, feet);
    ghost_ = {};
    ghost_.piece = tool_;
    ghost_.position = {std::round(aim.x * 2) / 2, 0, std::round(aim.z * 2) / 2};
    ghost_.position.y = ground(ghost_.position.x, ghost_.position.z);
    if (!turned_) {
        // Front door toward the player (procedural houses front on local -z).
        const float dx = feet.x - ghost_.position.x, dz = feet.z - ghost_.position.z;
        yaw_ = std::round(std::atan2(-dx, -dz) * RAD2DEG / 45) * 45;
    }
    ghost_.yaw = yaw_;
    if (tool_ == Piece::Road) {
        const Vector3 end = snap_road_end(region, ghost_.position);
        if (road_started_) { ghost_.position = road_start_; ghost_.end = end; }
        else ghost_.position = ghost_.end = end;
        if (road_started_) why_ = builds.check(region, ghost_);
        else why_ = ground(end.x, end.z) > .6F ? "" : "Too close to the sea";
    } else if (tool_ == Piece::BusStop) {
        Vector3 shelter{};
        float yaw = 0;
        if (PlayerBuilds::snap_bus_stop(region, ghost_.position, shelter, yaw)) {
            ghost_.position = shelter; ghost_.yaw = yaw;
            why_ = builds.check(region, ghost_);
        } else why_ = "Aim beside a road to put a bus stop there";
    } else why_ = builds.check(region, ghost_);
    ghost_valid_ = why_.empty();

    if (remove) {
        switch (builds.remove(aim, 6)) {
            case PlayerBuilds::Removal::Removed: result.changed = true; result.transit = true; notify("Removed", 2); break;
            case PlayerBuilds::Removal::Occupied: notify("A family lives there now", 2.5F); break;
            default: notify("Nothing you built is there", 2);
        }
        road_started_ = false;
    }
    if (place) {
        if (tool_ == Piece::Road && !road_started_) {
            if (ghost_valid_) { road_started_ = true; road_start_ = ghost_.position; notify("Now PLACE the other end", 3); }
            else notify(why_, 2.5F);
        } else if (!ghost_valid_) notify(why_, 2.5F);
        else {
            const Placed& made = builds.add(ghost_);
            result.changed = true;
            result.transit = tool_ == Piece::Road || tool_ == Piece::BusStop;
            if (tool_ == Piece::Road) {
                // Carry on from the end just laid: a street of several straight runs.
                road_start_ = made.end;
                notify("Road laid - PLACE again to carry on, or change piece", 3);
            } else if (made.home()) notify(builds.empty_homes() > 0 ? "Built! It stands empty until a bus brings newcomers to one of your stops" : "Built!", 4);
            else if (tool_ == Piece::BusStop) notify("Bus stop built: Linio 1 now calls here", 4);
        }
    }
    return result;
}

void BuildMode::draw_world(float time_seconds) const {
    if (!active_) return;
    const float pulse = .5F + .5F * std::sin(time_seconds * 5);
    const Color fill = ghost_valid_ ? Color{80, 220, 110, static_cast<unsigned char>(70 + 40 * pulse)} : Color{235, 70, 60, 90};
    const Color edge = ghost_valid_ ? Color{120, 255, 150, 255} : Color{255, 90, 80, 255};
    const auto& info = piece_info(ghost_.piece);
    if (ghost_.piece == Piece::Road) {
        const Vector3 a = ghost_.position, b = road_started_ ? ghost_.end : Vector3Add(ghost_.position, {.01F, 0, 0});
        const float length = flat(a, b);
        const Vector3 along = length > .01F ? Vector3Normalize({b.x - a.x, 0, b.z - a.z}) : Vector3{1, 0, 0};
        const Vector3 side{-along.z * info.size.x * .5F, 0, along.x * info.size.x * .5F};
        const int steps = std::max(1, static_cast<int>(length / 3));
        rlDisableBackfaceCulling();
        for (int i = 0; i < steps; ++i) {
            const Vector3 p = Vector3Lerp(a, b, static_cast<float>(i) / steps), q = Vector3Lerp(a, b, static_cast<float>(i + 1) / steps);
            const Vector3 p0{p.x + side.x, ground(p.x + side.x, p.z + side.z) + .15F, p.z + side.z}, p1{p.x - side.x, ground(p.x - side.x, p.z - side.z) + .15F, p.z - side.z};
            const Vector3 q0{q.x + side.x, ground(q.x + side.x, q.z + side.z) + .15F, q.z + side.z}, q1{q.x - side.x, ground(q.x - side.x, q.z - side.z) + .15F, q.z - side.z};
            DrawTriangle3D(p0, q0, q1, fill); DrawTriangle3D(p0, q1, p1, fill);
            DrawLine3D(p0, q0, edge); DrawLine3D(p1, q1, edge);
        }
        rlEnableBackfaceCulling();
        DrawCylinder({a.x, a.y, a.z}, .25F, .25F, 3, 8, edge);
        if (road_started_) DrawCylinder({b.x, ground(b.x, b.z), b.z}, .25F, .25F, 3, 8, edge);
        return;
    }
    const Vector3 c = ghost_.position;
    rlPushMatrix();
    rlTranslatef(c.x, c.y, c.z);
    rlRotatef(ghost_.yaw, 0, 1, 0);
    const Vector3 size = ghost_.piece == Piece::Tree || ghost_.piece == Piece::Lamp ? Vector3{1.2F, info.size.y, 1.2F} : info.size;
    DrawCube({0, size.y * .5F, 0}, size.x, size.y, size.z, fill);
    DrawCubeWires({0, size.y * .5F, 0}, size.x, size.y, size.z, edge);
    if (ghost_.home()) {
        // The front door, so the player can see which way the house will face.
        DrawCube({0, 1.1F, -size.z * .5F - .05F}, 1.2F, 2.2F, .12F, edge);
        DrawCube({0, .05F, -size.z * .5F - 2.0F}, 1.4F, .1F, 3.4F, fill);
    }
    rlPopMatrix();
}

void BuildMode::draw_hud(const PlayerBuilds& builds, int width, int height) const {
    // BUILD toggle beside MAP.
    const auto toggle = toggle_button(width, height);
    DrawRectangleRec(toggle, active_ ? Color{40, 120, 60, 220} : Fade(BLACK, .6F));
    DrawText("BUILD · K", static_cast<int>(toggle.x) + 12, static_cast<int>(toggle.y) + 10, 14, RAYWHITE);
    if (note_time_ > 0 && !note_.empty()) {
        const int w = MeasureText(note_.c_str(), 18);
        const float alpha = std::clamp(note_time_ * 2, 0.0F, 1.0F);
        const int y = active_ ? 262 : height / 2 - 120;
        DrawRectangle(width / 2 - w / 2 - 12, y - 6, w + 24, 30, Fade(BLACK, .65F * alpha));
        DrawText(note_.c_str(), width / 2 - w / 2, y, 18, Fade(Color{255, 236, 170, 255}, alpha));
    }
    if (!active_) return;
    const auto draw_button = [&](Button which, const char* label, Color tint) {
        const auto r = button(which, width, height);
        DrawRectangleRec(r, Fade(tint, .85F));
        DrawRectangleLinesEx(r, 1.5F, Fade(RAYWHITE, .5F));
        DrawText(label, static_cast<int>(r.x + r.width / 2 - MeasureText(label, 18) / 2.0F), static_cast<int>(r.y + 14), 18, RAYWHITE);
    };
    const Color dark{30, 34, 38, 255};
    draw_button(Button::Previous, "<", dark);
    draw_button(Button::Next, ">", dark);
    const auto left = button(Button::Previous, width, height), right = button(Button::Next, width, height);
    const Rectangle name{left.x + left.width + 8, left.y, right.x - left.x - left.width - 16, left.height};
    DrawRectangleRec(name, Fade(Color{20, 70, 40, 255}, .9F));
    const char* piece = piece_info(tool_).name;
    DrawText(piece, static_cast<int>(name.x + name.width / 2 - MeasureText(piece, 20) / 2.0F), static_cast<int>(name.y + 7), 20, RAYWHITE);
    const char* number = TextFormat("%d / %d", static_cast<int>(tool_) + 1, piece_count);
    DrawText(number, static_cast<int>(name.x + name.width / 2 - MeasureText(number, 10) / 2.0F), static_cast<int>(name.y + 31), 10, Fade(RAYWHITE, .7F));
    draw_button(Button::Turn, "TURN", dark);
    draw_button(Button::Place, tool_ == Piece::Road && !road_started_ ? "START" : "PLACE", ghost_valid_ ? Color{40, 140, 70, 255} : Color{110, 60, 55, 255});
    draw_button(Button::Remove, "REMOVE", dark);
    draw_button(Button::Done, "DONE", dark);
    // Status: what this piece is, or why it can't go here; and how the town is doing.
    const std::string status = ghost_valid_ ? piece_info(tool_).hint : why_;
    const int sw = MeasureText(status.c_str(), 16);
    DrawRectangle(width / 2 - sw / 2 - 10, 226, sw + 20, 24, Fade(BLACK, .6F));
    DrawText(status.c_str(), width / 2 - sw / 2, 230, 16, ghost_valid_ ? Color{200, 240, 200, 255} : Color{255, 160, 150, 255});
    int homes = 0, stops = 0, people = 0;
    for (const auto& p : builds.placed()) { homes += p.home(); stops += p.piece == Piece::BusStop; people += p.residents; }
    const char* town = TextFormat("YOUR TOWN: %d homes (%d empty), %d bus stops, %d residents   F place  R turn  Tab piece  Bksp remove  K done",
                                  homes, builds.empty_homes(), stops, people);
    DrawText(town, 16, height - 24, 14, Fade(RAYWHITE, .85F));
}

void BuildMode::notify(std::string text, float seconds) {
    note_ = std::move(text);
    note_time_ = seconds;
}

} // namespace outland::game::build
