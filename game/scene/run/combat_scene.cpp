#include "combat_scene.h"
#include "game/map/dungeon_room.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/enemy_generation/enemy_generator.h"
#include "game/enemy_generation/enemy_spawn_layout.h"
#include "game/gameflow/game_flow_manager.h"

#include <stdexcept>

RoomBuildResult CombatScene::build_room(const RoomBuildConfig& config)
{
    _map = create_and_add_object<DungeonRoom>(config.map_config, config.grid_size);

    return {.map = _map, .collision_world = _map};
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

    std::vector<elysia::core::Rect> occupied{player()->world_rect()};

    for (const auto& spawn : spawns)
    {
        auto enemy = std::make_unique<Enemy>(spawn.position, spawn.definition);
        const auto position = room_manager().find_spawn(enemy->size(), spawn.position, {.occupied = occupied});
        if (!position)
            throw std::runtime_error("Room has no free walkable spawn area");
        enemy->set_position(*position);
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

void CombatScene::on_after_update(double delta)
{
    const auto* room = run_room();
    const auto* map = room_map();
    const auto* character = player();
    if (!is_paused() && room && !room->cleared && map && !map->is_destroyed()
        && character && !character->is_destroyed() && character->is_active() && !character->is_dead()
        && GameFlowManager::instance()->get_current_room_id() == room->id && can_leave_room())
        GameFlowManager::instance()->mark_current_room_cleared();

    RoomScene::on_after_update(delta);
}
