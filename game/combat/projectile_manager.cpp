#include "projectile_manager.h"
#include "projectile_service.h"
#include "projectiles/bullet.h"
#include "../characters/character.h"
#include "engine/scene/scene.h"
#include "engine/tools/logger.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>

namespace
{
bool finite(elysia::core::Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool valid(const ShotDescriptor& shot)
{
    const auto& a = shot.bullet_attributes;
    return std::isfinite(shot.spawn_delay_sec) && finite(shot.spawn_offset)
        && finite(shot.shot_direction) && finite(a.start_position)
        && finite(a.starting_velocity) && finite(a.bullet_size)
        && a.bullet_size.x > 0 && a.bullet_size.y > 0
        && std::isfinite(a.bullet_speed) && a.bullet_speed >= 0
        && std::isfinite(a.max_age) && a.max_age > 0
        && std::isfinite(a.damage) && a.damage >= 0
        && std::isfinite(a.damage_cooldown_sec) && a.damage_cooldown_sec >= 0
        && std::ranges::all_of(a.behavior_appenders, [](const auto& fn) { return bool(fn); });
}
}

ProjectileManager::~ProjectileManager()
{
    // Scene owns projectiles and destroys them while its physics world is alive.
    (void)ProjectileService::instance()->unbind_manager(*this);
}

void ProjectileManager::bind_scene(elysia::scene::Scene& scene,
                                   elysia::physics::PhysicsWorld& world)
{
    unbind_scene();
    _scene = &scene;
    _world = &world;
}

void ProjectileManager::unbind_scene()
{
    (void)ProjectileService::instance()->unbind_manager(*this);
    clear();
    _scene = nullptr;
    _world = nullptr;
}

void ProjectileManager::clear()
{
    _scheduled.clear();
    _time = 0;
    if (!_scene)
        return;
    std::vector<Projectile*> projectiles;
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(*_scene)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
            if (auto* projectile = dynamic_cast<Projectile*>(&object))
                projectiles.push_back(projectile);
            return true;
        });
    for (auto* projectile : projectiles)
        projectile->destroy();
}

bool ProjectileManager::enqueue_fire_request(ProjectileFireRequest request)
{
    if (!is_bound()
        || request.source_actor == elysia::gameplay::collision::InvalidActorId
        || !request.source_handle.is_valid() || request.shots.empty()
        || !std::ranges::all_of(request.shots, valid))
        return false;

    const ScheduledProjectile source_check{
        request.source_actor, request.source_handle, {}, 0.0};
    if (!source_alive(source_check))
        return false;

    _scheduled.reserve(_scheduled.size() + request.shots.size());
    for (auto& shot : request.shots)
    {
        const double due = _time + std::max(0.0, double(shot.spawn_delay_sec));
        _scheduled.push_back({request.source_actor, request.source_handle, std::move(shot), due});
    }
    std::stable_sort(_scheduled.begin(), _scheduled.end(), [](const auto& a, const auto& b) {
        return a.due_time < b.due_time;
    });
    return true;
}

bool ProjectileManager::source_alive(const ScheduledProjectile& scheduled) const
{
    if (!_world->contains_object(scheduled.source_handle))
        return false;

    bool alive = false;
    static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(*_scene)
        .visit_game_objects(elysia::core::DepthLayerMask::all(), [&](auto& object) {
            const auto handle = _world->object_handle(object);
            if (handle && *handle == scheduled.source_handle)
            {
                const auto* character = dynamic_cast<const Character*>(&object);
                alive = character && character->actor_id() == scheduled.source_actor
                    && !character->is_destroyed();
                return false;
            }
            return true;
        });
    return alive;
}

void ProjectileManager::update(double delta_seconds)
{
    if (!is_bound() || !std::isfinite(delta_seconds) || delta_seconds < 0
        || !std::isfinite(_time + delta_seconds))
        return;
    _time += delta_seconds;
    std::erase_if(_scheduled, [&](const auto& shot) { return !source_alive(shot); });

    // Detach ready work before invoking constructors/behaviors, which may queue more shots.
    const auto end = std::find_if(_scheduled.begin(), _scheduled.end(),
        [&](const auto& shot) { return shot.due_time > _time; });
    std::vector<ScheduledProjectile> ready;
    ready.reserve(std::distance(_scheduled.begin(), end));
    std::move(_scheduled.begin(), end, std::back_inserter(ready));
    _scheduled.erase(_scheduled.begin(), end);
    for (auto& shot : ready)
        if (source_alive(shot))
            spawn_projectile(std::move(shot));
}

void ProjectileManager::spawn_projectile(ScheduledProjectile scheduled)
{
    auto attributes = std::move(scheduled.shot.bullet_attributes);
    const auto state = _world->body_state(scheduled.source_handle);
    if (!state)
        return;
    attributes.start_position = state->position + scheduled.shot.spawn_offset;
    if (!finite(attributes.start_position))
        return;
    auto* projectile = _scene->create_and_add_object<Bullet>(attributes);
    if (!projectile || !_world->contains_object(projectile->physics_handle()))
    {
        if (projectile)
            projectile->destroy();
        ELYSIA_LOG_ERROR("ProjectileManager", "Projectile physics registration failed.");
    }
}
