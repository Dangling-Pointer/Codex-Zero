#pragma once

#include "engine/tools/singleton.h"
#include "engine/core/geometry/vector2.h"

class Enemy;

// Determines how an enemy is selected based on its distance from an origin
enum class EnemySelection
{
    Closest,
    Furthest,
    Median
};

// Shared access to game object and position search functions
class Locator final : public elysia::tools::Singleton<Locator>
{
    friend class elysia::tools::Singleton<Locator>;

public:
    // Return an enemy based on selected distance, or a nullptr if not possible
    [[nodiscard]] Enemy* find_enemy(const elysia::core::Vector2& origin,
                      EnemySelection selection) const;

private:
    // Limits constuctor to a THE singleton helper
    Locator() = default;

    // Return closest living enemy to origin, nullptr if impossible
    [[nodiscard]] Enemy* find_closest_enemy(
        const elysia::core::Vector2& origin) const;

    // Return furthest, nullptr if impossible
    [[nodiscard]] Enemy* find_furthest_enemy(
        const elysia::core::Vector2& origin) const;

    // Return middle-distance enemy, nullptr if impossible
    [[nodiscard]] Enemy* find_median_enemy(
        const elysia::core::Vector2& origin) const;
};