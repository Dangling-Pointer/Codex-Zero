#include "room_corridor_layout.h"

#include <cmath>
#include <utility>

namespace game
{
using elysia::core::Rect;
using elysia::core::Vector2;

Vector2 corridor_normal(RoomDirection direction) noexcept
{
    switch (direction)
    {
    case RoomDirection::Up:
        return {0, -1};
    case RoomDirection::Right:
        return {1, 0};
    case RoomDirection::Down:
        return {0, 1};
    case RoomDirection::Left:
        return {-1, 0};
    default: return {};
    }
}

RoomDirection opposite_direction(RoomDirection direction) noexcept
{
    switch (direction)
    {
    case RoomDirection::Up: return RoomDirection::Down;
    case RoomDirection::Right: return RoomDirection::Left;
    case RoomDirection::Down: return RoomDirection::Up;
    case RoomDirection::Left: return RoomDirection::Right;
    default: return RoomDirection::None;
    }
}

Rect RoomCorridorLayout::gate_strip(Vector2 tile_size) const noexcept
{
    // The net strip begins at the original room wall, before its corridor half.
    switch (direction)
    {
    case RoomDirection::Up:
        return {walkable.left(), walkable.bottom() - tile_size.y, walkable.width(), tile_size.y};
    case RoomDirection::Right:
        return {walkable.left(), walkable.top(), tile_size.x, walkable.height()};
    case RoomDirection::Down:
        return {walkable.left(), walkable.top(), walkable.width(), tile_size.y};
    case RoomDirection::Left:
        return {walkable.right() - tile_size.x, walkable.top(), tile_size.x, walkable.height()};
    default: return {};
    }
}

bool RoomCorridorLayout::crossed_outward(Vector2 previous, Vector2 current,
                                         const Rect& body_at_current) const noexcept
{
    const auto normal = corridor_normal(direction);
    const auto distance = [&](Vector2 position) {
        const auto offset = position - seam_center();
        return offset.x * normal.x + offset.y * normal.y;
    };
    const float before = distance(previous);
    const float after = distance(current);
    if (normal.is_zero() || before > 0 || after < 0 || after <= before)
        return false;

    const float fraction = -before / (after - before);
    auto body_at_crossing = body_at_current;
    body_at_crossing.set_position(body_at_current.position() + (previous - current) * (1 - fraction));
    // Box2D's contact slop permits a tiny overlap with the side wall.
    constexpr float tolerance = 0.1f;
    if (normal.x == 0)
        return body_at_crossing.left() >= walkable.left() - tolerance
            && body_at_crossing.right() <= walkable.right() + tolerance;
    return body_at_crossing.top() >= walkable.top() - tolerance
        && body_at_crossing.bottom() <= walkable.bottom() + tolerance;
}

std::vector<RoomCorridorLayout> make_room_corridors(const Rect& body, const RoomCorridorConfig& config)
{
    std::vector<RoomCorridorLayout> result;
    const float width = static_cast<float>(config.width);
    const float vertical = static_cast<float>(config.vertical_length);
    const float horizontal = static_cast<float>(config.horizontal_length);
    const float x = std::floor(body.center().x - (width + 2) * 0.5f);
    const float y = std::floor(body.center().y - (width + 2) * 0.5f);
    const auto add = [&](RoomDirection direction, Rect footprint, Rect walkable) {
        if ((std::to_underlying(config.directions) & std::to_underlying(direction)) != 0)
            result.push_back({direction, footprint, walkable});
    };

    // The footprint overlaps the body's wall. The net strip excludes its outer cap.
    add(RoomDirection::Up, {x, body.y() - vertical, width + 2, vertical + 1},
        {x + 1, body.y() - vertical + 1, width, vertical});
    add(RoomDirection::Right, {body.right() - 1, y, horizontal + 1, width + 2},
        {body.right() - 1, y + 1, horizontal, width});
    add(RoomDirection::Down, {x, body.bottom() - 1, width + 2, vertical + 1},
        {x + 1, body.bottom() - 1, width, vertical});
    add(RoomDirection::Left, {body.x() - horizontal, y, horizontal + 1, width + 2},
        {body.x() - horizontal + 1, y + 1, horizontal, width});
    return result;
}
}
