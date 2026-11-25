#include "corridor_scene.h"

#include "game/map/dungeon_room.h"
#include "game/map/room_placement.h"
#include "game/scene/scene_keys.h"
#include "game/characters/player_character.h"

#include "game/gameflow/game_flow_manager.h"
#include <stdexcept>
#include <utility>

RoomBuildResult CorridorScene::build_room(const RoomBuildConfig& config)
{
    auto* map = create_and_add_object<DungeonRoom>(config.map_config, config.grid_size);
    if (corridor_traversal())
    {
        if (run_room())
            throw std::invalid_argument("Corridor scene cannot own a room ID");
        const auto layout = game::make_corridor_layout(
            GameFlowManager::instance()->current_floor(), *corridor_traversal());
        auto exits = layout.world_exits(map->world_origin(), map->tile_size());
        return {map, map, game::find_room_spawn(*map, game::kPlayerSpawnSize, exits.front().seam_center(), {}),
                std::move(exits), true};
    }
    const auto preferred = map->position()
        + elysia::core::Vector2{700, map->size().y * 0.5f};
    const auto spawn = game::find_room_spawn(*map, game::kPlayerSpawnSize, preferred, {});

    return {map, map, spawn, {}, true};
}

elysia::scene::SceneRoute make_corridor_test_route(const CharacterContext& character)
{
    const elysia::core::Vector2 grid_size{60, 7};
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

void CorridorScene::on_game_fixed_update(std::uint64_t, double)
{
    if (player())
        (void)player()->take_fire_requests();
}

