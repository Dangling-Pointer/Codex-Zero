#pragma once

#include "game/map/corridor/room_corridor_layout.h"
#include "game/map/tile/tile_data.h"
#include <span>

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
}
