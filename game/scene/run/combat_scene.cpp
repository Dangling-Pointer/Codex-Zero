#include "combat_scene.h"
#include "game/map/dungeon_room.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/enemy_generation/enemy_generator.h"
#include "game/enemy_generation/enemy_spawn_layout.h"
#include "game/map/room_placement.h"
#include "game/objcets/portal.h"

RoomBuildResult CombatScene::build_room(const RoomBuildConfig& config)
{
    _map = create_and_add_object<DungeonRoom>(config.map_config, config.grid_size);

    return {_map, _map, game::player_spawn_near_portal(*_map)};
}

void CombatScene::create_room_content()
{
    const auto* room = run_room();

    if (room && room->cleared)
        return;

    EnemyGenerator generator;
    EnemyGenerator::Config config;
    config.difficulty = room && room->type == RoomType::Elite ? 6.0f : 5.0f;
    config.base_budget = 5.0f;

    const auto purchases = generator.generate(config);
    const auto spawns = EnemySpawnLayout{}.map(purchases.purchases, _map->center());

    std::vector<elysia::core::Rect> occupied{player()->world_rect(), portal()->world_rect()};

    for (const auto& spawn : spawns)
    {
        auto enemy = std::make_unique<Enemy>(spawn.position, spawn.definition);
        enemy->set_position(game::find_room_spawn(*_map, enemy->size(), spawn.position, occupied));
        const auto bounds = enemy->world_rect();
        add_object(std::move(enemy));
        occupied.push_back(bounds);
    }
}

bool CombatScene::can_leave_room() const
{
    if (!run_room())
        return true;

    bool enemies_remaining = false;

    const auto& objects = static_cast<const elysia::object_query::IGameObjectQueryRuntime&>(*this);
    objects.visit_game_objects(elysia::core::DepthLayerMask::all(),
        [&](const auto& object) {
            const auto* enemy = dynamic_cast<const Enemy*>(&object);
            enemies_remaining = enemy && !enemy->is_destroyed() && !enemy->is_dead();
            return !enemies_remaining;
        });

    return !enemies_remaining;
}
