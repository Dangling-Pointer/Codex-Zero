#include "room_scene_payload.h"

#include "game/gameflow/game_flow_manager.h"
#include "game/scene/scene_keys.h"
#include "game/map/corridor/room_corridor_layout.h"
#include "engine/camera/camera.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace
{
bool finite(elysia::core::Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

const Room& require_boundary_destination(RoomID from, RoomDirection exit)
{
    const auto& flow = *GameFlowManager::instance();
    const auto* source = flow.find_room(from);
    const auto* target = flow.connected_room(from, exit);
    if (flow.get_current_room_id() != from || !source || !target
        || !supports_room_corridors(source->type) || !supports_room_corridors(target->type))
        throw std::invalid_argument("Boundary route requires connected fixed rooms");

    return *target;
}

RoomScenePayload make_room_payload(
    const Room& room, const CharacterContext& character, std::optional<RoomID> from_room)
{
    const auto& flow = *GameFlowManager::instance();
    RoomScenePayload payload{
        .character = character,
        .room = {},
        .room_id = room.id,
        .from_room = from_room,
    };

    if (room.type == RoomType::Combat || room.type == RoomType::Elite)
    {
        payload.room.map_config.seed = flow.room_seed(room.id);
        return payload;
    }

    const elysia::core::Vector2 start_grid{15, 15};
    const elysia::core::Vector2 default_grid{12, 10};
    payload.room.grid_size = room.type == RoomType::Start ? start_grid : default_grid;
    payload.room.map_config.baseRoomSize = payload.room.grid_size;
    payload.room.map_config.numSubRooms = 0;

    constexpr std::array<RoomDirection, 4> directions{
        RoomDirection::Up, RoomDirection::Right, RoomDirection::Down, RoomDirection::Left,
    };
    auto& corridors = payload.room.map_config.corridors;
    for (const auto direction : directions)
    {
        const auto* neighbor = flow.connected_room(room.id, direction);
        if (neighbor && supports_room_corridors(neighbor->type))
            corridors.directions = static_cast<RoomDirection>(
                std::to_underlying(corridors.directions) | std::to_underlying(direction));
    }
    payload.room.grid_size += corridors.padding() * 2;
    return payload;
}
}

bool supports_room_corridors(RoomType type) noexcept
{
    return type != RoomType::Combat && type != RoomType::Elite && type != RoomType::NONE;
}

void validate_room_entry(const RoomScenePayload& payload)
{
    if (payload.boundary_entry)
    {
        const auto& entry = *payload.boundary_entry;
        const bool valid_motion = finite(entry.motion.direction) && finite(entry.motion.velocity)
            && finite(entry.motion.render_offset);
        const bool valid_camera = finite(entry.camera_offset) && std::isfinite(entry.camera_zoom)
            && entry.camera_zoom >= elysia::camera::Camera::kMinZoom
            && entry.camera_zoom <= elysia::camera::Camera::kMaxZoom;
        if (!finite(entry.seam_offset) || !valid_motion || !valid_camera
            || game::corridor_normal(entry.direction).is_zero())
            throw std::invalid_argument("Invalid boundary entry state");
    }

    if (payload.corridor)
    {
        const auto& traversal = *payload.corridor;
        const auto& destination = require_boundary_destination(traversal.from, traversal.direction);
        const auto& floor = GameFlowManager::instance()->current_floor();
        if (destination.id != traversal.to || game::connection_gap(floor, traversal.from, traversal.to) < 1
            || !payload.boundary_entry)
            throw std::invalid_argument("Invalid corridor traversal");

        if (!payload.room_id)
        {
            const auto layout = game::make_corridor_layout(floor, traversal);
            const auto& map = payload.room.map_config;
            if (payload.from_room || payload.boundary_entry->direction != game::opposite_direction(traversal.direction)
                || payload.room.grid_size != layout.grid_size || map.baseRoomSize != layout.grid_size
                || map.numSubRooms != 0 || map.corridors.directions != RoomDirection::None)
                throw std::invalid_argument("Invalid corridor scene entry");
        }
        else
        {
            const bool returning = *payload.room_id == traversal.from;
            const auto entrance = returning ? traversal.direction : game::opposite_direction(traversal.direction);
            if ((!returning && *payload.room_id != traversal.to) || payload.from_room != traversal.from
                || payload.boundary_entry->direction != entrance)
                throw std::invalid_argument("Corridor exit does not match its room entrance");
            if (!returning && !GameFlowManager::instance()->can_enter_room(traversal.to))
                throw std::invalid_argument("Corridor destination is no longer reachable");
        }
        return;
    }

    if (payload.boundary_entry)
    {
        if (!payload.from_room || !payload.room_id)
            throw std::invalid_argument("Boundary room entry requires source and destination IDs");

        const auto& destination = require_boundary_destination(
            *payload.from_room, game::opposite_direction(payload.boundary_entry->direction));
        if (destination.id != *payload.room_id)
            throw std::invalid_argument("Boundary entry requires reciprocal fixed-room exits");
    }

    if (!payload.room_id)
    {
        if (payload.from_room)
            throw std::invalid_argument("Dungeon room entry requires a destination");
        return;
    }

    const auto& flow = *GameFlowManager::instance();
    const auto current_room = flow.get_current_room_id();
    const auto destination = *payload.room_id;
    if (payload.from_room)
    {
        if (*payload.from_room != current_room || !flow.can_enter_room(destination))
            throw std::invalid_argument("Invalid dungeon room entry");
        return;
    }

    const auto* room = flow.find_room(destination);
    if (!room || room->type != RoomType::Start || room->id != current_room)
        throw std::invalid_argument("Invalid dungeon room entry");
}

elysia::scene::SceneRoute make_boundary_room_route(
    RoomID from, RoomDirection exit, const CharacterContext& character, RoomBoundaryEntry entry)
{
    const auto& destination = require_boundary_destination(from, exit);
    entry.direction = game::opposite_direction(exit);
    const auto& floor = GameFlowManager::instance()->current_floor();
    if (game::connection_gap(floor, from, destination.id) > 0)
    {
        const CorridorTraversal traversal{from, destination.id, exit};
        const auto layout = game::make_corridor_layout(floor, traversal);
        RoomScenePayload payload{.character = character};
        payload.room.grid_size = layout.grid_size;
        payload.room.map_config.baseRoomSize = layout.grid_size;
        payload.room.map_config.numSubRooms = 0;
        payload.room.map_config.seed = 0;
        payload.boundary_entry = entry;
        payload.corridor = traversal;
        return {.target = Corridor, .payload = std::move(payload),
                .held_input_on_enter = elysia::scene::HeldInputOnEnter::Preserve};
    }

    auto payload = make_room_payload(destination, character, from);
    payload.boundary_entry = entry;
    return {
        .target = FixedRoom,
        .payload = std::move(payload),
        .held_input_on_enter = elysia::scene::HeldInputOnEnter::Preserve,
    };
}

elysia::scene::SceneRoute make_corridor_room_route(
    const CorridorTraversal& traversal, RoomDirection exit,
    const CharacterContext& character, RoomBoundaryEntry entry)
{
    const auto& destination = require_boundary_destination(traversal.from, traversal.direction);
    const auto& floor = GameFlowManager::instance()->current_floor();
    if (destination.id != traversal.to || game::connection_gap(floor, traversal.from, traversal.to) < 1)
        throw std::invalid_argument("Corridor no longer connects its original endpoints");
    const bool returning = exit == game::opposite_direction(traversal.direction);
    if (!returning && exit != traversal.direction)
        throw std::invalid_argument("Corridor exit must lead to one of its two rooms");
    const auto* room = GameFlowManager::instance()->find_room(returning ? traversal.from : traversal.to);
    auto payload = make_room_payload(*room, character, traversal.from);
    entry.direction = game::opposite_direction(exit);
    payload.boundary_entry = entry;
    payload.corridor = traversal;
    return {.target = FixedRoom, .payload = std::move(payload),
            .held_input_on_enter = elysia::scene::HeldInputOnEnter::Preserve};
}

elysia::scene::SceneRoute make_run_room_route(
    RoomID room_id, const CharacterContext& character, std::optional<RoomID> from_room)
{
    const auto* room = GameFlowManager::instance()->find_room(room_id);
    if (!room)
        throw std::invalid_argument("Unknown dungeon room");

    const bool combat = room->type == RoomType::Combat || room->type == RoomType::Elite;
    return {.target = combat ? CombatRoom : FixedRoom,
            .payload = make_room_payload(*room, character, from_room)};
}
