#pragma once
#include "outland/game/radio/Radio.hpp"
#include <raylib.h>
#include <string>

namespace outland::game::radio {

// Plays the car radio: streams what the tuned station has on air, from where it is now, and a
// burst of static when the dial turns. Only one song is ever loaded.
class RadioAudio {
public:
    ~RadioAudio();
    void update(CarRadio& radio, bool paused, float volume);
    void stop();

private:
    void unload();
    bool initialized_{false};
    Music music_{};
    std::string file_;
    Sound static_{};
    float volume_{-1};
};

} // namespace outland::game::radio
