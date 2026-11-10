#include "portal.h"
#include "engine/core/render/render_command.h"
#include "engine/core/render/colors.h"

Portal::Portal(elysia::core::Rect bounds) : GameObject(elysia::core::DepthLayer::Item)
{
    set_world_rect(bounds);
}

void Portal::check_reached(const elysia::core::Rect& player_bounds)
{
    if (world_rect().intersection(player_bounds).area() > 0)
        reach();
}

void Portal::reach()
{
    if (_reached || is_destroyed() || !is_active())
        return;
    _reached = true;

    const auto callback = _on_reached;
    if (callback)
        callback();
}

void Portal::reset() noexcept
{
    GameObject::reset();
    _reached = false;
}

void Portal::submit_render_commands(std::vector<elysia::core::RenderCommand>& commands) const
{
    if (is_destroyed() || !is_visible())
        return;

    commands.push_back(elysia::core::make_world_fill_rect_command(render_rect(), elysia::core::colors::elysia_deep_sea));

    commands.push_back(elysia::core::make_world_draw_rect_command(render_rect(), elysia::core::colors::abyss_blue, 2));
}
