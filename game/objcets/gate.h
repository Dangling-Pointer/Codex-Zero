#pragma once

#include "engine/core/game_object.h"
#include "engine/core/interface/updatable.h"
#include "engine/physics/contracts/physics_participant.h"

struct SDL_Texture;

class Gate final : public elysia::core::GameObject,
                   public elysia::core::Updatable,
                   public elysia::physics::PhysicsParticipant
{
public:
    static constexpr float kTileSize = 32;
    static constexpr double kAnimationDuration = 0.25;

    explicit Gate(elysia::core::Vector2 position, bool raised = false);
    void set_raised(bool raised);
    [[nodiscard]] bool is_raised() const noexcept { return _collider.enabled; }
    void update(double delta) override;
    void reset() noexcept override;

    // world_rect() stays one ground tile; the raised sprite extends one tile upward.
    [[nodiscard]] elysia::core::Rect render_rect() const noexcept override;
    void submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const override;
    [[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

private:
    [[nodiscard]] elysia::core::Rect raised_sprite_rect() const noexcept;

    double _raise_progress = 0;
    elysia::physics::Collider _collider;
    SDL_Texture* _up_texture = nullptr;
    SDL_Texture* _down_texture = nullptr;
};
