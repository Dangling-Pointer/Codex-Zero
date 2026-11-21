#pragma once

#include "dungeon_graph_types.h"
#include "engine/tools/singleton.h"
#include "floor_generator.h"

class GameFlowManager : public elysia::tools::Singleton<GameFlowManager>
{
public:
    GameFlowManager();
    void restart(std::uint64_t run_seed = 0);

    [[nodiscard]] DungeonDepth get_current_depth() const noexcept;
    [[nodiscard]] RoomID get_current_room_id() const noexcept;
    [[nodiscard]] RoomType get_current_room_type() const noexcept;

    [[nodiscard]] const Floor& current_floor() const noexcept;
    [[nodiscard]] const Room* find_room(RoomID room_id) const noexcept;
    [[nodiscard]] const Room* connected_room(RoomID from, RoomDirection direction) const noexcept;
    [[nodiscard]] bool can_enter_room(RoomID room_id) const noexcept;
    [[nodiscard]] bool try_enter_room(RoomID room_id) noexcept;
    [[nodiscard]] std::uint64_t room_seed(RoomID room_id) const noexcept;
    void mark_current_room_cleared() noexcept;

private:
    void visit_room(FloorPosition position) noexcept;

    FloorGenerator _generator;
    Floor _floor;
    RoomID _next_room_id = 0;
};
