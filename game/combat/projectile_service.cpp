#include "projectile_service.h"
#include "projectile_manager.h"

#include <utility>

bool ProjectileService::bind_manager(ProjectileManager& manager) noexcept
{
    if (!manager.is_bound() || (_manager && _manager != &manager))
        return false;
    _manager = &manager;
    return true;
}

bool ProjectileService::unbind_manager(const ProjectileManager& manager) noexcept
{
    if (_manager != &manager)
        return false;
    _manager = nullptr;
    return true;
}

bool ProjectileService::request_fire(ProjectileFireRequest request)
{
    return _manager && _manager->enqueue_fire_request(std::move(request));
}
