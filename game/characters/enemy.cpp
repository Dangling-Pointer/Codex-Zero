#include "enemy.h"

#include "engine/core/render/colors.h"
#include "engine/core/render/render_command.h"

Enemy::Enemy(elysia::core::Vector2 start_position, float max_health)
    : Character(start_position, {30, 30}, {0, 0, 30, 30}, 200.0f,
                elysia::gameplay::collision::teams::Enemy, max_health)
{
}

Enemy::Enemy(elysia::core::Vector2 start_position, const EnemyDefinition& definition)
    : Character(start_position, {30, 30}, {0, 0, 30, 30}, 200.0f,
                elysia::gameplay::collision::teams::Enemy),
      _faction(definition.faction),
      _archetype(definition.archetype)
{
}

void Enemy::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    const elysia::core::Color faction_color =
        _faction == EnemyFaction::Skeleton ? elysia::core::colors::white
                                           : elysia::core::colors::green_500;
    const elysia::core::Color marker_color =
        _faction == EnemyFaction::Skeleton ? elysia::core::colors::black
                                           : elysia::core::colors::white;
    const auto rect = render_rect();

    out_commands.push_back(elysia::core::make_world_fill_rect_command(
        rect, is_dead() ? elysia::core::colors::gray_500 : faction_color));

    // draw one, two, or three lines to show the enemy archetype
    int line_count = 1;
    switch (_archetype)
    {
    case EnemyArchetype::Ranged:
        line_count = 2;
        break;
    case EnemyArchetype::Mage:
        line_count = 3;
        break;
    case EnemyArchetype::Melee:
        break;
    }

    for (int line = 0; line < line_count; ++line)
    {
        const float y = rect.top() + 8.0f + static_cast<float>(line) * 7.0f;
        out_commands.push_back(elysia::core::make_world_draw_line_command(
            {rect.left() + 7.0f, y}, {rect.right() - 7.0f, y},
            marker_color, 3.0f));
    }
}
