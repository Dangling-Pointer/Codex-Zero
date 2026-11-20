#include "game_flow_manager.h"

#include "engine/tools/logger.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

GameFlowManager::GameFlowManager()
{
    restart();
}

void GameFlowManager::restart(std::uint64_t run_seed)
{
    RoomID next_room_id = 0;
    auto floor = _generator.generate(run_seed, next_room_id, {13,15});
    const auto start_position = floor.find_position(floor.start_room_id);
    const auto end_position = floor.find_position(floor.end_room_id);

    if (!start_position || !end_position)
        throw std::logic_error("Generated floor is missing its start or exit");

    auto& start = *floor.room_at(*start_position);
    auto& end = *floor.room_at(*end_position);
        
    if (start.type != RoomType::Start || end.type != RoomType::End)
        throw std::logic_error("Generated floor has an invalid start or exit");

    floor.current_room_id = start.id;
    start.discovered = true;
    start.visited = true;
    end.discovered = true;
    for (const auto& neighbor : floor.neighbors(*start_position))
    {
        if (neighbor)
            floor.room_at(*neighbor)->discovered = true;
    }

    next_room_id = floor.end_room_id + 1;
    _floor = std::move(floor);
    _next_room_id = next_room_id;
    ELYSIA_LOG_DEBUG("GameFlowManager", "start with seed: " << run_seed);
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
