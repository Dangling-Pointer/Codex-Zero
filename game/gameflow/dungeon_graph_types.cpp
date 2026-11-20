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
    {RoomDirection::Up, RoomDirection::Down, 0, -1},
    {RoomDirection::Right, RoomDirection::Left, 1, 0},
    {RoomDirection::Down, RoomDirection::Up, 0, 1},
    {RoomDirection::Left, RoomDirection::Right, -1, 0},
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

std::optional<FloorPosition> next_room(
    const Floor& floor, FloorPosition position, const DirectionStep& step) noexcept
{
    position.x += step.dx;
    position.y += step.dy;
    while (in_bounds(position))
    {
        if (floor.room_at(position))
            return position;

        position.x += step.dx;
        position.y += step.dy;
    }
    return std::nullopt;
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

        const auto next = next_room(*this, position, step);
        if (next && has_direction(*room_at(*next), step.opposite))
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
        if (next_room(*this, first, step) == second)
        {
            add_direction(*from, step.direction);
            add_direction(*to, step.opposite);
            return true;
        }
    }
    return false;
}
