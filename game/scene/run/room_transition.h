#pragma once

#include "room_scene_payload.h"
#include "game/map/room_corridor_layout.h"

#include <span>

class PlayerCharacter;
namespace elysia::camera { class Camera; }
namespace elysia::physics { class ITileCollisionWorld; }
namespace elysia::scene { class SceneCameraRuntime; }

namespace game
{
// Stateless helpers. The scene owns the player, crossing history and switch request.
[[nodiscard]] std::optional<elysia::scene::SceneRoute> find_boundary_route(
    RoomID from, std::span<const RoomCorridorLayout> corridors,
    elysia::core::Vector2 previous_center, const PlayerCharacter& player,
    const elysia::camera::Camera& camera);

// Called after the destination player has been registered with its physics world.
void restore_boundary_entry(const RoomBoundaryEntry& entry,
                            std::span<const RoomCorridorLayout> corridors,
                            const elysia::physics::ITileCollisionWorld& world,
                            PlayerCharacter& player);

void initialize_room_camera(elysia::scene::SceneCameraRuntime& camera,
                            const PlayerCharacter& player,
                            const std::optional<RoomBoundaryEntry>& entry);
}
