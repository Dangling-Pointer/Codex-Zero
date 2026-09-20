#include "character.h"

#include "../../engine/core/render/colors.h"
#include "../../engine/core/render/render_command.h"

Character::Character(elysia::core::Vector2 start_position) noexcept
    : elysia::core::GameObject(elysia::core::DepthLayer::Character)
{
    // tmp
    set_world_rect({start_position.x, start_position.y, 30, 30});

    _body_collider.material = elysia::physics::PhysicsMaterial{.friction = 0.5, .restitution = 1};
    // friction not working because there is no friction on floor tile

    _body_collider.shape = elysia::physics::AabbShape{{32, 32, 32, 32}};
    _body_collider.response = elysia::physics::CollisionResponse::Block;
}

void Character::update(double delta)
{
    (void)delta;
}

void Character::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
}

void Character::submit_render_commands(std::vector<elysia::core::RenderCommand> &out_commands) const
{
    out_commands.push_back(elysia::core::make_world_fill_rect_command(render_rect(), elysia::core::colors::red_500));
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
