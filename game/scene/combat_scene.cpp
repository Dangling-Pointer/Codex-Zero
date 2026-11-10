#include "combat_scene.h"
#include "game/map/dungeon_room.h"
#include "game/characters/player_character.h"
#include "game/characters/enemy.h"
#include "game/enemy_generation/enemy_generator.h"
#include "game/enemy_generation/enemy_spawn_layout.h"
#include <limits>
#include <stdexcept>

namespace
{
elysia::core::Vector2 spawn_position(const DungeonRoom& map, elysia::core::Vector2 preferred,
                                    const std::vector<elysia::core::Vector2>& occupied)
{
    float nearest = std::numeric_limits<float>::max();
    elysia::core::Vector2 result;
    const auto size = map.tile_render_size();
    for (int y = 0; y < map.rows(); ++y)
        for (int x = 0; x < map.columns(); ++x)
        {
            if (map.tile_map().get(x, y).type != TileType::Floor) continue;
            auto at = map.position() + elysia::core::Vector2{x * size.x, y * size.y};
            if (std::ranges::find(occupied, at) != occupied.end()) continue;
            const float distance = at.distance_squared_to(preferred);
            if (distance < nearest) { nearest = distance; result = at; }
        }
    if (nearest == std::numeric_limits<float>::max())
        throw std::runtime_error("Room has no free walkable spawn tile");
    return result;
}
}
RoomBuildResult CombatScene::build_room(const RoomBuildConfig& config)
{
    _map = create_and_add_object<DungeonRoom>(config.map_config, config.grid_size);
    return {_map, _map, spawn_position(*_map, _map->center(), {})};
}
void CombatScene::create_room_content()
{
    EnemyGenerator generator;
    EnemyGenerator::Config config;
    config.difficulty = 5.0f;
    config.base_budget = 5.0f;
    const auto purchases = generator.generate(config);
    const auto spawns = EnemySpawnLayout{}.map(purchases.purchases, _map->center());
    std::vector<elysia::core::Vector2> occupied{player()->position()};
    for (const auto& spawn : spawns)
    {
        auto at = spawn_position(*_map, spawn.position, occupied);
        create_and_add_object<Enemy>(at, spawn.definition);
        occupied.push_back(at);
    }
}
