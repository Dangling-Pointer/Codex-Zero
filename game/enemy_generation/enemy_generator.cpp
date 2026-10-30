#include "enemy_generator.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

/*
 the generator shuffles eligible definitions, then buys until no affordable definition remains
 it prefers an unused archetype while possible and uses the budget as the hard spending limit
 repeated definitions are allowed after available archetypes have been represented
 */

// TODO bug : random selection heavily favoring one type in free selection mode

EnemyGenerator::EnemyGenerator(EnemyCatalog catalog)
    : _catalog(std::move(catalog))
{
}

EnemyGenerator::Result EnemyGenerator::generate(const Config &config) const
{
    Result result;

    // invalid room setup
    if (!std::isfinite(config.difficulty) ||
        !std::isfinite(config.base_budget) || config.difficulty <= 0.0f ||
        config.base_budget <= 0.0f)
        return result;

    // init, calculate budget set seed
    result.budget = config.difficulty * config.base_budget;
    std::mt19937 random(config.seed == 0 ? std::random_device{}() : config.seed);
    std::vector<EnemyDefinition> available;

    // keep only definitions this room is allowed to purchase
    for (const auto &definition : _catalog.definitions())
    {
        if (definition.purchase_cost <= result.budget &&
            std::isfinite(definition.purchase_cost) && definition.purchase_cost > 0.0f &&
            std::find(config.factions.begin(), config.factions.end(), definition.faction) !=
                config.factions.end())
            available.push_back(definition);
    }

    // shuffle available options
    std::shuffle(available.begin(), available.end(), random);
    std::vector<EnemyArchetype> purchased_archetypes;

    const auto is_affordable = [&](const EnemyDefinition &definition)
    {
        return result.spent + definition.purchase_cost <= result.budget;
    };

    while (true)
    {
        if (!std::any_of(available.begin(), available.end(), is_affordable))
            break;

        // prefer an archetype not used yet
        const bool need_new_archetype =
            std::any_of(available.begin(), available.end(), [&](const EnemyDefinition &definition)
                        { return is_affordable(definition) &&
                                 std::find(purchased_archetypes.begin(), purchased_archetypes.end(),
                                           definition.archetype) == purchased_archetypes.end(); });

        const auto candidate = std::find_if(
            available.begin(), available.end(), [&](const EnemyDefinition &definition)
            { return is_affordable(definition) &&
                     (!need_new_archetype ||
                      std::find(purchased_archetypes.begin(), purchased_archetypes.end(),
                                definition.archetype) == purchased_archetypes.end()); });

        if (candidate == available.end())
            break;

        const EnemyDefinition definition = *candidate;
        result.spent += definition.purchase_cost;

        if (std::find(purchased_archetypes.begin(), purchased_archetypes.end(),
                      definition.archetype) == purchased_archetypes.end())
            purchased_archetypes.push_back(definition.archetype);

        result.purchases.push_back({definition, {}});
    }

    return result;
}
