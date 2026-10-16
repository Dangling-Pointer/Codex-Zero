#include "locator.h"

#include "game/characters/enemy.h"
#include "engine/object_query/game_object_query_service.h"

#include <algorithm>

using elysia::core::DepthLayer;
using elysia::core::Vector2;
using elysia::object_query::GameObjectQueryService;

Enemy* Locator::find_enemy(
    const Vector2& origin,
    EnemySelection selection) const
{
    switch (selection)
    {
        case EnemySelection::Closest:
            return find_closest_enemy(origin);

        case EnemySelection::Furthest:
            return find_furthest_enemy(origin);

        case EnemySelection::Median:
            return find_median_enemy(origin);
    }

    return nullptr;
}

Enemy* Locator::find_closest_enemy(const Vector2& origin) const
{
    return GameObjectQueryService::instance()
     ->find_nearest_object<Enemy>(
          origin,
          DepthLayer::Character,
         [](const Enemy& enemy)
         {
             return !enemy.is_dead();
         }
    );
}

Enemy* Locator::find_furthest_enemy(const Vector2& origin) const
{
    return GameObjectQueryService::instance()
        ->find_farthest_object<Enemy>(
         origin,
         DepthLayer::Character,
         [](const Enemy& enemy)
         {
                return !enemy.is_dead();
         }
    );
}

Enemy* Locator::find_median_enemy(const Vector2& origin) const
{
    auto enemies = GameObjectQueryService::instance()
        ->find_objects<Enemy>(
            DepthLayer::Character,
            [](const Enemy& enemy)
            {
                return !enemy.is_dead();
            }
        );

    if (enemies.empty())
    {
        return nullptr;
    }

    std::sort(
        enemies.begin(),
        enemies.end(),
        [&origin](const Enemy* first, const Enemy* second)
        {
            return origin.distance_squared_to(first->center())
                < origin.distance_squared_to(second->center());
        }
    );

    return enemies[enemies.size() / 2];
}