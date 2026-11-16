#pragma once

#include "dungeon_graph_types.h"
#include "engine/tools/singleton.h"

#include <array>
#include <span>

class GameFlowManager : public elysia::tools::Singleton<GameFlowManager>
{
public:
    GameFlowManager();
    void restart() noexcept;

    [[nodiscard]] DungeonDepth get_current_depth() const noexcept;
    [[nodiscard]] RoomID get_current_room_id() const noexcept;
    [[nodiscard]] RoomType get_current_room_type() const noexcept;

    [[nodiscard]] std::span<const Room> rooms() const noexcept;
    [[nodiscard]] std::vector<Room> get_rooms_in_level() const;
    [[nodiscard]] const Room* find_room(RoomID room_id) const noexcept;
    [[nodiscard]] bool try_enter_room(RoomID room_id) noexcept;

private:
    void visit_room(RoomID room_id) noexcept;

    std::array<Room, 14> _rooms;
    RoomID _current_room = 0;
};
