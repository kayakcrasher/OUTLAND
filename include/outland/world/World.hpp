#pragma once

#include <string>

namespace outland::world {

class World {
public:
    World();

    void load_training_arena();

    [[nodiscard]]
    bool loaded() const;

    [[nodiscard]]
    const std::string& map_name() const;

private:
    bool loaded_{false};
    std::string map_name_{"none"};
};

}
