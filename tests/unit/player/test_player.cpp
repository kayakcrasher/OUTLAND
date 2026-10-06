#include "outland/game/player/Player.hpp"
#include "../../support/Test.hpp"

void test_player() {
    using outland::game::Player;
    using outland::test::require_near;

    Player player;

    require_near(
        player.x(),
        0.0,
        0.001,
        "Player X should begin at zero."
    );

    require_near(
        player.y(),
        0.0,
        0.001,
        "Player Y should begin at zero."
    );

    require_near(
        player.z(),
        0.0,
        0.001,
        "Player Z should begin at zero."
    );

    player.move(
        10.0F,
        2.0F,
        -5.0F
    );

    require_near(
        player.x(),
        10.0,
        0.001,
        "Player X movement failed."
    );

    require_near(
        player.y(),
        2.0,
        0.001,
        "Player Y movement failed."
    );

    require_near(
        player.z(),
        -5.0,
        0.001,
        "Player Z movement failed."
    );

    player.health().damage(40.0F);

    require_near(
        player.health().current(),
        60.0,
        0.001,
        "Player health integration failed."
    );
}
