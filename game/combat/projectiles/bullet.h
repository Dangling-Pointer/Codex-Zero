#pragma once

#include "projectile.h"
#include "bullet_types.h"
#include "bullet_behavior/bullet_behavior_set.h"

#include <SDL3/SDL.h>

class Bullet final : public Projectile
{
public:
    explicit Bullet(const Bullet_Attributes &bullet_attributes) noexcept;

    void submit_render_commands(std::vector<elysia::core::RenderCommand> &out_commands) const override;

    [[nodiscard]] bool on_collision(const elysia::physics::CollisionEvent &event) noexcept override;
    [[nodiscard]] bool on_entity_collision(const elysia::physics::CollisionEvent &event) noexcept override;
    void on_death() noexcept override;
    void update(double delta) override;

    Bullet_Attributes *get_bullet_attributes() { return &_bullet_attributes; }
    BulletBehaviorSet *behavior_set() { return &_behaviors; }

private:
    SDL_Texture *_texture = nullptr;

    Bullet_Attributes _bullet_attributes;
    BulletBehaviorSet _behaviors;
};
