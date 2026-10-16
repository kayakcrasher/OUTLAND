#include "outland/game/combat/Grenades.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace outland::game::combat {

float Grenades::damage_at(float distance) {
    if (!std::isfinite(distance) || distance >= blast_radius) return 0;
    if (distance <= lethal_radius) return max_damage;
    const float t = (distance - lethal_radius) / (blast_radius - lethal_radius);
    return max_damage * std::pow(1 - t, 1.6F);
}

bool Grenades::throw_grenade(Vector3 from, Vector3 direction, Vector3 carried) {
    if (!ready() || Vector3Length(direction) < .001F) return false;
    const Vector3 aim = Vector3Normalize(direction);
    // Lob: aim plus a little lift, so a level throw lands about 25 m away.
    Vector3 velocity = Vector3Scale(Vector3Normalize(Vector3Add(aim, {0, .28F, 0})), throw_speed);
    velocity = Vector3Add(velocity, Vector3Scale(carried, .6F));
    live_.push_back({Vector3Add(from, Vector3Scale(aim, .5F)), velocity, fuse_seconds, 0, false});
    --count_;
    cooldown_ = cooldown_seconds;
    return true;
}

void Grenades::update(float dt, const GrenadeWorld& world) {
    exploded_.clear();
    exploded_remote_.clear();
    if (!std::isfinite(dt) || dt <= 0) return;
    dt = std::min(dt, .1F);
    cooldown_ = std::max(0.0F, cooldown_ - dt);
    for (auto& e : explosions_) e.age += dt;
    std::erase_if(explosions_, [](const Explosion& e) { return e.age > explosion_life; });
    for (auto& g : live_) {
        g.fuse -= dt;
        // Small substeps so a fast grenade cannot pass through a wall.
        const int steps = std::clamp(static_cast<int>(std::ceil(Vector3Length(g.velocity) * dt / .25F)), 1, 8);
        const float h = dt / static_cast<float>(steps);
        for (int s = 0; s < steps; ++s) {
            g.velocity.y -= 9.81F * h;
            g.velocity = Vector3Scale(g.velocity, std::exp(-.15F * h));
            const Vector3 next = Vector3Add(g.position, Vector3Scale(g.velocity, h));
            Vector3 point{}, normal{};
            if (world.collide && world.collide(g.position, next, point, normal)) {
                // Bounce: reflect, lose most of the speed into the surface and some along it.
                const float into = Vector3DotProduct(g.velocity, normal);
                Vector3 reflected = Vector3Subtract(g.velocity, Vector3Scale(normal, 2 * into));
                const Vector3 normal_part = Vector3Scale(normal, Vector3DotProduct(reflected, normal));
                const Vector3 tangent = Vector3Subtract(reflected, normal_part);
                g.velocity = Vector3Add(Vector3Scale(normal_part, .32F), Vector3Scale(tangent, .62F));
                g.position = Vector3Add(point, Vector3Scale(normal, .06F));
                if (Vector3Length(g.velocity) < .6F) g.velocity = {};
                ++g.bounces;
            } else {
                g.position = next;
            }
        }
        if (g.fuse <= 0) {
            explosions_.push_back({g.position, 0});
            exploded_.push_back(g.position);
            exploded_remote_.push_back(g.remote);
        }
    }
    std::erase_if(live_, [](const Grenade& g) { return g.fuse <= 0; });
}

} // namespace outland::game::combat
