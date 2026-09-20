#pragma once
#include "../../../engine/core/geometry/vector2.h"

#include <functional>
#include <vector>

class BulletBehaviorSet;

struct Bullet_Attributes
{
    float bullet_speed = 500.0f;
    float max_age = 20.0f;

    elysia::core::Vector2 start_position{};
    elysia::core::Vector2 starting_velocity{};
    elysia::core::Vector2 bullet_size = {24.0f, 24.0f};

    float damage = 100.0f;

    float damage_cooldown_sec = 0.3;

    std::vector<std::function<void(BulletBehaviorSet &)>> behavior_appenders;
};