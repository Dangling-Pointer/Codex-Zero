#pragma once

#include "scene_camera_config.h"
#include "../../camera/camera_manager.h"
#include "../../camera/camera_service_types.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace elysia::scene
{
struct CameraBlendCompletion
{
    elysia::camera::CameraBlendId id;
    elysia::camera::CameraSlot slot = elysia::camera::CameraSlot::Main;
};

struct SceneCameraUpdateResult
{
    elysia::camera::CameraUpdateResult motions;
    std::optional<CameraBlendCompletion> blend;
};

class SceneCameraRuntimeHost
{
public:
    virtual ~SceneCameraRuntimeHost() = default;
    [[nodiscard]] virtual std::optional<elysia::camera::CameraFocus>
        resolve_camera_focus(elysia::camera::CameraSlot slot) const = 0;
    virtual void on_camera_blend_completed(
        elysia::camera::CameraBlendId id, elysia::camera::CameraSlot slot) = 0;
    virtual void on_camera_motion_completed(
        elysia::camera::CameraMotionId id, elysia::camera::CameraSlot slot) = 0;
};

class SceneCameraRuntime final
{
public:
    explicit SceneCameraRuntime(CameraSceneConfig config);
    ~SceneCameraRuntime();
    SceneCameraRuntime(const SceneCameraRuntime&) = delete;
    SceneCameraRuntime& operator=(const SceneCameraRuntime&) = delete;
    SceneCameraRuntime(SceneCameraRuntime&&) = delete;
    SceneCameraRuntime& operator=(SceneCameraRuntime&&) = delete;

    [[nodiscard]] elysia::camera::CameraSlot presented_slot() const noexcept
    {
        return _presented_slot;
    }
    [[nodiscard]] const elysia::camera::Camera& presented_camera() const noexcept;
    [[nodiscard]] const elysia::camera::Camera& slot_camera(
        elysia::camera::CameraSlot slot) const;
    void set_center(elysia::camera::CameraSlot slot, const elysia::core::Vector2& center);
    void set_zoom(elysia::camera::CameraSlot slot, float zoom);
    void set_focus(
        elysia::camera::CameraSlot slot, std::optional<elysia::camera::CameraFocus> focus);
    void set_world_bounds(
        elysia::camera::CameraSlot slot, std::optional<elysia::core::Rect> world_bounds);
    void set_follow_strategy(
        elysia::camera::CameraSlot slot,
        std::unique_ptr<elysia::camera::IFollowStrategy> follow_strategy);
    void request_shake(
        elysia::camera::CameraSlot slot, const elysia::camera::CameraShakeParams& params);
    void snap_to_focus(elysia::camera::CameraSlot slot);
    void clear_effects(elysia::camera::CameraSlot slot);

    void cut_to(elysia::camera::CameraSlot slot);
    [[nodiscard]] std::optional<elysia::camera::CameraBlendId> blend_to(
        elysia::camera::CameraSlot slot,
        const elysia::camera::CameraBlendSpec& spec = {});
    [[nodiscard]] std::optional<elysia::camera::CameraBlendState> blend_state(
        elysia::camera::CameraBlendId id) const noexcept;
    bool pause_blend(elysia::camera::CameraBlendId id) noexcept;
    bool resume_blend(elysia::camera::CameraBlendId id) noexcept;
    bool cancel_blend(elysia::camera::CameraBlendId id) noexcept;

    [[nodiscard]] elysia::camera::CameraMotionId move_to(
        elysia::camera::CameraSlot slot,
        const elysia::camera::CameraPoseTarget& target,
        double duration_seconds,
        elysia::camera::CameraEasing easing = elysia::camera::CameraEasing::SmoothStep,
        elysia::camera::CameraMotionEndBehavior end_behavior =
            elysia::camera::CameraMotionEndBehavior::ResumeFollow);
    [[nodiscard]] elysia::camera::CameraMotionId play_path(
        elysia::camera::CameraSlot slot,
        const elysia::camera::CameraMotionSpec& motion);
    [[nodiscard]] std::optional<elysia::camera::CameraMotionState> motion_state(
        elysia::camera::CameraMotionId id) const noexcept;
    bool pause_motion(elysia::camera::CameraMotionId id) noexcept;
    bool resume_motion(elysia::camera::CameraMotionId id) noexcept;
    bool cancel_motion(elysia::camera::CameraMotionId id) noexcept;

    [[nodiscard]] SceneCameraUpdateResult advance(double delta_seconds);
    void advance(double delta_seconds, bool scene_paused, SceneCameraRuntimeHost& host);
    void cancel_activity() noexcept;
    void reset() noexcept;

private:
    friend class elysia::camera::CameraService;
    using OperationState = std::shared_ptr<elysia::camera::detail::CameraOperationState>;
    using Status = elysia::camera::CameraOperationStatus;
    enum class RequestKind { Shake, SnapToFocus, ClearEffects };
    struct Request
    {
        RequestKind kind;
        elysia::camera::CameraSlot slot;
        elysia::camera::CameraShakeParams shake{};
    };
    void finish_motion(std::size_t index, Status status) noexcept;
    void finish_blend(Status status) noexcept;
    void end_service_activity() noexcept;
    void clear_owned_transient_effects() noexcept;
    void flush_requests();

    struct ActiveBlend
    {
        elysia::camera::CameraBlendId id;
        elysia::camera::CameraSlot target = elysia::camera::CameraSlot::Main;
        elysia::camera::Camera source;
        elysia::camera::CameraBlendSpec spec;
        double elapsed_seconds = 0.0;
        bool paused = false;
    };

    void validate_owned_slot(elysia::camera::CameraSlot slot) const;
    [[nodiscard]] std::optional<std::size_t> owned_motion_index(
        elysia::camera::CameraMotionId id) const noexcept;

    CameraSceneConfig _config;
    elysia::camera::CameraSlot _presented_slot = elysia::camera::CameraSlot::Main;
    elysia::camera::Camera _blended_camera;
    std::optional<ActiveBlend> _blend;
    std::array<std::optional<elysia::camera::CameraMotionId>,
               static_cast<std::size_t>(elysia::camera::CameraSlot::Count)>
        _motions{};
    std::uint64_t _next_blend_id = 1;
    std::array<OperationState, static_cast<std::size_t>(elysia::camera::CameraSlot::Count)> _motion_operations{};
    OperationState _blend_operation;
    std::vector<Request> _requests;
};
} // namespace elysia::scene
