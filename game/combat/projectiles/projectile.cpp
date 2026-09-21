#include "projectile.h"

#include "game/combat/collision/combat_collision_categories.h"

Projectile::Projectile(
    elysia::core::Vector2 start_position, elysia::core::Vector2 start_size,
    elysia::core::Vector2 start_velocity,
    game::collision::categories::CollisionBits collision_category) noexcept
    : elysia::core::GameObject(elysia::core::DepthLayer::Item), _velocity(start_velocity)
{
    start_size.x = std::max(1.0f, start_size.x);
    start_size.y = std::max(1.0f, start_size.y);
    set_world_rect(elysia::core::Rect::from_center(start_position, start_size));
    _collider.shape = elysia::physics::AabbShape{{0.0f, 0.0f, start_size.x, start_size.y}};
    _collider.filter.category = collision_category;
    _collider.filter.mask = game::collision::categories::World;
    if (collision_category == game::collision::categories::PlayerAttack)
        _collider.filter.mask |= game::collision::categories::Enemy;
    else if (collision_category == game::collision::categories::EnemyAttack)
        _collider.filter.mask |= game::collision::categories::Player;
    else if (collision_category == game::collision::categories::NeutralAttack)
        _collider.filter.mask |= game::collision::categories::Player
            | game::collision::categories::Enemy;
    _collider.response = elysia::physics::CollisionResponse::Block;
}

Projectile::~Projectile()
{
    unregister_collision_listener();
}

void Projectile::update(double delta_seconds)
{
    _age_seconds += std::max(0.0, delta_seconds);
    register_collision_listener();
}

void Projectile::fixed_update(double fixed_delta_seconds)
{
    (void)fixed_delta_seconds;
}

void Projectile::on_collision_event(
    const elysia::physics::CollisionEvent &event)
{
    if (!_listener_registered || event.phase != elysia::physics::CollisionEventPhase::Begin)
        return;

    const auto target = elysia::physics::CollisionTarget::from_collider(
        physics_collider(0));
    if (event.contact.pair.first != target && event.contact.pair.second != target)
        return;

    const auto &other = event.contact.pair.first == target
                            ? event.contact.pair.second
                            : event.contact.pair.first;
    const bool handled = other.kind == elysia::physics::CollisionTargetKind::Collider
                             ? on_entity_collision(event)
                             : on_collision(event);
    if (handled)
        destroy();
}

bool Projectile::on_collision(
    const elysia::physics::CollisionEvent &event) noexcept
{
    (void)event;
    return true;
}

bool Projectile::on_entity_collision(
    const elysia::physics::CollisionEvent &event) noexcept
{
    (void)event;
    return true;
}

void Projectile::on_death() noexcept
{
}

elysia::physics::BodyDefinition Projectile::body_definition() const
{
    elysia::physics::BodyDefinition definition;
    definition.type = elysia::physics::BodyType::Dynamic;
    definition.velocity = _velocity;
    definition.gravity_scale = 0.0f;
    definition.linear_damping = 0.0f;
    definition.fixed_rotation = true;
    definition.enable_sleep = false;
    definition.bullet = true;
    return definition;
}

std::span<const elysia::physics::Collider> Projectile::collider_definitions() const
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}
void Projectile::set_velocity(elysia::core::Vector2 velocity) noexcept
{
    _velocity = velocity;
    elysia::physics::PhysicsParticipant::set_velocity(velocity);
}

void Projectile::set_restitution(float restitution) noexcept
{
    _collider.material.restitution = std::clamp(restitution, 0.0f, 1.0f);
    if (physics_world())
        update_physics_collider(0, _collider);
}

void Projectile::destroy() noexcept
{
    if (is_destroyed())
        return;

    unregister_collision_listener();
    if (!_death_notified)
    {
        _death_notified = true;
        on_death();
    }
    elysia::core::SceneObject::destroy();
}

[[nodiscard]] elysia::core::Vector2 Projectile::projectile_velocity() const noexcept
{
    return physics_world() ? PhysicsParticipant::velocity() : _velocity;
}

[[nodiscard]] double Projectile::age_seconds() const noexcept
{
    return _age_seconds;
}

// protected
void Projectile::reset() noexcept
{
    elysia::core::GameObject::reset();
    _age_seconds = 0.0;
    _death_notified = false;
}

// private
void Projectile::register_collision_listener() noexcept
{
    if (_listener_registered || !physics_world())
        return;

    _listener_registered = physics_world()->add_listener(*this);
}

void Projectile::unregister_collision_listener() noexcept
{
    if (!_listener_registered || !physics_world())
        return;

    (void)physics_world()->remove_listener(*this);
    _listener_registered = false;
}