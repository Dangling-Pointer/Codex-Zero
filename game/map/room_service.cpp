#include "room_service.h"

namespace game
{
bool RoomService::bind(const RoomQuery& query) noexcept
{
    if (_current && _current != &query)
        return false;
    _current = &query;
    return true;
}

bool RoomService::unbind(const RoomQuery& query) noexcept
{
    if (_current != &query)
        return false;
    _current = nullptr;
    return true;
}
}
