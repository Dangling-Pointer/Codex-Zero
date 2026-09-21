#pragma once

#include "engine/core/game_object.h"
#include "engine/gameplay/collision/gameplay_collision_types.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

#include <span>

class Character : public elysia::core::GameObject,
                  public elysia::physics::PhysicsParticipant,
                  public elysia::physics::PhysicsStepParticipant
{
public:
    enum class Facing { Left, Right };

    ~Character() override = default;
    void fixed_update(double fixed_delta_seconds) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override = 0;

    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;
    [[nodiscard]] elysia::gameplay::collision::TeamId team() const noexcept { return _team; }
    [[nodiscard]] Facing facing() const noexcept { return _facing; }
    [[nodiscard]] float move_speed() const noexcept { return _move_speed; }

protected:
    Character(elysia::core::Vector2 start_position, elysia::core::Vector2 size,
              elysia::core::Rect local_collision_rect, float move_speed,
              elysia::gameplay::collision::TeamId team) noexcept;
    void set_move_direction(elysia::core::Vector2 direction) noexcept;

private:
    elysia::physics::Collider _body_collider;
    elysia::core::Vector2 _move_direction{};
    Facing _facing = Facing::Right;
    float _move_speed;
    elysia::gameplay::collision::TeamId _team;
};
