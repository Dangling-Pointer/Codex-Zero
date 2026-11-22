#include "room_transition.h"

#include "engine/scene/runtime/scene_camera_runtime.h"
#include "game/characters/player_character.h"
#include "game/gameflow/game_flow_manager.h"
#include "game/map/room_placement.h"

#include <algorithm>
#include <stdexcept>

namespace game
{
namespace
{
RoomBoundaryEntry capture_entry(const RoomCorridorLayout& exit, const PlayerCharacter& player,
                                 const elysia::camera::Camera& camera)
{
    return {.seam_offset = player.center() - exit.seam_center(),
            .motion = player.capture_motion(),
            .camera_offset = camera.center() - player.render_rect().center(),
            .camera_zoom = camera.zoom()};
}
}

std::optional<elysia::scene::SceneRoute> find_boundary_route(
    RoomID from, std::span<const RoomCorridorLayout> corridors,
    elysia::core::Vector2 previous_center, const PlayerCharacter& player,
    const elysia::camera::Camera& camera)
{
    const auto& flow = *GameFlowManager::instance();
    for (const auto& corridor : corridors)
    {
        const auto* destination = flow.connected_room(from, corridor.direction);
        if (!destination || !supports_room_corridors(destination->type))
            continue;
        if (!corridor.crossed_outward(previous_center, player.center(), player.blocking_bounds()))
            continue;

        const auto entry = capture_entry(corridor, player, camera);
        return make_boundary_room_route(from, corridor.direction, player.capture_context(), entry);
    }
    return std::nullopt;
}

std::optional<elysia::scene::SceneRoute> find_corridor_exit_route(
    const CorridorTraversal& traversal, std::span<const RoomCorridorLayout> exits,
    elysia::core::Vector2 previous_center, const PlayerCharacter& player,
    const elysia::camera::Camera& camera)
{
    for (const auto& exit : exits)
        if (exit.crossed_outward(previous_center, player.center(), player.blocking_bounds()))
            return make_corridor_room_route(traversal, exit.direction, player.capture_context(),
                                             capture_entry(exit, player, camera));
    return std::nullopt;
}

void restore_boundary_entry(const RoomBoundaryEntry& entry,
                            std::span<const RoomCorridorLayout> corridors,
                            const elysia::physics::ITileCollisionWorld& world,
                            PlayerCharacter& player)
{
    const auto entrance = std::ranges::find(corridors, entry.direction, &RoomCorridorLayout::direction);
    if (entrance == corridors.end())
        throw std::invalid_argument("Destination has no matching corridor entrance");

    const auto normal = corridor_normal(entry.direction);
    const float outward_offset = entry.seam_offset.x * normal.x + entry.seam_offset.y * normal.y;
    if (outward_offset > 0.1f)
        throw std::invalid_argument("Boundary entry must be on the destination's interior side");

    // Keep world axes unchanged: this preserves lateral position and crossing overshoot.
    const auto center = entrance->seam_center() + entry.seam_offset;
    const auto position = center - player.size() * 0.5f;
    auto body = player.blocking_bounds();
    body.set_position(body.position() + position - player.position());

    // Validate the blocking body, allowing the solver's subpixel wall slop.
    constexpr float slop = 0.1f;
    const elysia::core::Rect interior{body.position() + elysia::core::Vector2{slop, slop},
                                     body.size() - elysia::core::Vector2{slop * 2, slop * 2}};
    if (!walkable_bounds(world, interior))
        throw std::invalid_argument("Boundary entry collision body is outside walkable ground");
    if (!player.physics_world()->teleport_object(player.physics_handle(), position))
        throw std::runtime_error("Cannot place the player at the corridor entrance");
    player.restore_motion(entry.motion);
}

void initialize_room_camera(elysia::scene::SceneCameraRuntime& camera,
                            const PlayerCharacter& player,
                            const std::optional<RoomBoundaryEntry>& entry)
{
    constexpr auto slot = elysia::camera::CameraSlot::Main;
    camera.set_follow_strategy(slot, std::make_unique<elysia::camera::HardFollowStrategy>());
    camera.set_zoom(slot, entry ? entry->camera_zoom : 2.0f);
    camera.set_center(slot, entry ? player.render_rect().center() + entry->camera_offset : player.center());
}
}
