#pragma once

#include "engine/tools/singleton.h"

namespace game
{
class RoomQuery;

// The active scene owns the query. Do not retain current() across scene switches.
class RoomService final : public elysia::tools::Singleton<RoomService>
{
    friend class elysia::tools::Singleton<RoomService>;
public:
    [[nodiscard]] const RoomQuery* current() const noexcept { return _current; }
    [[nodiscard]] bool bind(const RoomQuery& query) noexcept;
    [[nodiscard]] bool unbind(const RoomQuery& query) noexcept;

private:
    RoomService() = default;
    const RoomQuery* _current = nullptr;
};
}
