#pragma once

#include "engine/tools/singleton.h"
#include "room_runtime_types.h"
#include <optional>

namespace game
{
class RoomManager;

// Queries the current room. Returned corridor pointers expire on scene changes.
class RoomService final : public elysia::tools::Singleton<RoomService>
{
    friend class elysia::tools::Singleton<RoomService>;
    friend class RoomManager;
public:
    [[nodiscard]] std::optional<elysia::core::Rect> map_bounds() const noexcept;
    [[nodiscard]] std::optional<TileType> tile_at(elysia::core::Vector2 position) const;
    [[nodiscard]] RoomLocation region_at(elysia::core::Vector2 position) const;
    [[nodiscard]] const RoomCorridorLayout* corridor(RoomDirection direction) const noexcept;
    [[nodiscard]] bool is_walkable(const elysia::core::Rect& bounds) const;
    [[nodiscard]] bool can_place(const elysia::core::Rect& bounds,
                                 const PlacementOptions& options = {}) const;
    [[nodiscard]] std::optional<elysia::core::Vector2> find_spawn(
        elysia::core::Vector2 size, elysia::core::Vector2 preferred_center,
        const PlacementOptions& options = {}) const;

private:
    RoomService() = default;
    [[nodiscard]] bool bind(const RoomManager& manager) noexcept;
    bool unbind(const RoomManager& manager) noexcept;
    const RoomManager* _manager = nullptr;
};
}
