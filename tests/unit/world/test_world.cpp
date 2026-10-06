#include "outland/world/World.hpp"
#include "../../support/Test.hpp"

void test_world() {
    using outland::test::require;
    using outland::world::World;

    World world;

    require(
        !world.loaded(),
        "World should begin unloaded."
    );

    require(
        world.map_name() == "none",
        "Unloaded world should have map name 'none'."
    );

    world.load_training_arena();

    require(
        world.loaded(),
        "Training Arena should load successfully."
    );

    require(
        world.map_name() == "OUTLAND Training Arena",
        "Training Arena has incorrect map name."
    );
}
