#pragma once

#include "projectile.h"
#include "bullet_types.h"
#include "bullet_behavior/bullet_behavior_set.h"

#include <SDL3/SDL.h>
#include "game/combat/damage_receiver.h"

class Bullet final : public Projectile
{
public:
    explicit Bullet(const Bullet_Attributes &bullet_attributes) noexcept;

    void initialize_fire();

    // Identifies this specific attack for hit tracking and deduplication
    [[nodiscard]] elysia::gameplay::collision::AttackInstanceId attack_instance() const { return _attack_instance; }

    // Returns the actor(character) that initiated this attack
    // Identifies the actor responsible for this attack
    [[nodiscard]] elysia::gameplay::collision::ActorId instigator() const { return _instigator; }

    // Assigns the actor responsible for this attack
    void set_instigator(elysia::gameplay::collision::ActorId id) { _instigator = id; }

    void submit_render_commands(std::vector<elysia::core::RenderCommand> &out_commands) const override;

    [[nodiscard]] bool on_collision(const elysia::physics::CollisionEvent &event) noexcept override;
    [[nodiscard]] bool on_entity_collision(const elysia::physics::CollisionEvent &event) noexcept override;
    void on_death() noexcept override;
    void update(double delta) override;

    Bullet_Attributes *get_bullet_attributes() { return &_bullet_attributes; }
    BulletBehaviorSet *behavior_set() { return &_behaviors; }

private:
    bool _fired = false;
    elysia::gameplay::collision::AttackInstanceId _attack_instance;
    elysia::gameplay::collision::ActorId _instigator = 0;
    SDL_Texture *_texture = nullptr;

    Bullet_Attributes _bullet_attributes;
    BulletBehaviorSet _behaviors;
};
