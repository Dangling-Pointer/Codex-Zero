#include "player_character.h"

#include "../../engine/core/render/colors.h"
#include "../../engine/core/render/render_command.h"

namespace game::characters
{
namespace
{
constexpr float kPlayerSize = 64.0f;
constexpr float kColliderWidth = kPlayerSize * 0.65f;
constexpr float kColliderHeight = kPlayerSize * 0.38f;
constexpr float kColliderLeft = (kPlayerSize - kColliderWidth) * 0.5f;
constexpr float kColliderTop = kPlayerSize - kColliderHeight;
}

PlayerCharacter::PlayerCharacter(const elysia::core::Vector2 start_position)
    : GameObject(elysia::core::DepthLayer::Character)
{
    set_world_rect({start_position.x, start_position.y, kPlayerSize, kPlayerSize});
    _body_collider.shape = elysia::physics::AabbShape{
        {kColliderLeft, kColliderTop, kColliderWidth, kColliderHeight}};
    _body_collider.response = elysia::physics::CollisionResponse::Block;
    _body_collider.material.friction = 0.0f;
}

void PlayerCharacter::update(double delta_seconds) { (void)delta_seconds; }

void PlayerCharacter::on_gameplay_input_frame(const elysia::gameplay::GameplayInputFrame& input)
{
    _movement = input.move();
    if (!_movement.is_zero())
    {
        _movement = _movement.normalized();
        if (_movement.x < 0.0f)
            _facing_left = true;
        else if (_movement.x > 0.0f)
            _facing_left = false;
    }
}

void PlayerCharacter::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
    set_velocity(_movement * kMoveSpeed);
}

void PlayerCharacter::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    out_commands.push_back(elysia::core::make_world_fill_rect_command(
        render_rect(), elysia::core::colors::blue_500));
    const elysia::core::Rect& rect = render_rect();
    const float marker_x = _facing_left ? rect.left() : rect.right();
    out_commands.push_back(elysia::core::make_world_draw_line_command(
        {marker_x, rect.center().y},
        {_facing_left ? marker_x - 14.0f : marker_x + 14.0f, rect.center().y},
        elysia::core::colors::white, 3.0f));
}

elysia::physics::BodyDefinition PlayerCharacter::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Dynamic;
    definition.gravity_scale = 0.0f;
    definition.fixed_rotation = true;
    definition.enable_sleep = false;
    definition.linear_damping = 0.0f;
    return definition;
}

std::span<const elysia::physics::Collider> PlayerCharacter::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_body_collider, 1);
}
} // namespace game::characters
