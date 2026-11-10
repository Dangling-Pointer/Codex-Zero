#pragma once

#include "camera.h"
#include "camera_controller.h"
#include "camera_service_types.h"
#include "../tools/singleton.h"

#include <expected>

namespace elysia::scene { class Scene; class SceneCameraRuntime; }

#define ELYSIA_CAMERA (::elysia::camera::CameraService::instance())

namespace elysia::camera
{
// Main-thread facade for the currently attached scene. Queries return snapshots.
class CameraService final : public elysia::tools::Singleton<CameraService>
{
    friend class elysia::tools::Singleton<CameraService>;
    friend class elysia::scene::Scene;

public:
    [[nodiscard]] bool is_available() const noexcept { return _runtime != nullptr; }
    [[nodiscard]] std::expected<Camera, CameraServiceError> camera() const;
    [[nodiscard]] std::expected<Camera, CameraServiceError> camera(CameraSlot slot) const;
    [[nodiscard]] std::expected<CameraSlot, CameraServiceError> presented_slot() const;
    [[nodiscard]] std::expected<CameraSlotSet, CameraServiceError> owned_slots() const;

    [[nodiscard]] std::expected<void, CameraServiceError> set_center(CameraSlot slot, const elysia::core::Vector2& center);
    [[nodiscard]] std::expected<void, CameraServiceError> set_zoom(CameraSlot slot, float zoom);
    [[nodiscard]] std::expected<void, CameraServiceError> set_focus(CameraSlot slot, std::optional<CameraFocus> focus);
    [[nodiscard]] std::expected<void, CameraServiceError> set_world_bounds(CameraSlot slot, std::optional<elysia::core::Rect> bounds);
    [[nodiscard]] std::expected<void, CameraServiceError> set_follow_strategy(CameraSlot slot, std::unique_ptr<IFollowStrategy> strategy);
    [[nodiscard]] std::expected<void, CameraServiceError> request_shake(const CameraShakeParams& params);
    [[nodiscard]] std::expected<void, CameraServiceError> request_shake(CameraSlot slot, const CameraShakeParams& params);
    [[nodiscard]] std::expected<void, CameraServiceError> snap_to_focus(CameraSlot slot);
    [[nodiscard]] std::expected<void, CameraServiceError> clear_effects(CameraSlot slot);
    [[nodiscard]] std::expected<void, CameraServiceError> cut_to(CameraSlot slot);

    [[nodiscard]] std::expected<CameraBlendOperation, CameraServiceError> blend_to(CameraSlot slot, const CameraBlendSpec& spec = {});
    [[nodiscard]] std::expected<CameraMotionOperation, CameraServiceError> move_to(
        CameraSlot slot, const CameraPoseTarget& target, double duration_seconds,
        CameraEasing easing = CameraEasing::SmoothStep,
        CameraMotionEndBehavior end_behavior = CameraMotionEndBehavior::ResumeFollow);
    [[nodiscard]] std::expected<CameraMotionOperation, CameraServiceError> play_path(CameraSlot slot, const CameraMotionSpec& motion);

    [[nodiscard]] std::expected<void, CameraServiceError> pause_motion(const CameraMotionOperation& operation);
    [[nodiscard]] std::expected<void, CameraServiceError> resume_motion(const CameraMotionOperation& operation);
    [[nodiscard]] std::expected<void, CameraServiceError> cancel_motion(const CameraMotionOperation& operation);
    [[nodiscard]] std::expected<void, CameraServiceError> pause_blend(const CameraBlendOperation& operation);
    [[nodiscard]] std::expected<void, CameraServiceError> resume_blend(const CameraBlendOperation& operation);
    [[nodiscard]] std::expected<void, CameraServiceError> cancel_blend(const CameraBlendOperation& operation);

private:
    CameraService() = default;
    void attach_runtime(elysia::scene::SceneCameraRuntime& runtime);
    void detach_runtime(elysia::scene::SceneCameraRuntime& runtime) noexcept;
    [[nodiscard]] std::expected<void, CameraServiceError> validate_slot(CameraSlot slot) const noexcept;
    template <typename Id>
    [[nodiscard]] std::expected<void, CameraServiceError> validate_operation(const CameraOperation<Id>& operation) const noexcept;

    elysia::scene::SceneCameraRuntime* _runtime = nullptr;
    std::uint64_t _generation = 0;
};
}
