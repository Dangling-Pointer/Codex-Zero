#include "floor_generator.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

namespace
{
constexpr std::array<FloorPosition, 4> direction_steps{{
    {0, -1}, {1, 0}, {0, 1}, {-1, 0},
}};

bool in_bounds(FloorPosition position)
{
    return position.x >= 0 && position.x < Floor::width
        && position.y >= 0 && position.y < Floor::height;
}

// Visibility ignores direction bits: these are the possible edges before building the tree.
std::vector<FloorPosition> visible_rooms(const Floor& floor, FloorPosition position)
{
    std::vector<FloorPosition> result;
    for (const auto step : direction_steps)
    {
        FloorPosition next{position.x + step.x, position.y + step.y};
        while (in_bounds(next))
        {
            if (floor.room_at(next))
            {
                result.push_back(next);
                break;
            }
            next.x += step.x;
            next.y += step.y;
        }
    }
    return result;
}

std::vector<FloorPosition> fill_candidates(const Floor& floor)
{
    std::vector<FloorPosition> candidates;
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            const FloorPosition position{x, y};
            if (floor.room_at(position))
                continue;

            const auto visible = visible_rooms(floor, position);
            const bool can_attach = std::ranges::any_of(visible, [&](FloorPosition neighbor) {
                return floor.room_at(neighbor)->id != floor.end_room_id;
            });
            if (can_attach)
                candidates.push_back(position);
        }
    }
    return candidates;
}

void connect_rooms(Floor& floor, FloorPosition from, FloorPosition to)
{
    if (!floor.connect(from, to))
        throw std::logic_error("Generated tree contains an invalid connection");
}

FloorPosition perimeter_position(int index)
{
    // Walk around the perimeter, including each corner exactly once.
    static_assert(Floor::width == 6 && Floor::height == 6);
    if (index < 5)
        return {index, 0};
    if (index < 10)
        return {5, index - 5};
    if (index < 15)
        return {15 - index, 5};

    return {0, 20 - index};
}
}

Floor FloorGenerator::generate(std::uint64_t seed, RoomID first_room_id,
                               const FloorGenerationConfig& config)
{
    constexpr int capacity = Floor::width * Floor::height;

    if (config.min_room_number < 6 || config.min_room_number > config.max_room_number|| config.max_room_number > capacity)
        throw std::invalid_argument("Floor room count must satisfy 6 <= minimum <= maximum <= capacity");

    _random.set_seed(seed);

    const int room_number = _random.int_inclusive(config.min_room_number, config.max_room_number);
    const auto last_room_id = static_cast<std::uint64_t>(first_room_id) + room_number - 1;

    if (last_room_id > std::numeric_limits<RoomID>::max())
        throw std::overflow_error("Not enough room IDs for the generated floor");

    Floor floor{
        .start_room_id = first_room_id,
        .end_room_id = first_room_id + 1,
        .current_room_id = first_room_id,
        .seed = seed,
    };

    const FloorPosition start = get_start_pos();
    floor.rooms[start.y][start.x] = _room_factory.create_start(floor.start_room_id);
    floor.room_at(start)->accessible_directions = RoomDirection::None;

    const FloorPosition end = get_end_pos(start, room_number);
    floor.rooms[end.y][end.x] = _room_factory.create_end(floor.end_room_id);

    fill_rooms(floor, room_number);
    generate_connections(floor, room_number);

    return floor;
}

void FloorGenerator::fill_rooms(Floor& floor, int room_number)
{
    const auto start = *floor.find_position(floor.start_room_id);
    const auto end = *floor.find_position(floor.end_room_id);

    std::uint64_t next_id = static_cast<std::uint64_t>(floor.start_room_id) + 2;
    int placed = 2;
    const auto place_combat_room = [&](FloorPosition position) {
        if (floor.room_at(position))
            throw std::logic_error("Generated room would overwrite an occupied position");

        floor.rooms[position.y][position.x] = _room_factory.create(
            static_cast<RoomID>(next_id++), RoomType::Combat, RoomDirection::None);
        ++placed;
    };

    for (const auto step : direction_steps)
        place_combat_room({start.x + step.x, start.y + step.y});

    // The exit may already see one of the cross's rooms, so only add a bend when needed.
    if (visible_rooms(floor, end).empty())
    {
        if (placed == room_number)
            throw std::logic_error("Generated floor has no space for the exit's connecting room");

        const std::array<FloorPosition, 2> corners{{{start.x, end.y}, {end.x, start.y}}};
        place_combat_room(_random.pick(corners));
    }

    while (placed < room_number)
    {
        const auto candidates = fill_candidates(floor);
        if (candidates.empty())
            throw std::logic_error("Cannot extend the generated floor to its target room count");
        place_combat_room(_random.pick(candidates));
    }
}

void FloorGenerator::generate_connections(Floor& floor, int room_number)
{
    struct Connection
    {
        FloorPosition from;
        FloorPosition to;
    };

    std::array<std::array<bool, Floor::width>, Floor::height> in_tree{};
    const auto start = *floor.find_position(floor.start_room_id);
    in_tree[start.y][start.x] = true;
    int connected = 1;

    // Seed Prim with the entire cross so none of the four starting edges can be dropped.
    for (const auto step : direction_steps)
    {
        const FloorPosition neighbor{start.x + step.x, start.y + step.y};
        connect_rooms(floor, start, neighbor);
        in_tree[neighbor.y][neighbor.x] = true;
        ++connected;
    }

    // Random Prim growth connects every non-exit room without introducing cycles.
    while (connected < room_number - 1)
    {
        std::vector<Connection> frontier;
        for (int y = 0; y < Floor::height; ++y)
        {
            for (int x = 0; x < Floor::width; ++x)
            {
                if (!in_tree[y][x])
                    continue;

                for (const auto neighbor : visible_rooms(floor, {x, y}))
                {
                    if (!in_tree[neighbor.y][neighbor.x]
                        && floor.room_at(neighbor)->id != floor.end_room_id)
                        frontier.push_back({{x, y}, neighbor});
                }
            }
        }
        if (frontier.empty())
            throw std::logic_error("Generated floor has disconnected room positions");

        const auto edge = _random.pick(frontier);
        connect_rooms(floor, edge.from, edge.to);
        in_tree[edge.to.y][edge.to.x] = true;
        ++connected;
    }

    // The perimeter exit stays a leaf and cannot become a bridge between branches.
    const auto end = *floor.find_position(floor.end_room_id);
    const auto entrances = visible_rooms(floor, end);
    if (entrances.empty())
        throw std::logic_error("Generated exit has no entrance");
    connect_rooms(floor, end, _random.pick(entrances));
}

FloorPosition FloorGenerator::get_start_pos()
{
    return {
        _random.int_inclusive(2, 3),
        _random.int_inclusive(2, 3),
    };
}

FloorPosition FloorGenerator::get_end_pos(FloorPosition start, int room_number)
{
    std::vector<FloorPosition> candidates;
    for (int index = 0; index < 20; ++index)
    {
        const auto position = perimeter_position(index);
        if (room_number == 6)
        {
            // Six rooms leave no budget for a bend: the exit must see the starting cross.
            const bool shares_column = position.x >= start.x - 1 && position.x <= start.x + 1;
            const bool shares_row = position.y >= start.y - 1 && position.y <= start.y + 1;
            if (!shares_column && !shares_row)
                continue;
        }
        candidates.push_back(position);
    }
    return _random.pick(candidates);
}
