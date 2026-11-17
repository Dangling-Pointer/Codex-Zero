#include "dungeon_graph_types.h"

#include <utility>

namespace
{
struct DirectionStep
{
    RoomDirection direction;
    RoomDirection opposite;
    int dx;
    int dy;
};

constexpr std::array<DirectionStep, 4> directions{{
    {RoomDirection::North, RoomDirection::South, 0, -1},
    {RoomDirection::East, RoomDirection::West, 1, 0},
    {RoomDirection::South, RoomDirection::North, 0, 1},
    {RoomDirection::West, RoomDirection::East, -1, 0},
}};

bool in_bounds(FloorPosition position) noexcept
{
    return position.x >= 0 && position.x < Floor::width
        && position.y >= 0 && position.y < Floor::height;
}

bool has_direction(const Room& room, RoomDirection direction) noexcept
{
    return (std::to_underlying(room.accessible_directions) & std::to_underlying(direction)) != 0;
}

void add_direction(Room& room, RoomDirection direction) noexcept
{
    room.accessible_directions = static_cast<RoomDirection>(
        std::to_underlying(room.accessible_directions) | std::to_underlying(direction));
}
}

Room* Floor::room_at(FloorPosition position) noexcept
{
    if (!in_bounds(position))
        return nullptr;

    auto& cell = rooms[position.y][position.x];
    return cell ? &*cell : nullptr;
}

const Room* Floor::room_at(FloorPosition position) const noexcept
{
    if (!in_bounds(position))
        return nullptr;

    const auto& cell = rooms[position.y][position.x];
    return cell ? &*cell : nullptr;
}

std::optional<FloorPosition> Floor::find_position(RoomID room_id) const noexcept
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto& cell = rooms[y][x];
            if (cell && cell->id == room_id)
                return FloorPosition{x, y};
        }
    }
    return std::nullopt;
}

std::array<std::optional<FloorPosition>, 4> Floor::neighbors(FloorPosition position) const noexcept
{
    std::array<std::optional<FloorPosition>, 4> result{};
    const auto* room = room_at(position);
    if (!room)
        return result;

    for (std::size_t i = 0; i < directions.size(); ++i)
    {
        const auto& step = directions[i];
        if (!has_direction(*room, step.direction))
            continue;

        const FloorPosition next{position.x + step.dx, position.y + step.dy};
        const auto* neighbor = room_at(next);
        if (neighbor && has_direction(*neighbor, step.opposite))
            result[i] = next;
    }
    return result;
}

bool Floor::connect(FloorPosition first, FloorPosition second) noexcept
{
    auto* from = room_at(first);
    auto* to = room_at(second);
    if (!from || !to)
        return false;

    for (const auto& step : directions)
    {
        if (second.x - first.x == step.dx && second.y - first.y == step.dy)
        {
            add_direction(*from, step.direction);
            add_direction(*to, step.opposite);
            return true;
        }
    }
    return false;
}
