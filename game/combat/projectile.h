#pragma once

#include "../../engine/core/game_object.h"
#include "../../engine/core/interface/updatable.h"
#include "../../engine/physics/contracts/collision_listener.h"
#include "../../engine/physics/contracts/physics_participant.h"
#include "../../engine/physics/contracts/physics_step_participant.h"

#include <algorithm>
#include <span>

class Projectile : public elysia::core::GameObject,
                   public elysia::core::Updatable,
                   public elysia::physics::PhysicsParticipant,
                   public elysia::physics::PhysicsStepParticipant,
                   public elysia::physics::ICollisionListener

{
public:
    explicit Projectile(
        elysia::core::Vector2 start_position = {},
        elysia::core::Vector2 start_size = {1.0f, 1.0f},
        elysia::core::Vector2 start_velocity = {}) noexcept;

    ~Projectile() override;

    void update(double delta_seconds) override;
    void fixed_update(double fixed_delta_seconds) override;

    void on_collision_event(const elysia::physics::CollisionEvent &event) override;

    void set_velocity(elysia::core::Vector2 velocity) noexcept;

    [[nodiscard]] virtual bool on_collision(const elysia::physics::CollisionEvent &event) noexcept;

    [[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;

    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

    [[nodiscard]] elysia::core::Vector2 projectile_velocity() const noexcept;

    [[nodiscard]] double age_seconds() const noexcept;

    void destroy() noexcept;

protected:
    void reset() noexcept override;

private:
    void register_collision_listener() noexcept;
    void unregister_collision_listener() noexcept;

private:
    elysia::core::Vector2 _velocity{};
    elysia::physics::Collider _collider{};
    double _age_seconds = 0.0;
    bool _listener_registered = false;
};
