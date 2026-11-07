#pragma once

#include "character.h"

enum class EnemyFaction
{
    Skeleton,
    Goblin
};

enum class EnemyArchetype
{
    Melee,
    Ranged,
    Mage
};

struct EnemyDefinition
{
    EnemyFaction faction = EnemyFaction::Skeleton;
    EnemyArchetype archetype = EnemyArchetype::Melee;
    float purchase_cost = 1.0f;
};

// Basic enemy without autonomous movement or attacks.
class Enemy : public Character
{
public:
    explicit Enemy(elysia::core::Vector2 start_position, float max_health = 100.0f);
    Enemy(elysia::core::Vector2 start_position, const EnemyDefinition& definition);

    void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;

    [[nodiscard]] EnemyFaction faction() const noexcept { return _faction; }
    [[nodiscard]] EnemyArchetype archetype() const noexcept { return _archetype; }

private:
    EnemyFaction _faction = EnemyFaction::Skeleton;
    EnemyArchetype _archetype = EnemyArchetype::Melee;
};
