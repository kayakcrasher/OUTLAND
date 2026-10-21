#pragma once
#include "outland/input/TouchLayout.hpp"
#include <map>
#include <string>
#include <vector>

// The player's own layout for the on-screen controls (SETTINGS > Touch layout): any button moved
// and resized. Each is kept by name as an override of the default TouchLayout (where it sits as a
// share of the screen, and its size); a button with no override sits where it always did.
namespace outland::input {

struct TouchCustom { float x{0}, y{0}, scale{1}; };
using TouchOverrides = std::map<std::string, TouchCustom>;

struct TouchElementInfo {
    const char* key;                    // saved name
    const char* label;                  // on the editor
    TouchElementLayout TouchLayout::* member;
    bool on_foot, driving;              // which set of controls it shows in
    bool stick;                         // the movement stick (bigger)
};
const std::vector<TouchElementInfo>& touch_elements();

inline constexpr float custom_scale_min = .5F, custom_scale_max = 2.0F;

// The default layout with the player's overrides on it.
inline TouchLayout customised(const TouchOverrides& overrides) {
    TouchLayout layout;
    for (const auto& e : touch_elements()) {
        const auto found = overrides.find(e.key);
        if (found == overrides.end()) continue;
        auto& element = layout.*e.member;
        element.x = std::clamp(found->second.x, .02F, .98F);
        element.y = std::clamp(found->second.y, .02F, .98F);
        element.scale = std::clamp(found->second.scale, custom_scale_min, custom_scale_max) * (TouchLayout{}.*e.member).scale;
    }
    return layout;
}

} // namespace outland::input
