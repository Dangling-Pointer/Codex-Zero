#include "follow_strategy.h"
#include "camera.h"

#include <algorithm>

namespace elysia::camera
{
elysia::core::Vector2 HardFollowStrategy::update_center(
    const CameraFollowContext& context,
    const elysia::core::Rect& focus_rect,
    double delta_seconds
) const
{
    (void)context;
    (void)delta_seconds;
    return focus_rect.center();
}

DeadZoneFollowStrategy::DeadZoneFollowStrategy(
    const elysia::core::Rect& dead_zone_rect
) noexcept
    : _dead_zone_rect(dead_zone_rect)
{
}

void DeadZoneFollowStrategy::set_dead_zone_rect(
    const elysia::core::Rect& dead_zone_rect
) noexcept
{
    _dead_zone_rect = dead_zone_rect;
}

const elysia::core::Rect& DeadZoneFollowStrategy::dead_zone_rect() const noexcept
{
    return _dead_zone_rect;
}

elysia::core::Vector2 DeadZoneFollowStrategy::update_center(
    const CameraFollowContext& context,
    const elysia::core::Rect& focus_rect,
    double delta_seconds
) const
{
    (void)delta_seconds;

    const float zoom = Camera::clamp_zoom(context.zoom);
    if (_dead_zone_rect.is_empty()
        || focus_rect.width() * zoom > _dead_zone_rect.width()
        || focus_rect.height() * zoom > _dead_zone_rect.height())
    {
        return focus_rect.center();
    }

    elysia::core::Vector2 updated_center = context.current_center;
    const elysia::core::Rect view_rect = elysia::core::Rect::from_center(
        context.current_center,
        context.viewport_size / zoom
    );

    const elysia::core::Rect focus_local_rect(
        (focus_rect.x() - view_rect.x()) * zoom,
        (focus_rect.y() - view_rect.y()) * zoom,
        focus_rect.width() * zoom,
        focus_rect.height() * zoom
    );

    if (focus_local_rect.left() < _dead_zone_rect.left())
    {
        updated_center.x +=
            (focus_local_rect.left() - _dead_zone_rect.left()) / zoom;
    }
    else if (focus_local_rect.right() > _dead_zone_rect.right())
    {
        updated_center.x +=
            (focus_local_rect.right() - _dead_zone_rect.right()) / zoom;
    }

    if (focus_local_rect.top() < _dead_zone_rect.top())
    {
        updated_center.y +=
            (focus_local_rect.top() - _dead_zone_rect.top()) / zoom;
    }
    else if (focus_local_rect.bottom() > _dead_zone_rect.bottom())
    {
        updated_center.y +=
            (focus_local_rect.bottom() - _dead_zone_rect.bottom()) / zoom;
    }

    return updated_center;
}

SmoothFollowStrategy::SmoothFollowStrategy(
    double follow_speed_units_per_second
) noexcept
    : _follow_speed_units_per_second(std::max(0.0, follow_speed_units_per_second))
{
}

void SmoothFollowStrategy::set_follow_speed_units_per_second(
    double follow_speed_units_per_second
) noexcept
{
    _follow_speed_units_per_second = std::max(0.0, follow_speed_units_per_second);
}

double SmoothFollowStrategy::follow_speed_units_per_second() const noexcept
{
    return _follow_speed_units_per_second;
}

elysia::core::Vector2 SmoothFollowStrategy::update_center(
    const CameraFollowContext& context,
    const elysia::core::Rect& focus_rect,
    double delta_seconds
) const
{
    const elysia::core::Vector2 current_center = context.current_center;
    const elysia::core::Vector2 target_center = focus_rect.center();

    if (delta_seconds <= 0.0 || _follow_speed_units_per_second <= 0.0)
    {
        return current_center;
    }

    const elysia::core::Vector2 delta = target_center - current_center;
    const float distance = delta.length();
    const float max_step = static_cast<float>(_follow_speed_units_per_second * delta_seconds);

    if (distance <= max_step || distance <= elysia::core::Vector2::k_epsilon)
    {
        return target_center;
    }

    return current_center + delta.normalized() * max_step;
}
}
