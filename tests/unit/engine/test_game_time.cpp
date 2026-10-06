#include "outland/engine/core/GameTime.hpp"
#include "../../support/Test.hpp"

void test_game_time() {
    using outland::engine::GameTime;
    using outland::test::require_near;

    GameTime time;

    time.update(0.016);

    require_near(
        time.delta_seconds(),
        0.016,
        0.000001,
        "GameTime delta incorrect."
    );

    require_near(
        time.total_seconds(),
        0.016,
        0.000001,
        "GameTime total incorrect."
    );

    time.update(0.020);

    require_near(
        time.total_seconds(),
        0.036,
        0.000001,
        "GameTime accumulation failed."
    );

    time.update(-10.0);

    require_near(
        time.delta_seconds(),
        0.0,
        0.000001,
        "Negative delta time should clamp to zero."
    );

    require_near(
        time.total_seconds(),
        0.036,
        0.000001,
        "Negative delta must not alter total time."
    );
}
