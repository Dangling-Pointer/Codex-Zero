#pragma once

#include "room_corridor_layout.h"

// A corridor belongs to an edge, not to a cell in Floor::rooms.
struct CorridorTraversal
{
    RoomID from;
    RoomID to;
    RoomDirection direction; // Outward direction from 'from'.
};

namespace game
{
[[nodiscard]] int connection_gap(const Floor& floor, RoomID from, RoomID to) noexcept;
[[nodiscard]] int extra_corridor_tiles(int empty_cells) noexcept;
[[nodiscard]] float room_corridor_half_tiles(RoomDirection direction) noexcept;
[[nodiscard]] float room_edge_fraction(int empty_cells, RoomDirection direction) noexcept;

struct CorridorLayout
{
    elysia::core::Vector2 grid_size;
    elysia::core::Vector2 source_seam; // Tile coordinates.
    elysia::core::Vector2 target_seam;
    RoomDirection direction;
    int buffer_tiles;

    [[nodiscard]] std::vector<RoomCorridorLayout> world_exits(
        elysia::core::Vector2 origin, elysia::core::Vector2 tile) const;
    [[nodiscard]] float progress(elysia::core::Vector2 world_center,
                                 elysia::core::Vector2 origin, elysia::core::Vector2 tile) const noexcept;
};

[[nodiscard]] CorridorLayout make_corridor_layout(const Floor& floor, const CorridorTraversal& traversal);
}
