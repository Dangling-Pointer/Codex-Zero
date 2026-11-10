#pragma once

#include "engine/core/game_object.h"
#include "engine/physics/tile/tile_collision_world.h"

#include <span>

class DungeonRoom;

namespace game
{
inline constexpr elysia::core::Vector2 kPortalSize{62, 62};
inline constexpr elysia::core::Vector2 kPlayerSpawnSize{32, 32};

[[nodiscard]] elysia::core::Rect central_portal_bounds(const elysia::core::GameObject& map);
[[nodiscard]] bool walkable_bounds(const elysia::physics::ITileCollisionWorld& map,
                                   const elysia::core::Rect& bounds);
// Select tile-aligned positions using the complete object rectangle, not just its origin.
[[nodiscard]] elysia::core::Vector2 find_room_spawn(
    const DungeonRoom& map, elysia::core::Vector2 size, elysia::core::Vector2 preferred,
    std::span<const elysia::core::Rect> occupied);
[[nodiscard]] elysia::core::Vector2 player_spawn_near_portal(const DungeonRoom& map);
}
