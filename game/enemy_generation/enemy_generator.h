#pragma once

#include "../characters/enemy.h"
#include "enemy_catalog.h"

#include <cstdint>
#include <vector>

/*
this generator converts difficulty into a purchase budget,
it filters the catalog, selects affordable variety, and returns purchase records,
placement and scene ownership are deliberately handled by the spawn layout and scene
 */

struct EnemySpawn
{
    EnemyDefinition definition;
    elysia::core::Vector2 position;
};

class EnemyGenerator
{
public:
    struct Config
    {
        float difficulty = 3.0f;
        float base_budget = 4.0f;

        std::vector<EnemyFaction> factions{
            EnemyFaction::Skeleton, EnemyFaction::Goblin};

        std::uint32_t seed = 0;
    };

    struct Result
    {
        std::vector<EnemySpawn> purchases;
        float budget = 0.0f;
        float spent = 0.0f;
    };

    explicit EnemyGenerator(EnemyCatalog catalog = {});

    // purchase a composition of enemies
    [[nodiscard]] Result generate(const Config &config) const;

private:
    EnemyCatalog _catalog;
};
