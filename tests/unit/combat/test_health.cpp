#include "outland/game/combat/Health.hpp"
#include "../../support/Test.hpp"

void test_health() {
    using outland::game::Health;
    using outland::test::require;
    using outland::test::require_near;

    Health health{100.0F};

    require_near(
        health.current(),
        100.0,
        0.001,
        "Health should begin at maximum."
    );

    require(
        health.alive(),
        "Player should begin alive."
    );

    health.damage(25.0F);

    require_near(
        health.current(),
        75.0,
        0.001,
        "25 damage should leave 75 health."
    );

    health.heal(10.0F);

    require_near(
        health.current(),
        85.0,
        0.001,
        "Healing should restore health."
    );

    health.heal(1000.0F);

    require_near(
        health.current(),
        100.0,
        0.001,
        "Health must not exceed maximum."
    );

    health.damage(500.0F);

    require_near(
        health.current(),
        0.0,
        0.001,
        "Damage must not reduce health below zero."
    );

    require(
        !health.alive(),
        "Zero health should mean dead."
    );

    health.reset();

    require_near(
        health.current(),
        100.0,
        0.001,
        "Reset should restore maximum health."
    );
}
