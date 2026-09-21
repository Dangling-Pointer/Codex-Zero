#include "player_character.h"

#include "engine/core/render/colors.h"
#include "engine/core/render/render_command.h"

namespace
{
constexpr float kPlayerSize = 32.0f;
constexpr float kColliderWidth = kPlayerSize * 0.65f;
constexpr float kColliderHeight = kPlayerSize * 0.38f;
constexpr float kColliderLeft = (kPlayerSize - kColliderWidth) * 0.5f;
constexpr float kColliderTop = kPlayerSize - kColliderHeight;
}

PlayerCharacter::PlayerCharacter(elysia::core::Vector2 start_position)
    : Character(start_position, {kPlayerSize, kPlayerSize},
                {kColliderLeft, kColliderTop, kColliderWidth, kColliderHeight},
                kMoveSpeed, elysia::gameplay::collision::teams::Player)
{
}

void PlayerCharacter::on_gameplay_input_frame(const elysia::gameplay::GameplayInputFrame& input)
{
    set_move_direction(input.move());
}

void PlayerCharacter::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    out_commands.push_back(elysia::core::make_world_fill_rect_command(
        render_rect(), elysia::core::colors::blue_500));
    const auto& rect = render_rect();
    const bool facing_left = facing() == Facing::Left;
    const float marker_x = facing_left ? rect.left() : rect.right();
    out_commands.push_back(elysia::core::make_world_draw_line_command(
        {marker_x, rect.center().y},
        {facing_left ? marker_x - 14.0f : marker_x + 14.0f, rect.center().y},
        elysia::core::colors::white, 3.0f));
}
