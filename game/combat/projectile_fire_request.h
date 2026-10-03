#pragma once

#include "shot_descriptor.h"
#include "engine/gameplay/collision/gameplay_collision_types.h"
#include "engine/physics/physics_object_handle.h"

#include <vector>

struct ProjectileFireRequest
{
    elysia::gameplay::collision::ActorId source_actor =
        elysia::gameplay::collision::InvalidActorId;
    elysia::physics::PhysicsObjectHandle source_handle{};
    std::vector<ShotDescriptor> shots;
};
