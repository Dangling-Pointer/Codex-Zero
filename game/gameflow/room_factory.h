#pragma once

#include "dungeon_graph_types.h"

class RoomFactory
{
public:
    [[nodiscard]] Room create(RoomID id, RoomType type, RoomDirection direction,
                              std::optional<std::string> content_id = std::nullopt) const;

    [[nodiscard]] Room create_start(RoomID id) const;
    [[nodiscard]] Room create_end(RoomID id) const;
    [[nodiscard]] Room create_test_room(RoomID id) const;
};
