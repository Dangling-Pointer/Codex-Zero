#include "room_scene_payload.h"

#include "game/gameflow/game_flow_manager.h"
#include "game/scene/scene_keys.h"

#include <stdexcept>

elysia::scene::SceneRoute make_run_room_route(
    RoomID room_id, const CharacterContext& character, std::optional<RoomID> from_room)
{
    const auto& flow = *GameFlowManager::instance();
    const Room* room = flow.find_room(room_id);

    if (!room)
        throw std::invalid_argument("Unknown dungeon room");

    RoomScenePayload payload{
        .character = character,
        .room = {},
        .room_id = room_id,
        .from_room = from_room,
    };

    if (room->type == RoomType::Combat || room->type == RoomType::Elite)
    {
        payload.room.map_config.seed = flow.room_seed(room_id);
        return {.target = CombatRoom, .payload = payload};
    }

    const elysia::core::Vector2 start_grid{15, 15};
    const elysia::core::Vector2 default_grid{12, 10};
    payload.room.grid_size = room->type == RoomType::Start ? start_grid : default_grid;
    payload.room.map_config.baseRoomSize = payload.room.grid_size;
    payload.room.map_config.numSubRooms = 0;
    return {.target = FixedRoom, .payload = payload};
}
