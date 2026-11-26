#pragma once

#include "game/map/dungeon_room.h"
#include "game/map/runtime/room_manager.h"

namespace test
{
inline bool walkable(const DungeonRoom& map, const elysia::core::Rect& bounds)
{
    game::RoomManager manager;
    manager.bind_map(map);
    return manager.is_walkable(bounds);
}
}
