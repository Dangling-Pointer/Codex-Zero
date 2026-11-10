#include "room_placement.h"
#include "dungeon_room.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace game
{
elysia::core::Rect central_portal_bounds(const elysia::core::GameObject& map)
{
    return elysia::core::Rect::from_center(map.center(), kPortalSize);
}

bool walkable_bounds(const elysia::physics::ITileCollisionWorld& map,const elysia::core::Rect& bounds)
{
    const auto tile = map.tile_size();
    const auto origin = map.world_origin();
    if (!std::isfinite(tile.x) || !std::isfinite(tile.y) || tile.x <= 0 || tile.y <= 0
        || !std::isfinite(bounds.left()) || !std::isfinite(bounds.top())
        || !std::isfinite(bounds.right()) || !std::isfinite(bounds.bottom())
        || bounds.width() <= 0 || bounds.height() <= 0)
        return false;
    const elysia::core::Rect map_bounds{origin, {map.columns() * tile.x, map.rows() * tile.y}};
    if (!map_bounds.contains(bounds)) return false;
    const int left = static_cast<int>(std::floor((bounds.left() - origin.x) / tile.x));
    const int top = static_cast<int>(std::floor((bounds.top() - origin.y) / tile.y));
    const int right = static_cast<int>(std::ceil((bounds.right() - origin.x) / tile.x));
    const int bottom = static_cast<int>(std::ceil((bounds.bottom() - origin.y) / tile.y));
    const auto* dungeon = dynamic_cast<const DungeonRoom*>(&map);
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
        {
            if (map.cell_at({x, y}).type != elysia::physics::TileCollisionType::Empty)
                return false;
            // Void tiles are physically empty too, but cannot host room objects
            if (dungeon && dungeon->tile_map().get(x, y).type != TileType::Floor)
                return false;
        }
    return true;
}

elysia::core::Vector2 find_room_spawn(const DungeonRoom& map, elysia::core::Vector2 size,
                                     elysia::core::Vector2 preferred,
                                     std::span<const elysia::core::Rect> occupied)
{
    float nearest = std::numeric_limits<float>::max();
    elysia::core::Vector2 result;
    const auto tile = map.tile_size();
    for (int y = 0; y < map.rows(); ++y)
        for (int x = 0; x < map.columns(); ++x)
        {
            const auto at = map.position() + elysia::core::Vector2{x * tile.x, y * tile.y};
            const elysia::core::Rect bounds{at, size};
            if (!walkable_bounds(map, bounds)
                || std::ranges::any_of(occupied, [&](const auto& other) { return bounds.intersects(other); }))
                continue;
            const float distance = bounds.center().distance_squared_to(preferred);
            if (distance < nearest) { nearest = distance; result = at; }
        }
    if (nearest == std::numeric_limits<float>::max())
        throw std::runtime_error("Room has no free walkable spawn area");
    return result;
}

elysia::core::Vector2 player_spawn_near_portal(const DungeonRoom& map)
{
    const auto portal = central_portal_bounds(map);
    if (!walkable_bounds(map, portal))
        throw std::runtime_error("Room center cannot host the portal");
    return find_room_spawn(map, kPlayerSpawnSize, portal.center(), {&portal, 1});
}
}
