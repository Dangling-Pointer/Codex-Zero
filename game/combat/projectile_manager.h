#pragma once

#include "projectile_fire_request.h"
#include "engine/physics/physics_object_handle.h"

#include <cstddef>
#include <vector>

namespace elysia::scene { class Scene; }
namespace elysia::physics { class PhysicsWorld; }

// Owned by its scene. Update only outside Scene's object-update traversal.
class ProjectileManager final
{
public:
    ProjectileManager() = default;
    ~ProjectileManager();
    ProjectileManager(const ProjectileManager&) = delete;
    ProjectileManager& operator=(const ProjectileManager&) = delete;

    void bind_scene(elysia::scene::Scene& scene, elysia::physics::PhysicsWorld& world);
    void unbind_scene();
    void clear();
    [[nodiscard]] bool is_bound() const noexcept { return _scene && _world; }
    [[nodiscard]] bool enqueue_fire_request(ProjectileFireRequest request);
    void update(double delta_seconds);
    [[nodiscard]] std::size_t pending_count() const noexcept { return _scheduled.size(); }

private:
    struct ScheduledProjectile
    {
        const elysia::core::GameObject* source = nullptr;
        elysia::physics::PhysicsObjectHandle source_handle{};
        ShotDescriptor shot{};
        double due_time = 0;
    };

    [[nodiscard]] bool source_alive(const ScheduledProjectile& scheduled) const;
    void spawn_projectile(ScheduledProjectile scheduled);

    elysia::scene::Scene* _scene = nullptr;
    elysia::physics::PhysicsWorld* _world = nullptr;
    double _time = 0;
    std::vector<ScheduledProjectile> _scheduled;
};
