#include "enemy.h"

#include "engine/core/render/colors.h"
#include "engine/core/render/render_command.h"

Enemy::Enemy(elysia::core::Vector2 start_position, float max_health)
    : Character(start_position, {30, 30}, {0, 0, 30, 30}, 200.0f,
                elysia::gameplay::collision::teams::Enemy, max_health)
{
}

void Enemy::submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const
{
    out_commands.push_back(elysia::core::make_world_fill_rect_command(
        render_rect(), is_dead() ? elysia::core::colors::gray_500 : elysia::core::colors::red_500));
}
