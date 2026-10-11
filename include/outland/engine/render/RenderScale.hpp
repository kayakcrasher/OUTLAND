#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace outland::engine {
// Dynamic resolution for the 3D world. Phones are usually limited by pixels, not by game logic, so
// the world is drawn at a fraction of the screen and stretched (the HUD stays at full resolution).
// Every second the average frame time decides: slower than ~50 fps while waiting on the GPU steps
// the scale down, faster than ~58 fps for a few seconds steps it back up. OUTLAND_RENDER_SCALE=0.6 (0.35..1) fixes it.
class RenderScale {
public:
    static constexpr float minimum = .5F, maximum = 1.0F, step = .05F;
    RenderScale() {
        if (const char* fixed = std::getenv("OUTLAND_RENDER_SCALE"); fixed && *fixed) {
            const float value = static_cast<float>(std::atof(fixed));
            if (std::isfinite(value) && value > 0) {scale_ = std::clamp(value, .35F, 1.0F); fixed_ = true;}
        }
    }
    float scale() const { return scale_; }
    // Player setting: adapt (AUTO), or fix the scale (FAST 0.6, SHARP full resolution).
    // OUTLAND_RENDER_SCALE still wins when set.
    void set_mode(int mode) {
        if (std::getenv("OUTLAND_RENDER_SCALE")) return;
        fixed_ = mode != 0;
        if (mode == 1) scale_ = .6F;
        else if (mode == 2) scale_ = 1.0F;
        else scale_ = std::clamp(scale_, minimum, maximum);
        elapsed_ = submit_ = 0; frames_ = calm_ = 0;
    }
    bool fixed() const { return fixed_; }
    // Call once per frame with the last frame's duration and how much of it the CPU spent getting
    // the frame ready (everything before EndDrawing). A slow frame that was mostly CPU work is not
    // helped by fewer pixels, so then the resolution is left alone, or even raised.
    void update(float frame_seconds, float submit_seconds) {
        if (fixed_ || !std::isfinite(frame_seconds) || frame_seconds <= 0 || frame_seconds > .5F) return;
        elapsed_ += frame_seconds; ++frames_;
        if (std::isfinite(submit_seconds) && submit_seconds > 0) submit_ += std::min(submit_seconds, frame_seconds);
        if (elapsed_ < 1) return;
        const float average = elapsed_ / static_cast<float>(frames_);
        const float waiting = 1 - submit_ / elapsed_; // share of the frame spent waiting on the GPU
        elapsed_ = submit_ = 0; frames_ = 0;
        if (average > 1.0F / 50 && waiting > .3F) {scale_ = std::max(minimum, scale_ - step); calm_ = 0;}
        else if (average > 1.0F / 50) {scale_ = std::min(maximum, scale_ + step); calm_ = 0;}
        else if (average < 1.0F / 58 && ++calm_ >= 3) {scale_ = std::min(maximum, scale_ + step); calm_ = 0;}
        else if (average >= 1.0F / 58) calm_ = 0;
    }
private:
    float scale_{.75F};
    bool fixed_{false};
    float elapsed_{0}, submit_{0};
    int frames_{0}, calm_{0};
};
}
