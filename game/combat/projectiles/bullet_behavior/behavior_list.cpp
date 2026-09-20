#include "behavior_list.h"

#include "../../../engine/core/geometry/vector2.h"
#include "../bullet.h"

void AccelerationBehavior::on_update(BulletBehaviorContext &context)
{
    elysia::core::Vector2 new_velocity = context.bullet.projectile_velocity();
    elysia::core::Vector2 direction = new_velocity.normalized();
    new_velocity += direction * (_acceleration * static_cast<float>(context.delta_seconds));

    context.bullet.set_velocity(new_velocity);
}

void DecelerationBehavior::on_update(BulletBehaviorContext &context)
{
    elysia::core::Vector2 velocity = context.bullet.projectile_velocity();
    float speed = velocity.length();

    // enforce min speed
    if (speed <= _min_speed)
    {
        return;
    }

    float new_speed = speed - (_deceleration * static_cast<float>(context.delta_seconds));
    new_speed = std::max(new_speed, _min_speed); // enforce min speed

    elysia::core::Vector2 direction = velocity.normalized();

    context.bullet.set_velocity(new_speed * direction);
}

bool BounceBehavior::on_collision(BulletBehaviorContext &context)
{
    if (_remaining_bounces <= 0)
    {
        return false;
    }

    --_remaining_bounces;
    return true;
}

void BounceBehavior::on_fire(BulletBehaviorContext &context)
{
    context.bullet.set_restitution(_restitution);
}

// Todo this code doesnt make sense looking back
void CurveBehavior::on_update(BulletBehaviorContext &context)
{
    elysia::core::Vector2 velocity = context.bullet.projectile_velocity();

    // Bullets only curve when going a real speed
    float min_speed = 1.0f;
    if (velocity.length() < min_speed)
        return;

    elysia::core::Vector2 forward = velocity.normalized();

    // Local left/right relative to travel direction.
    elysia::core::Vector2 left = {-forward.y, forward.x};

    // Apply curve
    velocity += left * (_curve * static_cast<float>(context.delta_seconds));

    context.bullet.set_velocity(velocity);
}

void GrowthBehavior::on_update(BulletBehaviorContext &context)
{
    Bullet_Attributes *attributes = context.bullet.get_bullet_attributes();

    if (!_base_damage)
    {
        _base_damage = attributes->damage;
    }

    attributes->damage = _base_damage + _growth * context.bullet.age_seconds();
}

bool PierceBehavior::on_entity_collision(BulletBehaviorContext &context)
{
    if (_pierces <= 0)
    {
        return false;
    }

    _pierces--;
    return true;
}

bool WallStickBehavior::on_collision(BulletBehaviorContext &context)
{
    if (_stick_length <= 0.0f)
        return false;

    _last_collision_direction = context.collision_normal();

    elysia::core::Vector2 velocity = context.bullet.projectile_velocity();

    float speed = velocity.length();

    elysia::core::Vector2 wall_direction =
        -_last_collision_direction.normalized();

    _stored_velocity = wall_direction * speed;

    context.bullet.set_velocity(wall_direction * 0.0001f);

    _stuck_to_wall = true;
    _elapsed_since_activation = 0.0f;

    return true;
}

void WallStickBehavior::on_update(BulletBehaviorContext &context)
{
    if (!_stuck_to_wall)
        return;

    _stick_length -= static_cast<float>(context.delta_seconds);
    _elapsed_since_activation += static_cast<float>(context.delta_seconds);

    // Activate collision effects
    if (_elapsed_since_activation >= _activation_interval)
    {
        _elapsed_since_activation = 0.0f;

        context.bullet.set_velocity(_stored_velocity);
        _stuck_to_wall = false;

        BulletBehaviorContext collision_context{
            .bullet = context.bullet,
            .self_target = context.self_target,
            .collision = context.collision,
            .delta_seconds = context.delta_seconds};

        context.bullet.behavior_set()->replay_collision_behaviors_except(
            collision_context,
            this);
    }

    // Wall stick ends
    if (_stick_length <= 0)
    {
        context.bullet.set_velocity(_stored_velocity);
        _stuck_to_wall = false;
    }
}
