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
}

Vector2 FloorMinimapHud::screen_position(Vector2 grid) const noexcept
{
    return screen_rect().center() + (grid - _player_grid.value_or(Vector2{})) * kGridSpacing;
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

        const auto* destination = _floor.room_at(*neighbors[index]);
        const int gap = game::connection_gap(_floor, room, destination->id);
        const float fraction = game::room_edge_fraction(gap, corridor.direction);
        const auto neighbor = grid_point(*neighbors[index]);
        _player_grid = grid_point(*position) + (neighbor - grid_point(*position)) * (progress * fraction);
        return;
    }
}

void FloorMinimapHud::set_corridor_location(const CorridorTraversal& traversal, float progress)
{
    _current_room.reset();
    _player_grid.reset();
    const auto from = _floor.find_position(traversal.from);
    const auto to = _floor.find_position(traversal.to);
    const auto normal = game::corridor_normal(traversal.direction);
    if (!from || !to || normal.is_zero())
        return;
    const std::size_t index = static_cast<std::size_t>(
        std::ranges::find(directions, traversal.direction) - directions.begin());
    if (_floor.neighbors(*from)[index] != to)
        return;
    const int gap = game::connection_gap(_floor, traversal.from, traversal.to);
    if (gap < 1)
        return;
    const float room_part = game::room_edge_fraction(gap, traversal.direction);
    const float fraction = room_part + std::clamp(progress, 0.0f, 1.0f) * (1 - room_part * 2);
    _player_grid = grid_point(*from) + (grid_point(*to) - grid_point(*from)) * fraction;
}

void FloorMinimapHud::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const
{
    using namespace elysia::core;
    commands.push_back(make_ui_fill_rect_command(screen_rect(), Color{235, 238, 240, 215}, 8));
    const auto map_begin = commands.size();
    constexpr UiStrokeWidth line_width{UiStrokeWidthMode::Logical, 3};
    constexpr float block_size = 16;

    // Keep all topology, but clip its player-centered projection to the HUD viewport.
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
    const Rect clip{screen_rect().position() + Vector2{8, 8}, screen_rect().size() - Vector2{16, 16}};
    for (std::size_t i = map_begin; i < commands.size(); ++i)
        set_ui_command_clip_rect(commands[i], clip);
}
