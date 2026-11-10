#pragma once
#include "engine/gameplay/collision/gameplay_collision_types.h"
#include <atomic>
namespace game
{
inline elysia::gameplay::collision::ActorId allocate_actor_id() noexcept
{
    static std::atomic<elysia::gameplay::collision::ActorId> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}
}
