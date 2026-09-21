#include "character.h"

Character::Character(elysia::core::Vector2 start_position, elysia::core::Vector2 render_size,
                     elysia::core::Rect collision_rect, float move_speed,
                     elysia::gameplay::collision::TeamId team) noexcept
    : GameObject(elysia::core::DepthLayer::Character), _move_speed(move_speed), _team(team)
{
    set_world_rect({start_position, render_size});
    _body_collider.shape = elysia::physics::AabbShape{collision_rect};
    _body_collider.response = elysia::physics::CollisionResponse::Block;
    _body_collider.material.friction = 0.0f;
    _body_collider.material.restitution = 0.0f;
}

void Character::set_move_direction(elysia::core::Vector2 direction) noexcept
{
    _move_direction = direction.is_zero() ? elysia::core::Vector2{} : direction.normalized();
    if (_move_direction.x < 0.0f)
        _facing = Facing::Left;
    else if (_move_direction.x > 0.0f)
        _facing = Facing::Right;
}

void Character::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
    set_velocity(_move_direction * _move_speed);
}

elysia::physics::BodyDefinition Character::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Dynamic;
    definition.gravity_scale = 0.0f;
    definition.fixed_rotation = true;
    definition.enable_sleep = false;
    definition.linear_damping = 0.0f;
    return definition;
}

std::span<const elysia::physics::Collider> Character::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_body_collider, 1);
}
