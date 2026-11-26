#include "corridor_layout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace game
{
using elysia::core::Vector2;
using elysia::core::Rect;

int connection_gap(const Floor& floor, RoomID from, RoomID to) noexcept
{
    const auto first = floor.find_position(from);
    const auto second = floor.find_position(to);
    if (!first || !second || from == to || (first->x != second->x && first->y != second->y))
        return -1;
    return std::abs(first->x - second->x) + std::abs(first->y - second->y) - 1;
}

int extra_corridor_tiles(int empty_cells) noexcept
{
    return empty_cells > 0 ? 13 + 6 * std::min(empty_cells - 1, 2) : 0;
}

float room_corridor_half_tiles(RoomDirection direction) noexcept
{
    switch (direction)
    {
    case RoomDirection::Up: case RoomDirection::Down:
        return kStandardRoomCorridors.vertical_length * 0.5f;
    case RoomDirection::Left: case RoomDirection::Right:
        return kStandardRoomCorridors.horizontal_length * 0.5f;
    default: return 0;
    }
}

float room_edge_fraction(int empty_cells, RoomDirection direction) noexcept
{
    const float half = room_corridor_half_tiles(direction);
    const float total = half * 2 + extra_corridor_tiles(empty_cells);
    return total > 0 ? half / total : 0;
}

CorridorLayout make_corridor_layout(const Floor& floor, const CorridorTraversal& traversal)
{
    const int gap = connection_gap(floor, traversal.from, traversal.to);
    const auto normal = corridor_normal(traversal.direction);
    if (gap < 1 || normal.is_zero())
        throw std::invalid_argument("Corridor requires a cardinal connection across empty cells");
    const auto from = *floor.find_position(traversal.from);
    const auto to = *floor.find_position(traversal.to);
    if ((to.x - from.x) * normal.x + (to.y - from.y) * normal.y != gap + 1)
        throw std::invalid_argument("Corridor direction does not point from source to target");

    const int extra = extra_corridor_tiles(gap);
    const int buffer = static_cast<int>(std::ceil(room_corridor_half_tiles(traversal.direction)));
    const float length = static_cast<float>(buffer * 2 + extra + 2);
    const float breadth = static_cast<float>(kStandardRoomCorridors.width + 2);
    const bool horizontal = normal.x != 0;
    const Vector2 low = horizontal ? Vector2{1.0f + buffer, breadth * 0.5f}
                                   : Vector2{breadth * 0.5f, 1.0f + buffer};
    const Vector2 high = horizontal ? low + Vector2{static_cast<float>(extra), 0}
                                    : low + Vector2{0, static_cast<float>(extra)};
    const bool forward = normal.x + normal.y > 0;
    return {horizontal ? Vector2{length, breadth} : Vector2{breadth, length},
            forward ? low : high, forward ? high : low, traversal.direction, buffer};
}

std::vector<RoomCorridorLayout> CorridorLayout::world_exits(Vector2 origin, Vector2 tile) const
{
    const bool horizontal = corridor_normal(direction).x != 0;
    const float width = static_cast<float>(kStandardRoomCorridors.width);
    const Vector2 size = horizontal ? Vector2{buffer_tiles * 2.0f, width}
                                    : Vector2{width, buffer_tiles * 2.0f};
    const auto to_world = [&](const Rect& rect) {
        return Rect{origin + Vector2{rect.x() * tile.x, rect.y() * tile.y},
                    {rect.width() * tile.x, rect.height() * tile.y}};
    };
    const auto endpoint = [&](RoomDirection outward, Vector2 seam) {
        const auto strip = Rect::from_center(seam, size);
        return RoomCorridorLayout{outward, to_world(strip.expanded({1, 1})), to_world(strip)};
    };
    return {endpoint(opposite_direction(direction), source_seam), endpoint(direction, target_seam)};
}

float CorridorLayout::progress(Vector2 world_center, Vector2 origin, Vector2 tile) const noexcept
{
    const Vector2 from = origin + Vector2{source_seam.x * tile.x, source_seam.y * tile.y};
    const Vector2 delta{(target_seam.x - source_seam.x) * tile.x,
                         (target_seam.y - source_seam.y) * tile.y};
    const float length_squared = delta.x * delta.x + delta.y * delta.y;
    if (length_squared == 0)
        return 0;
    const auto offset = world_center - from;
    return std::clamp((offset.x * delta.x + offset.y * delta.y) / length_squared, 0.0f, 1.0f);
}
}
