#pragma once

#include "../../engine/core/geometry/vector2.h"
#include <cstdint>
#include <optional>

// Base config dont delete
struct MapConfig
{
    // Can't go lower than 6 for some reason
    elysia::core::Vector2 baseRoomSize = {10, 10};

    int numSubRooms = 7;
    elysia::core::Vector2 minSubRoomSize = {3, 3};

    // No lower than 2
    int minGap = 3;
    std::optional<std::uint64_t> seed;
};
