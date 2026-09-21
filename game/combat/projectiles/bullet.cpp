#include "bullet.h"

#include "engine/core/render/render_command.h"
#include "engine/resources/resource_service.h"
#include "bullet_behavior/bullet_behavior_context.h"

#include <cmath>

constexpr float kRadiansToDegrees = 57.29577951308232f;

Bullet::Bullet(const Bullet_Attributes &bullet_attributes) noexcept
    : Projectile(
          bullet_attributes.start_position,
          bullet_attributes.bullet_size,
          bullet_attributes.starting_velocity,
          bullet_attributes.collision_category)
{
    _texture = ELYSIA_RESOURCES->find_texture("bullet");

    _bullet_attributes = bullet_attributes;

    for (const auto &append_behavior : _bullet_attributes.behavior_appenders)
    {
        append_behavior(_behaviors);
    }

        BulletBehaviorContext context{.bullet = *this};
    _behaviors.on_fire(context);
}

// TODO collision box doesnt align with texture rotation
void Bullet::submit_render_commands(std::vector<elysia::core::RenderCommand> &out_commands) const
{
    if (!_texture)
        return;

    elysia::core::RenderCommand command;
    command.texture = _texture;
    command.command_rect = world_rect();
    const elysia::core::Vector2 shot_velocity = projectile_velocity();
    if (!shot_velocity.is_zero())
        command.rotation_degrees = std::atan2(shot_velocity.y, shot_velocity.x) * kRadiansToDegrees;
    out_commands.push_back(std::move(command));
}

bool Bullet::on_collision(const elysia::physics::CollisionEvent &event) noexcept
{
    BulletBehaviorContext context{
        .bullet = *this,
        .self_target = elysia::physics::CollisionTarget::from_collider(physics_collider(0)),
        .other_target = event.contact.pair.first ==
                                elysia::physics::CollisionTarget::from_collider(physics_collider(0))
                            ? event.contact.pair.second
                            : event.contact.pair.first,
        .collision = &event};

    return !_behaviors.collision_handled(context);
}

bool Bullet::on_entity_collision(const elysia::physics::CollisionEvent &event) noexcept
{
    BulletBehaviorContext context{
        .bullet = *this,
        .self_target = elysia::physics::CollisionTarget::from_collider(physics_collider(0)),
        .other_target = event.contact.pair.first ==
                                elysia::physics::CollisionTarget::from_collider(physics_collider(0))
                            ? event.contact.pair.second
                            : event.contact.pair.first,
        .collision = &event};

    return !_behaviors.entity_collision_handled(context);
}

void Bullet::on_death() noexcept
{
    BulletBehaviorContext context{
        .bullet = *this,
        .self_target = elysia::physics::CollisionTarget::from_collider(physics_collider(0))};
    _behaviors.on_death(context);
}

void Bullet::update(double delta)
{
    Projectile::update(delta);

    if (age_seconds() >= _bullet_attributes.max_age)
    {
        destroy();
        return;
    }

    // call on update behaviors
    BulletBehaviorContext context{.bullet = *this, .delta_seconds = delta};
    _behaviors.on_update(context);
}
