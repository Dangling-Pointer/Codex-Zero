#include "camera_controller.h"

#include <algorithm>
#include <cmath>

namespace elysia::camera
{
CameraController::CameraController(Camera& camera) noexcept
    : _camera(camera),
      _logical_center(camera.center()),
      _final_render_center(camera.center())
{
}

void CameraController::set_follow_strategy(
    std::unique_ptr<IFollowStrategy> follow_strategy
) noexcept
{
    _follow_strategy = std::move(follow_strategy);
}

void CameraController::set_focus_rect(
    std::optional<elysia::core::Rect> focus_rect
) noexcept
{
    if (!focus_rect.has_value())
        _has_initialized_focus = false;

    _focus_rect = focus_rect;
}

void CameraController::set_world_bounds(
    std::optional<elysia::core::Rect> world_bounds
) noexcept
{
    _world_bounds = world_bounds;
    _logical_center = clamp_center_to_world_bounds(_logical_center);
    write_final_camera_center(_logical_center);
}

void CameraController::set_viewport_size(
    const elysia::core::Vector2& viewport_size
) noexcept
{
    _camera.set_viewport_size(viewport_size);
    _logical_center = clamp_center_to_world_bounds(_logical_center);
    write_final_camera_center(_logical_center);
}

void CameraController::set_center(const elysia::core::Vector2& center) noexcept
{
    _logical_center = clamp_center_to_world_bounds(center);
    write_final_camera_center(_logical_center);
}

void CameraController::set_zoom(float zoom) noexcept
{
    _zoom_transition.reset();
    _camera.set_zoom(zoom);
    _logical_center = clamp_center_to_world_bounds(_logical_center);
    write_final_camera_center(_logical_center);
}

void CameraController::snap_to_focus() noexcept
{
    if (!_focus_rect.has_value())
    {
        return;
    }

    _logical_center = clamp_center_to_world_bounds(_focus_rect->center());
    write_final_camera_center(_logical_center);
}

void CameraController::start_shake(const CameraShakeParams& params)
{
    _active_effect = std::make_unique<CameraShakeEffect>(params);
}

void CameraController::start_zoom_transition(
    float target_zoom,
    double duration_seconds
) noexcept
{
    const float clamped_target = Camera::clamp_zoom(target_zoom);
    if (!std::isfinite(duration_seconds) || duration_seconds <= 0.0)
    {
        set_zoom(clamped_target);
        return;
    }

    _zoom_transition = ZoomTransition{
        .start_zoom = _camera.zoom(),
        .target_zoom = clamped_target,
        .duration_seconds = duration_seconds,
        .elapsed_seconds = 0.0
    };
}

void CameraController::clear_shake() noexcept
{
    _active_effect.reset();
    write_final_camera_center(_logical_center);
}

void CameraController::clear_effects() noexcept
{
    _active_effect.reset();
    _zoom_transition.reset();
    write_final_camera_center(_logical_center);
}

void CameraController::reset_scene_state() noexcept
{
    _follow_strategy.reset();
    _focus_rect.reset();
    _world_bounds.reset();
    _active_effect.reset();
    _zoom_transition.reset();
    _has_initialized_focus = false;
    _camera.set_zoom(Camera::k_default_zoom);
    _logical_center = elysia::core::Vector2::zero();
    write_final_camera_center(_logical_center);
}

void CameraController::update(double delta_seconds)
{
    update_zoom_transition(delta_seconds);

    if (_focus_rect.has_value())
    {
        if (!_has_initialized_focus)
        {
            snap_to_focus();
            _has_initialized_focus = true;
        }
        else if (_follow_strategy)
        {
            const CameraFollowContext context{
                _logical_center,
                _camera.viewport_size(),
                _camera.zoom()
            };

            _logical_center = _follow_strategy->update_center(
                context,
                *_focus_rect,
                delta_seconds
            );
        }
    }

    _logical_center = clamp_center_to_world_bounds(_logical_center);

    elysia::core::Vector2 resolved_center = _logical_center;
    if (_active_effect)
    {
        resolved_center += _active_effect->update(delta_seconds);

        if (_active_effect->is_finished())
        {
            _active_effect.reset();
        }
    }

    write_final_camera_center(resolved_center);
}

Camera& CameraController::camera() noexcept
{
    return _camera;
}

const Camera& CameraController::camera() const noexcept
{
    return _camera;
}

const IFollowStrategy* CameraController::follow_strategy() const noexcept
{
    return _follow_strategy.get();
}

const std::optional<elysia::core::Rect>& CameraController::focus_rect() const noexcept
{
    return _focus_rect;
}

const std::optional<elysia::core::Rect>& CameraController::world_bounds() const noexcept
{
    return _world_bounds;
}

const elysia::core::Vector2& CameraController::logical_center() const noexcept
{
    return _logical_center;
}

const elysia::core::Vector2& CameraController::final_render_center() const noexcept
{
    return _final_render_center;
}

elysia::core::Vector2 CameraController::clamp_center_to_world_bounds(
    const elysia::core::Vector2& center
) const noexcept
{
    if (!_world_bounds.has_value())
    {
        return center;
    }

    const elysia::core::Rect& bounds = *_world_bounds;
    const elysia::core::Vector2 viewport_size = _camera.world_viewport_size();
    const elysia::core::Vector2 viewport_half = viewport_size * 0.5f;

    elysia::core::Vector2 clamped_center = center;

    if (bounds.width() <= viewport_size.x)
    {
        clamped_center.x = bounds.center().x;
    }
    else
    {
        clamped_center.x = std::clamp(
            clamped_center.x,
            bounds.left() + viewport_half.x,
            bounds.right() - viewport_half.x
        );
    }

    if (bounds.height() <= viewport_size.y)
    {
        clamped_center.y = bounds.center().y;
    }
    else
    {
        clamped_center.y = std::clamp(
            clamped_center.y,
            bounds.top() + viewport_half.y,
            bounds.bottom() - viewport_half.y
        );
    }

    return clamped_center;
}

void CameraController::update_zoom_transition(double delta_seconds) noexcept
{
    if (!_zoom_transition.has_value())
        return;

    ZoomTransition& transition = *_zoom_transition;
    transition.elapsed_seconds += std::max(0.0, delta_seconds);

    const double progress = std::clamp(
        transition.elapsed_seconds / transition.duration_seconds,
        0.0,
        1.0
    );
    const double smooth_progress = progress * progress * (3.0 - 2.0 * progress);
    const float zoom = transition.start_zoom
        + (transition.target_zoom - transition.start_zoom)
            * static_cast<float>(smooth_progress);
    _camera.set_zoom(zoom);

    if (progress >= 1.0)
        _zoom_transition.reset();
}

void CameraController::write_final_camera_center(
    const elysia::core::Vector2& center
) noexcept
{
    _final_render_center = center;
    _camera.set_center(_final_render_center);
}
}
