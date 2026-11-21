#include "floor_minimap_hud.h"
#include "engine/core/render/colors.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
using elysia::core::Color;
using elysia::core::Vector2;

constexpr std::array<RoomDirection, 4> directions{
    RoomDirection::Up, RoomDirection::Right, RoomDirection::Down, RoomDirection::Left,
};

Vector2 grid_point(FloorPosition position)
{
    return {static_cast<float>(position.x), static_cast<float>(position.y)};
}

Color room_color(RoomType type)
{
    switch (type)
    {
    case RoomType::Start: return {24, 62, 42};
    case RoomType::End: return {56, 28, 72};
    case RoomType::Shop: return {72, 49, 20};
    case RoomType::Rest: return {22, 55, 65};
    case RoomType::Combat: return {64, 24, 28};
    case RoomType::Elite: case RoomType::MiniBoss: case RoomType::Boss: return {84, 23, 39};
    default: return elysia::core::colors::gray_900;
    }
}
}

FloorMinimapHud::FloorMinimapHud(const elysia::core::Rect& bounds, const Floor& floor)
    : UiElement(bounds, 10), _floor(floor)
{
    int left = Floor::width, top = Floor::height, right = -1, bottom = -1;
    for (int y = 0; y < Floor::height; ++y)
        for (int x = 0; x < Floor::width; ++x)
            if (floor.room_at({x, y}))
            {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
    if (right < left)
        return;

    _grid_center = {(left + right) * 0.5f, (top + bottom) * 0.5f};
    _spacing = std::max(1.0f, std::min({32.0f,
        (bounds.width() - 44) / std::max(1, right - left),
        (bounds.height() - 44) / std::max(1, bottom - top)}));
}

Vector2 FloorMinimapHud::screen_position(Vector2 grid) const noexcept
{
    return screen_rect().center() + (grid - _grid_center) * _spacing;
}

void FloorMinimapHud::set_player_location(
    RoomID room, Vector2 world_center, std::span<const game::RoomCorridorLayout> corridors)
{
    _current_room = room;
    const auto position = _floor.find_position(room);
    _player_grid = position ? std::optional{grid_point(*position)} : std::nullopt;
    if (!position)
        return;

    const auto neighbors = _floor.neighbors(*position);
    for (const auto& corridor : corridors)
    {
        // Use the footprint for the lateral check: the sprite can overhang a side wall.
        if (!corridor.footprint.contains(world_center))
            continue;
        const auto index = std::ranges::find(directions, corridor.direction) - directions.begin();
        if (index == directions.size() || !neighbors[index])
            continue;

        const auto normal = game::corridor_normal(corridor.direction);
        const float half_length = (normal.x == 0 ? corridor.walkable.height() : corridor.walkable.width()) * 0.5f;
        if (half_length <= 0)
            continue;
        const auto inner_edge = corridor.seam_center() - normal * half_length;
        const auto offset = world_center - inner_edge;
        const float distance = offset.x * normal.x + offset.y * normal.y;
        const float progress = std::clamp(distance / half_length, 0.0f, 1.0f);

        // Each scene represents its own half of the edge; both seams map to the same midpoint.
        const auto neighbor = grid_point(*neighbors[index]);
        _player_grid = grid_point(*position) + (neighbor - grid_point(*position)) * (progress * 0.5f);
        return;
    }
}

void FloorMinimapHud::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const
{
    using namespace elysia::core;
    commands.push_back(make_ui_fill_rect_command(screen_rect(), Color{235, 238, 240, 215}, 8));
    constexpr UiStrokeWidth line_width{UiStrokeWidthMode::Logical, 3};
    const float block_size = std::min(16.0f, _spacing * 0.5f);

    // Show the whole floor for this first test version, including long links across empty cells.
    for (int y = 0; y < Floor::height; ++y)
        for (int x = 0; x < Floor::width; ++x)
        {
            const auto* room = _floor.room_at({x, y});
            if (!room)
                continue;
            const auto center = screen_position(grid_point({x, y}));
            for (const auto& neighbor : _floor.neighbors({x, y}))
                if (neighbor && _floor.room_at(*neighbor)->id > room->id)
                    commands.push_back(make_ui_draw_line_command(
                        center, screen_position(grid_point(*neighbor)), colors::black, line_width));
        }

    for (int y = 0; y < Floor::height; ++y)
        for (int x = 0; x < Floor::width; ++x)
        {
            const auto* room = _floor.room_at({x, y});
            if (!room)
                continue;
            const auto block = Rect::from_center(screen_position(grid_point({x, y})), {block_size, block_size});
            commands.push_back(make_ui_fill_rect_command(block, room_color(room->type), 3));
            if (room->visited || _current_room == room->id)
                commands.push_back(make_ui_draw_rect_command(block, colors::gray_500,
                    3, UiStrokeWidth{UiStrokeWidthMode::Logical, 1}));
        }

    if (_player_grid)
    {
        const auto marker = screen_position(*_player_grid);
        commands.push_back(make_ui_fill_circle_command(marker, 5, colors::white));
        commands.push_back(make_ui_fill_circle_command(marker, 3.5f, colors::red_500));
    }
}
