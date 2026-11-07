#include "enemy_catalog.h"

#include <utility>

EnemyCatalog::EnemyCatalog()
    // default definitions provide the initial roster used by game scenes
    : _definitions{
          {EnemyFaction::Skeleton, EnemyArchetype::Melee, 1.0f},
          {EnemyFaction::Skeleton, EnemyArchetype::Ranged, 2.0f},
          {EnemyFaction::Skeleton, EnemyArchetype::Mage, 3.0f},
          {EnemyFaction::Goblin, EnemyArchetype::Melee, 1.0f},
          {EnemyFaction::Goblin, EnemyArchetype::Ranged, 2.0f},
          {EnemyFaction::Goblin, EnemyArchetype::Mage, 3.0f}}
{
}

EnemyCatalog::EnemyCatalog(std::vector<EnemyDefinition> definitions)
    : _definitions(std::move(definitions))
{
}

const std::vector<EnemyDefinition> &EnemyCatalog::definitions() const noexcept
{
    return _definitions;
}
