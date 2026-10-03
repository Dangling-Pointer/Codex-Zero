#pragma once

#include "engine/core/game_object.h"
#include "engine/gameplay/collision/gameplay_collision_types.h"
#include "game/combat/collision/combat_collision_categories.h"
#include "engine/physics/contracts/physics_participant.h"
#include "engine/physics/contracts/physics_step_participant.h"

#include <span>
#include <vector>
#include "game/combat/damage_receiver.h"
#include "engine/gameplay/collision/actor_collision_rig.h"
//TODO:
//// Replace direct input handling with controller-based input
//// Fix character damping

class Character : public elysia::core::GameObject,
                  public elysia::physics::PhysicsParticipant,
                  public elysia::physics::PhysicsStepParticipant,
                  public CombatReceiver
{
public:
    enum class Facing { Left, Right };   

    DamageResult receive_attack(const AttackInfo& attack) override;
    [[nodiscard]] float health() const noexcept { return _health; }
    [[nodiscard]] float max_health() const noexcept { return _max_health; }
    [[nodiscard]] bool is_dead() const noexcept { return _dead; }
    [[nodiscard]] elysia::gameplay::collision::ActorCollisionRig collision_rig() const;
    ~Character() override = default;
    void fixed_update(double fixed_delta_seconds) override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override = 0;

    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

    [[nodiscard]] elysia::gameplay::collision::TeamId team() const noexcept { return _team; }
    [[nodiscard]] elysia::gameplay::collision::ActorId actor_id() const noexcept { return _actor_id; }
    [[nodiscard]] Facing facing() const noexcept { return _facing; }
    [[nodiscard]] float move_speed() const noexcept { return _move_speed; }

protected:
    Character(elysia::core::Vector2 start_position, elysia::core::Vector2 render_size,
              elysia::core::Rect collision_rect, float move_speed,
              elysia::gameplay::collision::TeamId team, float max_health = 100.0f,
              std::vector<elysia::core::Rect> hurt_boxes = {});
    virtual void on_death() noexcept {}
    void set_move_direction(elysia::core::Vector2 direction) noexcept;

private:
    elysia::gameplay::collision::ActorId _actor_id =
        elysia::gameplay::collision::InvalidActorId;
    std::vector<elysia::physics::Collider> _colliders;
    float _max_health = 100;
    float _health = 100;
    bool _dead = false;
    elysia::core::Vector2 _move_direction{};
    elysia::core::Rect _render_rect{};
    Facing _facing = Facing::Right;
    float _move_speed;
    elysia::gameplay::collision::TeamId _team;
};
