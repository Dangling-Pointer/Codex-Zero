#include "room_placement.h"
#include "dungeon_room.h"
#include "room_query.h"

#include <stdexcept>

namespace game
{
elysia::core::Rect central_portal_bounds(const elysia::core::GameObject& map)
{
    return elysia::core::Rect::from_center(map.center(), kPortalSize);
}

elysia::core::Vector2 find_room_spawn(const DungeonRoom& map, elysia::core::Vector2 size,
                                     elysia::core::Vector2 preferred,
                                     std::span<const elysia::core::Rect> occupied)
{
    const auto result = RoomQuery{map}.find_spawn(size, preferred, {.occupied = occupied});
    if (!result)
        throw std::runtime_error("Room has no free walkable spawn area");
    return *result;
}

elysia::core::Vector2 player_spawn_near_portal(const DungeonRoom& map)
{
    const auto portal = central_portal_bounds(map);

    if (!walkable_bounds(map, portal))
        throw std::runtime_error("Room center cannot host the portal");

    return find_room_spawn(map, kPlayerSpawnSize, portal.center(), {&portal, 1});
}
}
