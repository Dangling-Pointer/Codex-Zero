#include "room_manager.h"
#include "game/map/dungeon_room.h"
#include "room_service.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace game
{
namespace
{
using elysia::core::Rect;
using elysia::core::Vector2;

bool finite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool terrain_is_walkable(const elysia::physics::ITileCollisionWorld& map, const Rect& bounds)
{
    const auto tile = map.tile_size();
    const auto origin = map.world_origin();
    if (!finite(tile) || tile.x <= 0 || tile.y <= 0 || !finite(origin)
        || !finite(bounds.position()) || !finite({bounds.right(), bounds.bottom()})
        || bounds.width() <= 0 || bounds.height() <= 0)
        return false;

    const Rect map_bounds{origin, {map.columns() * tile.x, map.rows() * tile.y}};
    if (!map_bounds.contains(bounds))
        return false;

    const int left = static_cast<int>(std::floor((bounds.left() - origin.x) / tile.x));
    const int top = static_cast<int>(std::floor((bounds.top() - origin.y) / tile.y));
    const int right = static_cast<int>(std::ceil((bounds.right() - origin.x) / tile.x));
    const int bottom = static_cast<int>(std::ceil((bounds.bottom() - origin.y) / tile.y));
    const auto* dungeon = dynamic_cast<const DungeonRoom*>(&map);
    for (int y = top; y < bottom; ++y)
        for (int x = left; x < right; ++x)
        {
            if (map.cell_at({x, y}).type != elysia::physics::TileCollisionType::Empty)
                return false;
            // Void is physically empty too, but cannot host room objects.
            if (dungeon && dungeon->tile_map().get(x, y).type != TileType::Floor)
                return false;
        }
    return true;
}

bool covered_by_corridors(const Rect& bounds, std::span<const RoomCorridorLayout> corridors)
{
    // Subtract each passage from the remaining area. This checks the whole object,
    // including layouts that overlap or meet inside a standalone test map.
    std::vector<Rect> remaining{bounds};
    for (const auto& corridor : corridors)
    {
        std::vector<Rect> next;
        for (const auto& part : remaining)
        {
            if (!part.intersects(corridor.walkable))
            {
                next.push_back(part);
                continue;
            }
            const auto cut = part.intersection(corridor.walkable);
            if (cut.top() > part.top())
                next.emplace_back(part.left(), part.top(), part.width(), cut.top() - part.top());
            if (cut.bottom() < part.bottom())
                next.emplace_back(part.left(), cut.bottom(), part.width(), part.bottom() - cut.bottom());
            if (cut.left() > part.left())
                next.emplace_back(part.left(), cut.top(), cut.left() - part.left(), cut.height());
            if (cut.right() < part.right())
                next.emplace_back(cut.right(), cut.top(), part.right() - cut.right(), cut.height());
        }
        remaining = std::move(next);
        if (remaining.empty())
            return true;
    }
    return false;
}
}

RoomManager::~RoomManager() { clear(); }

void RoomManager::bind_map(const DungeonRoom& map, std::span<const RoomCorridorLayout> corridors,
                           bool corridor_only) noexcept
{
    clear();
    _map = &map;
    _corridors = corridors;
    _corridor_only = corridor_only;
}

bool RoomManager::activate() noexcept
{
    return _map && !_map->is_destroyed() && RoomService::instance()->bind(*this);
}

void RoomManager::clear() noexcept
{
    (void)RoomService::instance()->unbind(*this);
    _map = nullptr;
    _corridors = {};
    _corridor_only = false;
}

std::optional<elysia::core::Rect> RoomManager::map_bounds() const noexcept
{
    if (!_map || _map->is_destroyed())
        return std::nullopt;
    const auto tile = _map->tile_size();
    return Rect{_map->world_origin(), {_map->columns() * tile.x, _map->rows() * tile.y}};
}
std::optional<TileType> RoomManager::tile_at(Vector2 position) const
{
    const auto bounds = map_bounds();
    if (!finite(position) || !bounds || !bounds->contains_half_open(position))
        return std::nullopt;
    const auto local = position - _map->world_origin();
    const auto tile = _map->tile_size();
    return _map->tile_map().get(static_cast<int>(std::floor(local.x / tile.x)),
                               static_cast<int>(std::floor(local.y / tile.y))).type;
}

RoomLocation RoomManager::region_at(Vector2 position) const
{
    if (tile_at(position) != TileType::Floor)
        return {};
    for (const auto& corridor : _corridors)
        if (corridor.walkable.contains_half_open(position))
            return {RoomRegion::Corridor, corridor.direction};
    return {_corridor_only ? RoomRegion::Corridor : RoomRegion::Room};
}

const RoomCorridorLayout* RoomManager::corridor(RoomDirection direction) const noexcept
{
    if (!map_bounds())
        return nullptr;
    const auto found = std::ranges::find(_corridors, direction, &RoomCorridorLayout::direction);
    return found == _corridors.end() ? nullptr : &*found;
}

bool RoomManager::is_walkable(const Rect& bounds) const
{
    return map_bounds().has_value() && terrain_is_walkable(*_map, bounds);
}

bool RoomManager::can_place(const Rect& bounds, const PlacementOptions& options) const
{
    if (!is_walkable(bounds))
        return false;

    switch (options.region)
    {
    case SpawnRegion::Any: break;
    case SpawnRegion::Room:
        if (_corridor_only || std::ranges::any_of(_corridors, [&](const auto& corridor) {
                return bounds.intersects(corridor.walkable);
            }))
            return false;
        break;
    case SpawnRegion::Corridor:
        if (!_corridor_only && !covered_by_corridors(bounds, _corridors))
            return false;
        break;
    default: return false;
    }

    return !std::ranges::any_of(options.occupied, [&](const auto& other) {
        return bounds.intersects(other);
    });
}

std::optional<Vector2> RoomManager::find_spawn(Vector2 size, Vector2 preferred_center,
                                            const PlacementOptions& options) const
{
    if (!map_bounds() || !finite(size) || size.x <= 0 || size.y <= 0 || !finite(preferred_center))
        return std::nullopt;

    double nearest = std::numeric_limits<double>::infinity();
    std::optional<Vector2> result;
    const auto tile = _map->tile_size();
    for (int y = 0; y < _map->rows(); ++y)
        for (int x = 0; x < _map->columns(); ++x)
        {
            const auto at = _map->world_origin() + Vector2{x * tile.x, y * tile.y};
            const Rect bounds{at, size};
            if (!can_place(bounds, options))
                continue;
            const auto center = bounds.center();
            const double dx = static_cast<double>(center.x) - preferred_center.x;
            const double dy = static_cast<double>(center.y) - preferred_center.y;
            const double distance = dx * dx + dy * dy;
            if (distance < nearest)
            {
                nearest = distance;
                result = at;
            }
        }
    return result;
}

}
