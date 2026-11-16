#include "game_flow_manager.h"

#include "engine/tools/logger.h"

#include <algorithm>
#include <utility>

//for testing
namespace
{
constexpr RoomID start_room_id = 0;
constexpr RoomID end_room_id = 13;

std::array<Room, 14> make_fixed_rooms()
{
    // Room IDs are array indices. Positive x points right and positive y points down.
    std::array<Room, 14> rooms{{
        {start_room_id, RoomType::Start, 0, 0},
        {1, RoomType::Combat, 0, -1},
        {2, RoomType::Combat, 1, 0},
        {3, RoomType::Rest, 0, 1},
        {4, RoomType::Combat, -1, 0},
        {5, RoomType::Event, 1, -1},
        {6, RoomType::Elite, 2, -1},
        {7, RoomType::Combat, 2, 0},
        {8, RoomType::Combat, 3, 0},
        {9, RoomType::Event, 0, -2},
        {10, RoomType::Shop, 1, 1},
        {11, RoomType::Rest, -2, 0},
        {12, RoomType::Event, -1, 1},
        {end_room_id, RoomType::End, 3, -1},
    }};

    constexpr std::pair<RoomID, RoomID> connections[] = {
        {0, 1}, {1, 5}, {5, 6}, {6, 13}, // North route
        {0, 2}, {2, 7}, {7, 8}, {8, 13}, // East route
        {2, 5}, {6, 7},                  // Links between routes
        {0, 3}, {3, 12}, {12, 4}, {4, 0}, // Southwest loop
        {1, 9}, {3, 10}, {4, 11},         // Dead ends
    };
    for (const auto& [first, second] : connections)
    {
        rooms[first].neighbors.push_back(second);
        rooms[second].neighbors.push_back(first);
    }
    return rooms;
}
}
//for testing


GameFlowManager::GameFlowManager() : _rooms(make_fixed_rooms())
{
    restart();
}

void GameFlowManager::restart(std::uint64_t run_seed) noexcept
{
    _run_seed = run_seed;

    ELYSIA_LOG_DEBUG("GameFlowManager", "start with seed: " << run_seed);

    for (auto& room : _rooms)
    {
        room.discovered = false;
        room.visited = false;
        room.cleared = false;
    }

    _rooms[end_room_id].discovered = true;
    visit_room(start_room_id);
}

DungeonDepth GameFlowManager::get_current_depth() const noexcept
{
    return 0;
}

RoomID GameFlowManager::get_current_room_id() const noexcept
{
    return _current_room;
}

RoomType GameFlowManager::get_current_room_type() const noexcept
{
    return _rooms[_current_room].type;
}

std::span<const Room> GameFlowManager::rooms() const noexcept
{
    return _rooms;
}

std::vector<Room> GameFlowManager::get_rooms_in_level() const
{
    return std::vector<Room>(_rooms.begin(), _rooms.end());
}

const Room* GameFlowManager::find_room(RoomID room_id) const noexcept
{
    return room_id < _rooms.size() ? &_rooms[room_id] : nullptr;
}

bool GameFlowManager::can_enter_room(RoomID room_id) const noexcept
{
    const auto* target = find_room(room_id);
    if (!target || room_id == _current_room)
        return false;

    // Each first visit extends the connected set of visited rooms by one edge.
    return target->visited || std::ranges::any_of(target->neighbors, [this](RoomID neighbor) {
        return _rooms[neighbor].visited;
    });
}

bool GameFlowManager::try_enter_room(RoomID room_id) noexcept
{
    if (!can_enter_room(room_id))
        return false;

    visit_room(room_id);
    return true;
}

std::uint64_t GameFlowManager::room_seed(RoomID room_id) const noexcept
{
    return _run_seed + room_id;
}

void GameFlowManager::mark_current_room_cleared() noexcept
{
    _rooms[_current_room].cleared = true;
}

void GameFlowManager::visit_room(RoomID room_id) noexcept
{
    _current_room = room_id;
    auto& room = _rooms[room_id];
    room.discovered = true;
    room.visited = true;
    for (const auto neighbor : room.neighbors)
        _rooms[neighbor].discovered = true;
}
