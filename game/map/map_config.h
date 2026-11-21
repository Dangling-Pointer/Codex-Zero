#pragma once

#include "../../engine/core/geometry/vector2.h"
#include "game/gameflow/dungeon_graph_types.h"
#include <cstdint>
#include <optional>

struct RoomCorridorConfig
{
    RoomDirection directions = RoomDirection::None;
    int width = 5;              // Walkable tiles, excluding the two side walls.
    int vertical_length = 22;   // Up/down, starting at the original room wall.
    int horizontal_length = 28; // Left/right, starting at the original room wall.

    [[nodiscard]] constexpr elysia::core::Vector2 padding() const noexcept
    {
        if (directions == RoomDirection::None)
            return {};
        return {static_cast<float>(horizontal_length), static_cast<float>(vertical_length)};
    }
};

inline constexpr RoomCorridorConfig kStandardRoomCorridors{};

// Base config dont delete
struct MapConfig
{
    // Can't go lower than 6 for some reason
    elysia::core::Vector2 baseRoomSize = {14, 14};

    int numSubRooms = 4;
    elysia::core::Vector2 minSubRoomSize = {8, 8};

    // No lower than 2
    int minGap = 4;
    std::optional<std::uint64_t> seed;
    RoomCorridorConfig corridors = kStandardRoomCorridors;
};
