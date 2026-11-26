#include "fixed_room_scene.h"

#include "game/map/dungeon_room.h"

RoomBuildResult FixedRoomScene::build_room(const RoomBuildConfig& config)
{
    auto* map = create_and_add_object<DungeonRoom>(config.map_config, config.grid_size);

    return {.map = map, .collision_world = map, .corridors = map->corridors()};
}
