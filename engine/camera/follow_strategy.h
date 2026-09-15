#pragma once

#include "../core/geometry/rect.h"
#include "../core/geometry/vector2.h"

namespace elysia::camera
{
struct CameraFollowContext
{
    elysia::core::Vector2 current_center{};
    elysia::core::Vector2 viewport_size{};
    float zoom = 1.0f;
};

class IFollowStrategy
{
public:
    virtual ~IFollowStrategy() = default;

    [[nodiscard]] virtual elysia::core::Vector2 update_center(const CameraFollowContext& context,
        const elysia::core::Rect& focus_rect,double delta_seconds) const = 0;
};

class HardFollowStrategy final : public IFollowStrategy
{
public:
    [[nodiscard]] elysia::core::Vector2 update_center(const CameraFollowContext& context,
        const elysia::core::Rect& focus_rect,double delta_seconds) const override;
};

class DeadZoneFollowStrategy final : public IFollowStrategy
{
public:
    explicit DeadZoneFollowStrategy(const elysia::core::Rect& dead_zone_rect) noexcept;

    void set_dead_zone_rect(const elysia::core::Rect& dead_zone_rect) noexcept;
    [[nodiscard]] const elysia::core::Rect& dead_zone_rect() const noexcept;

    [[nodiscard]] elysia::core::Vector2 update_center(const CameraFollowContext& context,
        const elysia::core::Rect& focus_rect,double delta_seconds) const override;

private:
    elysia::core::Rect _dead_zone_rect{};
};

class SmoothFollowStrategy final : public IFollowStrategy
{
public:
    explicit SmoothFollowStrategy(double follow_speed_units_per_second = 0.0) noexcept;

    void set_follow_speed_units_per_second(double follow_speed_units_per_second) noexcept;
    [[nodiscard]] double follow_speed_units_per_second() const noexcept;

    [[nodiscard]] elysia::core::Vector2 update_center(const CameraFollowContext& context,
        const elysia::core::Rect& focus_rect,double delta_seconds) const override;

private:
    double _follow_speed_units_per_second = 0.0;
};
}
