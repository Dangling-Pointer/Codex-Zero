#pragma once

#include "map_config.h"
#include "engine/core/geometry/rect.h"

#include <vector>

namespace game
{
[[nodiscard]] elysia::core::Vector2 corridor_normal(RoomDirection direction) noexcept;
[[nodiscard]] RoomDirection opposite_direction(RoomDirection direction) noexcept;

// Tile coordinates in the generator, world coordinates when returned by DungeonRoom.
struct RoomCorridorLayout
{
    RoomDirection direction;
    elysia::core::Rect footprint;
    elysia::core::Rect walkable;

    [[nodiscard]] elysia::core::Vector2 seam_center() const noexcept { return walkable.center(); }
    [[nodiscard]] bool crossed_outward(elysia::core::Vector2 previous,
                                       elysia::core::Vector2 current,
                                       const elysia::core::Rect& body_at_current) const noexcept;
};

[[nodiscard]] std::vector<RoomCorridorLayout> make_room_corridors(
    const elysia::core::Rect& body, const RoomCorridorConfig& config);
}
