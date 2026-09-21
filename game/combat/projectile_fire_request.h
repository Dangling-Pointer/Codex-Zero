#pragma once

#include "shot_descriptor.h"
#include "engine/core/game_object.h"

#include <vector>

struct ProjectileFireRequest
{
    // Borrowed only for submission; the manager validates scene membership.
    const elysia::core::GameObject* source = nullptr;
    std::vector<ShotDescriptor> shots;
};
