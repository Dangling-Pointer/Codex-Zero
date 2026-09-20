#pragma once

#include "../../../../engine/physics/collision/collision_event.h"

class Bullet;
namespace elysia::core
{
    class GameObject;
}

struct BulletBehaviorContext
{
    Bullet &bullet;

    elysia::physics::CollisionTarget self_target{};
    elysia::physics::CollisionTarget other_target{};
    const elysia::physics::CollisionEvent *collision = nullptr;
    double delta_seconds = 0.0;

    [[nodiscard]] elysia::core::Vector2 collision_normal() const noexcept
    {
        if (!collision)
            return {};

        const auto &contact = collision->contact;
        return contact.pair.first == self_target
                   ? contact.manifold.normal
                   : -contact.manifold.normal;
    }
};