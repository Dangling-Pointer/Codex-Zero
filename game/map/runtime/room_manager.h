#pragma once

#include "room_runtime_types.h"
#include <optional>

class DungeonRoom;

namespace game
{
// Scene-owned. The map and world-space layouts must outlive the binding.
class RoomManager final
{
public:
    RoomManager() = default;
    ~RoomManager();
    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;
    RoomManager(RoomManager&&) = delete;
    RoomManager& operator=(RoomManager&&) = delete;

    void bind_map(const DungeonRoom& map, std::span<const RoomCorridorLayout> corridors = {},
                  bool corridor_only = false) noexcept;
    [[nodiscard]] bool activate() noexcept;
    void clear() noexcept;

    [[nodiscard]] std::optional<elysia::core::Rect> map_bounds() const noexcept;
    [[nodiscard]] std::optional<TileType> tile_at(elysia::core::Vector2 position) const;
    [[nodiscard]] RoomLocation region_at(elysia::core::Vector2 position) const;
    [[nodiscard]] const RoomCorridorLayout* corridor(RoomDirection direction) const noexcept;
    [[nodiscard]] bool is_walkable(const elysia::core::Rect& bounds) const;
    [[nodiscard]] bool can_place(const elysia::core::Rect& bounds,
                                 const PlacementOptions& options = {}) const;
    // preferred_center is in world coordinates. The result is a tile-aligned top left.
    [[nodiscard]] std::optional<elysia::core::Vector2> find_spawn(
        elysia::core::Vector2 size, elysia::core::Vector2 preferred_center,
        const PlacementOptions& options = {}) const;

private:
    const DungeonRoom* _map = nullptr;
    std::span<const RoomCorridorLayout> _corridors;
    bool _corridor_only = false;
};
}
