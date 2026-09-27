#include "character.h"

#include <atomic>
#include <cmath>
#include <stdexcept>

namespace
{
elysia::gameplay::collision::ActorId allocate_actor_id() noexcept
{
    static std::atomic<elysia::gameplay::collision::ActorId> next_id{1};
    return next_id.fetch_add(1, std::memory_order_relaxed);
}
}

Character::Character(elysia::core::Vector2 start_position, elysia::core::Vector2 render_size,
                     elysia::core::Rect collision_rect, float move_speed,
                     elysia::gameplay::collision::TeamId team, float max_health,
                     std::vector<elysia::core::Rect> hurt_boxes)
    : GameObject(elysia::core::DepthLayer::Character),
      _actor_id(allocate_actor_id()), _move_speed(move_speed), _team(team)
{
    if (!std::isfinite(max_health) || max_health <= 0)
        throw std::invalid_argument("Character health must be finite and positive");
    _max_health = _health = max_health;
    set_world_rect({start_position, render_size});
    using namespace game::collision::categories;
    elysia::physics::Collider body;
    body.shape = elysia::physics::AabbShape{collision_rect};
    body.filter.category = team == elysia::gameplay::collision::teams::Enemy ? Enemy : Player;
    body.filter.mask = World | Player | Enemy;
    body.material.friction = 0;
    body.material.restitution = 0;
    _colliders.push_back(body);
    if (hurt_boxes.empty()) hurt_boxes.push_back({{0, 0}, render_size});
    for (auto rect : hurt_boxes)
    {
        elysia::physics::Collider hurt;
        hurt.shape = elysia::physics::AabbShape{rect};
        const bool enemy = team == elysia::gameplay::collision::teams::Enemy;
        hurt.filter.category = enemy ? EnemyHurt : PlayerHurt;
        hurt.filter.mask = (enemy ? PlayerAttack : EnemyAttack) | NeutralAttack;
        hurt.response = elysia::physics::CollisionResponse::Overlap;
        hurt.sensor_contributes_mass = false;
        _colliders.push_back(hurt);
    }
}

void Character::set_move_direction(elysia::core::Vector2 direction) noexcept
{
    if (_dead) return;
    _move_direction = direction.is_zero() ? elysia::core::Vector2{} : direction.normalized();
    if (_move_direction.x < 0.0f)
        _facing = Facing::Left;
    else if (_move_direction.x > 0.0f)
        _facing = Facing::Right;
}

void Character::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
    set_velocity(_dead ? elysia::core::Vector2{} : _move_direction * _move_speed);
}

elysia::physics::BodyDefinition Character::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Dynamic;
    definition.gravity_scale = 0.0f;
    definition.fixed_rotation = true;
    definition.enable_sleep = false;
    definition.linear_damping = 0.0f;
    return definition;
}

std::span<const elysia::physics::Collider> Character::collider_definitions() const
{
    return _colliders;
}

DamageResult Character::receive_attack(const AttackInfo& attack)
{
    if (_dead || is_destroyed() || !std::isfinite(attack.damage) || attack.damage < 0)
        return {};
    DamageResult result{true, std::min(_health, attack.damage), false};
    _health -= result.health_lost;
    if (_health == 0)
    {
        _dead = result.killed = true;
        _move_direction = {};
        set_velocity({});
        for (std::size_t i = 0; i < _colliders.size(); ++i)
        {
            _colliders[i].enabled = false;
            update_physics_collider(i, _colliders[i]);
        }
        on_death();
    }
    return result;
}
elysia::gameplay::collision::ActorCollisionRig Character::collision_rig() const
{
    elysia::gameplay::collision::ActorCollisionRig rig;
    rig.owner = _actor_id;
    rig.team = _team;
    rig.body = physics_collider(0);
    for (std::size_t i = 1; i < _colliders.size(); ++i)
        rig.hurt_boxes.push_back(physics_collider(i));
    return rig;
}
