#include "room_factory.h"

#include <utility>

Room RoomFactory::create(RoomID id, RoomType type, RoomDirection direction,
                         std::optional<std::string> content_id) const
{
    return Room{
        .id = id,
        .type = type,
        .accessible_directions = direction,
        .content_id = std::move(content_id),
    };
}

Room RoomFactory::create_start(RoomID id) const
{
    return create(id, RoomType::Start, RoomDirection::All);
}

Room RoomFactory::create_end(RoomID id) const
{
    return create(id, RoomType::End, RoomDirection::None);
}

Room RoomFactory::create_test_room(RoomID id) const
{
    return create(id, RoomType::Combat, RoomDirection::All);
}
