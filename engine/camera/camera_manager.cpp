#include "camera_manager.h"

#include <algorithm>
#include <cassert>
#include <type_traits>
#include <utility>

namespace elysia::camera
{
CameraManager::CameraRig::CameraRig() noexcept
    : controller(camera)
{
}

const Camera& CameraManager::camera(CameraSlot slot) const noexcept
{
    return rig(slot).camera;
}

void CameraManager::set_center(
    CameraSlot slot,
    const elysia::core::Vector2& center
) noexcept
{
    rig(slot).controller.set_center(center);
}

void CameraManager::set_viewport_size(
    CameraSlot slot,
    const elysia::core::Vector2& viewport_size
) noexcept
{
    rig(slot).controller.set_viewport_size(viewport_size);
}

void CameraManager::set_zoom(CameraSlot slot, float zoom) noexcept
{
    std::erase_if(_requests, [slot](const CameraRequest& request)
    {
        return request.slot == slot
            && std::holds_alternative<ZoomToRequest>(request.payload);
    });

    rig(slot).controller.set_zoom(zoom);
}

void CameraManager::set_focus_rect(
    CameraSlot slot,
    std::optional<elysia::core::Rect> focus_rect
) noexcept
{
    rig(slot).controller.set_focus_rect(focus_rect);
}

void CameraManager::set_world_bounds(
    CameraSlot slot,
    std::optional<elysia::core::Rect> world_bounds
) noexcept
{
    rig(slot).controller.set_world_bounds(world_bounds);
}

void CameraManager::set_follow_strategy(
    CameraSlot slot,
    std::unique_ptr<IFollowStrategy> follow_strategy
) noexcept
{
    rig(slot).controller.set_follow_strategy(std::move(follow_strategy));
}

void CameraManager::request_shake(
    CameraSlot slot,
    const CameraShakeParams& params
)
{
    _requests.push_back(CameraRequest{ slot, ShakeRequest{ params } });
}

void CameraManager::request_zoom_to(
    CameraSlot slot,
    float target_zoom,
    double duration_seconds
)
{
    _requests.push_back(CameraRequest{
        slot,
        ZoomToRequest{ target_zoom, duration_seconds }
    });
}

void CameraManager::request_snap_to_focus(CameraSlot slot)
{
    _requests.push_back(CameraRequest{ slot, SnapToFocusRequest{} });
}

void CameraManager::request_clear_effects(CameraSlot slot)
{
    _requests.push_back(CameraRequest{ slot, ClearEffectsRequest{} });
}

void CameraManager::update(double delta_seconds)
{
    process_requests();

    for (CameraRig& camera_rig : _rigs)
    {
        camera_rig.controller.update(delta_seconds);
    }
}

void CameraManager::reset(CameraSlot slot) noexcept
{
    std::erase_if(_requests, [slot](const CameraRequest& request)
    {
        return request.slot == slot;
    });

    rig(slot).controller.reset_scene_state();
}

void CameraManager::reset_all() noexcept
{
    _requests.clear();

    for (CameraRig& camera_rig : _rigs)
    {
        camera_rig.controller.reset_scene_state();
    }
}

std::size_t CameraManager::slot_index(CameraSlot slot) noexcept
{
    const std::size_t index = static_cast<std::size_t>(slot);
    const std::size_t slot_count = static_cast<std::size_t>(CameraSlot::Count);
    assert(index < slot_count);
    return index < slot_count ? index : static_cast<std::size_t>(CameraSlot::Main);
}

CameraManager::CameraRig& CameraManager::rig(CameraSlot slot) noexcept
{
    return _rigs[slot_index(slot)];
}

const CameraManager::CameraRig& CameraManager::rig(CameraSlot slot) const noexcept
{
    return _rigs[slot_index(slot)];
}

void CameraManager::process_requests()
{
    while (!_requests.empty())
    {
        CameraRequest request = std::move(_requests.front());
        _requests.pop_front();

        CameraController& target = rig(request.slot).controller;
        std::visit(
            [&target](auto&& payload)
            {
                using Payload = std::remove_cvref_t<decltype(payload)>;

                if constexpr (std::is_same_v<Payload, ShakeRequest>)
                    target.start_shake(payload.params);
                else if constexpr (std::is_same_v<Payload, ZoomToRequest>)
                    target.start_zoom_transition(
                        payload.target_zoom,
                        payload.duration_seconds
                    );
                else if constexpr (std::is_same_v<Payload, SnapToFocusRequest>)
                    target.snap_to_focus();
                else if constexpr (std::is_same_v<Payload, ClearEffectsRequest>)
                    target.clear_effects();
            },
            request.payload
        );
    }
}
}
