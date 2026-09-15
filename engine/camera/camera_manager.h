#pragma once

#include "camera.h"
#include "camera_controller.h"
#include "../tools/singleton.h"

#include <array>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <variant>

#define ELYSIA_CAMERA (::elysia::camera::CameraManager::instance())

namespace elysia::camera
{
enum class CameraSlot : std::size_t
{
    Main,
    Cinematic,
    Auxiliary1,
    Auxiliary2,
    Count
};

class CameraManager final : public elysia::tools::Singleton<CameraManager>
{
    friend class elysia::tools::Singleton<CameraManager>;

public:
    [[nodiscard]] const Camera& camera(CameraSlot slot) const noexcept;

    void set_center(CameraSlot slot, const elysia::core::Vector2& center) noexcept;
    void set_viewport_size(CameraSlot slot, const elysia::core::Vector2& viewport_size) noexcept;
    void set_zoom(CameraSlot slot, float zoom) noexcept;
    void set_focus_rect(CameraSlot slot, std::optional<elysia::core::Rect> focus_rect) noexcept;
    void set_world_bounds(CameraSlot slot, std::optional<elysia::core::Rect> world_bounds) noexcept;
    void set_follow_strategy(CameraSlot slot, std::unique_ptr<IFollowStrategy> follow_strategy) noexcept;

    void request_shake(CameraSlot slot, const CameraShakeParams& params);
    void request_zoom_to(CameraSlot slot, float target_zoom, double duration_seconds);
    void request_snap_to_focus(CameraSlot slot);
    void request_clear_effects(CameraSlot slot);

    void update(double delta_seconds);
    void reset(CameraSlot slot) noexcept;
    void reset_all() noexcept;

private:
    struct CameraRig
    {
        Camera camera;
        CameraController controller;

        CameraRig() noexcept;
        CameraRig(const CameraRig&) = delete;
        CameraRig& operator=(const CameraRig&) = delete;
        CameraRig(CameraRig&&) = delete;
        CameraRig& operator=(CameraRig&&) = delete;
    };

    struct ShakeRequest
    {
        CameraShakeParams params;
    };

    struct SnapToFocusRequest {};
    struct ClearEffectsRequest {};
    struct ZoomToRequest
    {
        float target_zoom = Camera::k_default_zoom;
        double duration_seconds = 0.0;
    };

    using RequestPayload = std::variant<
        ShakeRequest,ZoomToRequest,
        SnapToFocusRequest,ClearEffectsRequest
    >;

    struct CameraRequest
    {
        CameraSlot slot = CameraSlot::Main;
        RequestPayload payload;
    };

    CameraManager() = default;

    [[nodiscard]] static std::size_t slot_index(CameraSlot slot) noexcept;
    [[nodiscard]] CameraRig& rig(CameraSlot slot) noexcept;
    [[nodiscard]] const CameraRig& rig(CameraSlot slot) const noexcept;
    void process_requests();

    std::array<CameraRig, static_cast<std::size_t>(CameraSlot::Count)> _rigs;
    std::deque<CameraRequest> _requests;
};
}
