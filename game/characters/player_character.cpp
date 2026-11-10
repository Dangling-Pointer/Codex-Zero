#include "player_character.h"
#include "character_manager.h"
#include "game/input/game_actions.h"
#include <utility>
#include <cmath>
#include <stdexcept>

#include "engine/core/render/colors.h"
#include "engine/core/render/render_command.h"

namespace
{
constexpr float kPlayerRenderSize = 32.0f;
constexpr float kColliderWidth = kPlayerRenderSize * 0.65f;
constexpr float kColliderHeight = kPlayerRenderSize * 0.38f;
constexpr float kColliderLeft = (kPlayerRenderSize - kColliderWidth) * 0.5f;
constexpr float kColliderTop = kPlayerRenderSize - kColliderHeight;
}

PlayerCharacter::PlayerCharacter(elysia::core::Vector2 start_position, float max_health)
    : Character(start_position, {kPlayerRenderSize, kPlayerRenderSize},
                {kColliderLeft, kColliderTop, kColliderWidth, kColliderHeight},
                kMoveSpeed, elysia::gameplay::collision::teams::Player, max_health)
{
}

void PlayerCharacter::on_control_command(const elysia::gameplay::ControlCommand& command, double)
{
    if (is_dead() || is_destroyed())
    {
        _fire_requests.clear();
        return;
    }
    set_move_direction(command.state.axis2d(game::input::actions::Move));
    for (const auto& event : command.events)
        if (event.action == game::input::actions::Fire
            && event.phase == elysia::input::ActionInputPhase::Started)
            _fire_requests.push_back(command.pointer_position);
}

void PlayerCharacter::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    out_commands.push_back(elysia::core::make_world_fill_rect_command(
        render_rect(), is_dead() ? elysia::core::colors::gray_500 : elysia::core::colors::blue_500));

    const auto& rect = render_rect();
    const bool facing_left = facing() == Facing::Left;
    const float marker_x = facing_left ? rect.left() : rect.right();
    out_commands.push_back(elysia::core::make_world_draw_line_command(
        {marker_x, rect.center().y},
        {facing_left ? marker_x - 14.0f : marker_x + 14.0f, rect.center().y},
        elysia::core::colors::white, 3.0f));
}

void PlayerCharacter::on_control_cancelled(elysia::gameplay::InputCancelReason)
{
    _fire_requests.clear();
    set_move_direction({});
    set_velocity({});
}
void PlayerCharacter::validate_context(const CharacterContext& context)
{
    if (!CharacterManager::instance()->find_definition(context.character_id)
        || !std::isfinite(context.health) || !std::isfinite(context.max_health)
        || !std::isfinite(context.move_speed) || context.max_health <= 0
        || context.health < 0 || context.health > context.max_health || context.move_speed < 0)
        throw std::invalid_argument("Invalid player character context");
}
PlayerCharacter::PlayerCharacter(elysia::core::Vector2 start_position, const CharacterContext& context)
    : PlayerCharacter(start_position, (validate_context(context), context.max_health))
{
    _character_id = context.character_id;
    restore_state(context.health, context.move_speed);
}
CharacterContext PlayerCharacter::capture_context() const
{
    return {_character_id, health(), max_health(), move_speed()};
}

std::vector<PlayerCharacter::FireRequest> PlayerCharacter::take_fire_requests()
{
    return std::exchange(_fire_requests, {});
}

void PlayerCharacter::on_death() noexcept
{
    _fire_requests.clear();
}
