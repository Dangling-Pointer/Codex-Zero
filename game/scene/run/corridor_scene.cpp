#include "corridor_scene.h"

#include "game/map/dungeon_room.h"
#include "game/map/room_placement.h"
#include "game/scene/scene_keys.h"
#include "game/characters/player_character.h"

#include "engine/tools/logger.h"

RoomBuildResult CorridorScene::build_room(const RoomBuildConfig& config)
{
    auto* map = create_and_add_object<DungeonRoom>(config.map_config, config.grid_size);
    const auto tile = map->tile_size();
    const auto preferred = map->position()
        + elysia::core::Vector2{700, map->size().y * 0.5f};
    const auto spawn = game::find_room_spawn(*map, game::kPlayerSpawnSize, preferred, {});

    return {map, map, spawn};
}

elysia::scene::SceneRoute make_corridor_test_route(const CharacterContext& character)
{
    elysia::core::Vector2 grid_size{ 60, 7 };
    //elysia::core::Vector2 grid_size{60, 7};
    const RoomBuildConfig room{
        .map_config = MapConfig{
            .baseRoomSize = grid_size,
            .numSubRooms = 0,
            .seed = 0,
        },
        .grid_size = grid_size,
    };
    return {
        .target = Corridor,
        .payload = RoomScenePayload{.character = character, .room = room},
        .reload_mode = elysia::scene::SceneReloadMode::Reuse,
        .held_input_on_enter = elysia::scene::HeldInputOnEnter::Preserve
    };
}

void CorridorScene::on_before_update(double delta)
{
    RoomScene::on_before_update(delta);
    PlayerCharacter* player = RoomScene::player();
    elysia::core::Vector2 pos = player->position();

    auto* map  = RoomScene::room_map();

    int x = map->size().x;
    //ELYSIA_LOG_DEBUG("CorridorScene", "player pos:" << pos.x << "");

    if (pos.x > 1500 || pos.x<350)
    {
        ELYSIA_LOG_DEBUG("CorridorScene", "player pos:" << pos.x << "");
        Scene::request_scene_switch(make_corridor_test_route(player->capture_context()));
    }

}

