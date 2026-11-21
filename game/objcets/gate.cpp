#include "gate.h"

#include "engine/core/render/render_command.h"
#include "engine/resources/resource_service.h"
#include "game/combat/collision/combat_collision_categories.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

Gate::Gate(elysia::core::Vector2 position, bool raised)
    : GameObject(elysia::core::DepthLayer::Item), _raise_progress(raised ? 1.0 : 0.0)
{
    set_world_rect({position, {kTileSize, kTileSize}});
    _collider.shape = elysia::physics::BoxShape::from_rect({0, 0, kTileSize, kTileSize});
    using namespace game::collision::categories;
    _collider.filter.category = World;
    _collider.filter.mask = Player | Enemy | PlayerAttack | EnemyAttack | NeutralAttack;
    _collider.response = elysia::physics::CollisionResponse::Block;
    _collider.material.friction = 0;
    _collider.material.restitution = 0;
    _collider.enabled = raised;

    _up_texture = ELYSIA_RESOURCES->find_texture("gate_up");
    _down_texture = ELYSIA_RESOURCES->find_texture("gate_down");
}

void Gate::set_raised(bool raised)
{
    if (is_raised() == raised)
        return;
    if (physics_world() && !physics_world()->set_collider_enabled(physics_collider(0), raised))
        throw std::runtime_error("Cannot change gate collision state");
    _collider.enabled = raised;
}

void Gate::reset() noexcept
{
    GameObject::reset();
    _raise_progress = 0;
    _collider.enabled = false;
    if (physics_world())
        (void)physics_world()->set_collider_enabled(physics_collider(0), false);
}

void Gate::update(double delta)
{
    const double step = std::max(0.0, delta) / kAnimationDuration;
    _raise_progress = is_raised() ? std::min(1.0, _raise_progress + step)
                                : std::max(0.0, _raise_progress - step);
}

elysia::core::Rect Gate::raised_sprite_rect() const noexcept
{
    const auto ground = GameObject::render_rect();
    // Move the sprite out of the ground, exposing its top pixels without scaling.
    const float height = static_cast<float>(std::round(kTileSize * 2 * _raise_progress));
    return {ground.x(), ground.bottom() - height, kTileSize, height};
}

elysia::core::Rect Gate::render_rect() const noexcept
{
    const auto ground = GameObject::render_rect();
    const float top = std::min(ground.top(), raised_sprite_rect().top());
    return {ground.x(), top, ground.width(), ground.bottom() - top};
}

void Gate::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    if (is_destroyed() || !is_visible())
        return;
    elysia::core::RenderCommand command;
    command.texture = _down_texture;
    command.command_rect = GameObject::render_rect();
    commands.push_back(command);

    const auto raised = raised_sprite_rect();
    if (raised.height() > 0)
    {
        command.texture = _up_texture;
        command.command_rect = raised;
        command.use_src_rect = true;
        command.src_rect = {0, 0, kTileSize, raised.height()};
        commands.push_back(command);
    }
}

std::span<const elysia::physics::Collider> Gate::collider_definitions() const
{
    return {&_collider, 1};
}
