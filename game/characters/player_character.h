#pragma once

#include "../../engine/core/game_object.h"
#include "../../engine/core/interface/updatable.h"
#include "../../engine/gameplay/input/contracts/gameplay_input_frame_receiver.h"
#include "../../engine/physics/contracts/physics_participant.h"
#include "../../engine/physics/contracts/physics_step_participant.h"

#include <span>

class PlayerCharacter final
    : public elysia::core::GameObject,
      public elysia::core::Updatable,
      public elysia::gameplay::GameplayInputFrameReceiver,
      public elysia::physics::PhysicsParticipant,
      public elysia::physics::PhysicsStepParticipant
{
public:
    static constexpr float kMoveSpeed = 200.0f;
    explicit PlayerCharacter(elysia::core::Vector2 start_position);
    void update(double delta_seconds) override;
    void on_gameplay_input_frame(const elysia::gameplay::GameplayInputFrame &input) override;
    void fixed_update(double fixed_delta_seconds) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand> &out_commands) const override;
    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

private:
    elysia::physics::Collider _body_collider;
    elysia::core::Vector2 _movement{};
    bool _facing_left = false;
};
