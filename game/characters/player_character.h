#pragma once

#include "character.h"
#include "engine/gameplay/control/control_command.h"
#include "character_context.h"
#include "player_motion_state.h"
#include <optional>
#include <vector>

class PlayerCharacter final : public Character,
                              public elysia::gameplay::ControlCommandReceiver
{
public:
    static constexpr float kMoveSpeed = 200.0f;

    explicit PlayerCharacter(elysia::core::Vector2 start_position, float max_health = 100.0f);
    PlayerCharacter(elysia::core::Vector2 start_position, const CharacterContext& context);
    static void validate_context(const CharacterContext& context);
    [[nodiscard]] CharacterContext capture_context() const;
    [[nodiscard]] PlayerMotionState capture_motion() const;
    void restore_motion(const PlayerMotionState& motion);
    [[nodiscard]] elysia::core::Rect blocking_bounds() const;
    [[nodiscard]] elysia::core::Rect render_rect() const noexcept override;
    void fixed_update(double delta) override;
    using FireRequest = std::optional<elysia::core::Vector2>; // Logical screen position, when available.
    [[nodiscard]] std::vector<FireRequest> take_fire_requests();
    void on_control_command(const elysia::gameplay::ControlCommand& command, double) override;
    void on_control_cancelled(elysia::gameplay::InputCancelReason) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;

protected:
    void on_death() noexcept override;

private:
    std::vector<FireRequest> _fire_requests;
    CharacterId _character_id = "default_player";
    std::optional<elysia::core::Vector2> _entry_render_offset;

};
