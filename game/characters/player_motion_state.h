#pragma once

#include "engine/core/geometry/vector2.h"

// Transient scene handoff; persistent character progression stays in CharacterContext.
struct PlayerMotionState
{
    elysia::core::Vector2 direction;
    elysia::core::Vector2 velocity;
    elysia::core::Vector2 render_offset;
    bool facing_left = false;
};
