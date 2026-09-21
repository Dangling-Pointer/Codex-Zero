#pragma once

#include "projectile_fire_request.h"
#include "engine/tools/singleton.h"

class ProjectileManager;

class ProjectileService final : public elysia::tools::Singleton<ProjectileService>
{
    friend class elysia::tools::Singleton<ProjectileService>;
public:
    [[nodiscard]] bool bind_manager(ProjectileManager& manager) noexcept;
    [[nodiscard]] bool unbind_manager(const ProjectileManager& manager) noexcept;
    [[nodiscard]] bool request_fire(ProjectileFireRequest request);
private:
    ProjectileService() = default;
    ProjectileManager* _manager = nullptr;
};
