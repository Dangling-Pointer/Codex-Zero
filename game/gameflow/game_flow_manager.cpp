#include "game_flow_manager.h"

#include "engine/tools/logger.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace
{
constexpr RoomID start_room_id = 0;
constexpr RoomID end_room_id = 13;

struct FixedRoom
{
    RoomID id;
    RoomType type;
    FloorPosition position;
};

Floor make_fixed_floor()
{
    // Grid x increases to the right, and y increases downwards.
    constexpr FixedRoom rooms[] = {
        {start_room_id, RoomType::Start, {2, 2}},
        {1, RoomType::Combat, {2, 1}},
        {2, RoomType::Combat, {3, 2}},
        {3, RoomType::Rest, {2, 3}},
        {4, RoomType::Combat, {1, 2}},
        {5, RoomType::Event, {3, 1}},
        {6, RoomType::Elite, {4, 1}},
        {7, RoomType::Combat, {4, 2}},
        {8, RoomType::Combat, {5, 2}},
        {9, RoomType::Event, {2, 0}},
        {10, RoomType::Shop, {3, 3}},
        {11, RoomType::Rest, {0, 2}},
        {12, RoomType::Event, {1, 3}},
        {end_room_id, RoomType::End, {5, 1}},

        // Fill the remaining cells to exercise the full map layout.
        {14, RoomType::Rest, {0, 0}},
        {15, RoomType::Combat, {1, 0}},
        {16, RoomType::Shop, {3, 0}},
        {17, RoomType::Combat, {4, 0}},
        {18, RoomType::Event, {5, 0}},
        {19, RoomType::Combat, {0, 1}},
        {20, RoomType::Elite, {1, 1}},
        {21, RoomType::Event, {0, 3}},
        {22, RoomType::Elite, {4, 3}},
        {23, RoomType::Rest, {5, 3}},
        {24, RoomType::Combat, {0, 4}},
        {25, RoomType::Rest, {1, 4}},
        {26, RoomType::Combat, {2, 4}},
        {27, RoomType::Shop, {3, 4}},
        {28, RoomType::Combat, {4, 4}},
        {29, RoomType::Event, {5, 4}},
        {30, RoomType::Shop, {0, 5}},
        {31, RoomType::Combat, {1, 5}},
        {32, RoomType::Event, {2, 5}},
        {33, RoomType::Elite, {3, 5}},
        {34, RoomType::Rest, {4, 5}},
        {35, RoomType::Combat, {5, 5}},
    };

    constexpr std::pair<RoomID, RoomID> connections[] = {
        {0, 1}, {1, 5}, {5, 6}, {6, 13}, // North route
        {0, 2}, {2, 7}, {7, 8}, {8, 13}, // East route
        {2, 5}, {6, 7},                  // Links between routes
        {0, 3}, {3, 12}, {12, 4}, {4, 0}, // Southwest loop
        {1, 9}, {3, 10}, {4, 11},         // Side branches

        {9, 15}, {15, 14}, {14, 19}, {19, 20},
        {9, 16}, {16, 17}, {17, 18},
        {11, 21}, {21, 24}, {24, 30}, {30, 31}, {31, 25},
        {25, 26}, {26, 32}, {32, 33}, {33, 27},
        {10, 22}, {22, 23}, {23, 29}, {29, 35}, {35, 34}, {34, 28},
    };

    Floor floor{
        .start_room_id = start_room_id,
        .end_room_id = end_room_id,
        .current_room_id = start_room_id,
    };
    for (const auto& room : rooms)
        floor.rooms[room.position.y][room.position.x].emplace(Room{.id = room.id, .type = room.type});

    for (const auto& [first, second] : connections)
    {
        const auto from = floor.find_position(first);
        const auto to = floor.find_position(second);
        if (!from || !to || !floor.connect(*from, *to))
            throw std::logic_error("Invalid connection in the fixed floor");
    }
    return floor;
}
}

GameFlowManager::GameFlowManager() : _floor(make_fixed_floor())
{
    restart();
}

void GameFlowManager::restart(std::uint64_t run_seed) noexcept
{
    _floor.seed = run_seed;
    ELYSIA_LOG_DEBUG("GameFlowManager", "start with seed: " << run_seed);

    for (auto& row : _floor.rooms)
    {
        for (auto& room : row)
        {
            if (!room)
                continue;

            room->discovered = false;
            room->visited = false;
            room->cleared = false;
            room->event_triggered = false;
        }
    }

    _floor.room_at(*_floor.find_position(_floor.end_room_id))->discovered = true;
    visit_room(*_floor.find_position(_floor.start_room_id));
}

DungeonDepth GameFlowManager::get_current_depth() const noexcept
{
    return 0;
}

RoomID GameFlowManager::get_current_room_id() const noexcept
{
    return _floor.current_room_id;
}

RoomType GameFlowManager::get_current_room_type() const noexcept
{
    return find_room(_floor.current_room_id)->type;
}

const Floor& GameFlowManager::current_floor() const noexcept
{
    return _floor;
}

const Room* GameFlowManager::find_room(RoomID room_id) const noexcept
{
    const auto position = _floor.find_position(room_id);
    return position ? _floor.room_at(*position) : nullptr;
}

bool GameFlowManager::can_enter_room(RoomID room_id) const noexcept
{
    if (room_id == _floor.current_room_id)
        return false;

    const auto position = _floor.find_position(room_id);
    if (!position)
        return false;

    const auto& target = *_floor.room_at(*position);
    if (target.visited)
        return true;

    // Each first visit extends the connected set of visited rooms by one edge.
    return std::ranges::any_of(_floor.neighbors(*position), [this](const auto& neighbor) {
        return neighbor && _floor.room_at(*neighbor)->visited;
    });
}

bool GameFlowManager::try_enter_room(RoomID room_id) noexcept
{
    if (!can_enter_room(room_id))
        return false;

    visit_room(*_floor.find_position(room_id));
    return true;
}

std::uint64_t GameFlowManager::room_seed(RoomID room_id) const noexcept
{
    return _floor.seed + room_id;
}

void GameFlowManager::mark_current_room_cleared() noexcept
{
    _floor.room_at(*_floor.find_position(_floor.current_room_id))->cleared = true;
}

void GameFlowManager::visit_room(FloorPosition position) noexcept
{
    auto& room = *_floor.room_at(position);
    _floor.current_room_id = room.id;
    room.discovered = true;
    room.visited = true;

    for (const auto& neighbor : _floor.neighbors(position))
    {
        if (neighbor)
            _floor.room_at(*neighbor)->discovered = true;
    }
}
