#include "outland/world/World.hpp"

namespace outland::world {

World::World() = default;

void World::load_training_arena() {
    map_name_ = "OUTLAND Training Arena";
    loaded_ = true;
}

bool World::loaded() const {
    return loaded_;
}

const std::string& World::map_name() const {
    return map_name_;
}

}
