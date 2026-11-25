#pragma once

#include "room_corridor_layout.h"
#include "tile_data.h"

#include <optional>
#include <span>

class DungeonRoom;

namespace game
{
enum class RoomRegion { None, Room, Corridor };

struct RoomLocation
{
    RoomRegion region = RoomRegion::None;
    RoomDirection direction = RoomDirection::None;
};

enum class SpawnRegion { Any, Room, Corridor };

struct PlacementOptions
{
    SpawnRegion region = SpawnRegion::Any;
    std::span<const elysia::core::Rect> occupied;
};

// Borrows the map and world-space layouts. They must outlive this query.
// Walkability describes terrain; live objects are only checked through occupied.
class RoomQuery
{
public:
    explicit RoomQuery(const DungeonRoom& map,
                       std::span<const RoomCorridorLayout> corridors = {},
                       bool corridor_only = false) noexcept;

    [[nodiscard]] elysia::core::Rect map_bounds() const noexcept;
    [[nodiscard]] std::optional<TileType> tile_at(elysia::core::Vector2 position) const;
    [[nodiscard]] RoomLocation region_at(elysia::core::Vector2 position) const;
    [[nodiscard]] const RoomCorridorLayout* corridor(RoomDirection direction) const noexcept;
    [[nodiscard]] bool is_walkable(const elysia::core::Rect& bounds) const;
    [[nodiscard]] bool can_place(const elysia::core::Rect& bounds,
                                 const PlacementOptions& options = {}) const;
    // preferred_center is a world-space center; the result is a tile-aligned top left.
    [[nodiscard]] std::optional<elysia::core::Vector2> find_spawn(
        elysia::core::Vector2 size, elysia::core::Vector2 preferred_center,
        const PlacementOptions& options = {}) const;

private:
    const DungeonRoom& _map;
    std::span<const RoomCorridorLayout> _corridors;
    bool _corridor_only;
};
}
