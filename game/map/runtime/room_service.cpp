#include "room_service.h"
#include "room_manager.h"

namespace game
{
bool RoomService::bind(const RoomManager& manager) noexcept
{
    if (_manager && _manager != &manager)
        return false;
    _manager = &manager;
    return true;
}

bool RoomService::unbind(const RoomManager& manager) noexcept
{
    if (_manager != &manager)
        return false;
    _manager = nullptr;
    return true;
}

std::optional<elysia::core::Rect> RoomService::map_bounds() const noexcept
{
    return _manager ? _manager->map_bounds() : std::nullopt;
}

std::optional<TileType> RoomService::tile_at(elysia::core::Vector2 position) const
{
    return _manager ? _manager->tile_at(position) : std::nullopt;
}

RoomLocation RoomService::region_at(elysia::core::Vector2 position) const
{
    return _manager ? _manager->region_at(position) : RoomLocation{};
}

const RoomCorridorLayout* RoomService::corridor(RoomDirection direction) const noexcept
{
    return _manager ? _manager->corridor(direction) : nullptr;
}

bool RoomService::is_walkable(const elysia::core::Rect& bounds) const
{
    return _manager && _manager->is_walkable(bounds);
}

bool RoomService::can_place(const elysia::core::Rect& bounds, const PlacementOptions& options) const
{
    return _manager && _manager->can_place(bounds, options);
}

std::optional<elysia::core::Vector2> RoomService::find_spawn(
    elysia::core::Vector2 size, elysia::core::Vector2 preferred_center,
    const PlacementOptions& options) const
{
    return _manager ? _manager->find_spawn(size, preferred_center, options) : std::nullopt;
}
}
