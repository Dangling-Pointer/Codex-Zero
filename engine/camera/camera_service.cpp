#include "camera_service.h"
#include "../scene/runtime/scene_camera_runtime.h"

#include <stdexcept>
#include <type_traits>

namespace elysia::camera
{
namespace
{
template <typename Action>
std::expected<void, CameraServiceError> apply(
    std::expected<void, CameraServiceError> validation, Action&& action)
{
    if (!validation) return validation;
    action();
    return {};
}

bool active(CameraOperationStatus status) noexcept
{
    return status == CameraOperationStatus::Playing || status == CameraOperationStatus::Paused
        || status == CameraOperationStatus::Holding;
}
}

void CameraService::attach_runtime(elysia::scene::SceneCameraRuntime& runtime)
{
    if (_runtime) throw std::logic_error("Another camera runtime is already attached.");
    // Construct the Manager while attach can still report allocation failures,
    // so synchronous detach cleanup never needs to initialize it.
    (void)CameraManager::instance();
    ++_generation;
    _runtime = &runtime;
}

void CameraService::detach_runtime(elysia::scene::SceneCameraRuntime& runtime) noexcept
{
    if (_runtime != &runtime) return;
    _runtime = nullptr;
    ++_generation;
    runtime.end_service_activity();
    runtime.clear_owned_transient_effects();
}

std::expected<void, CameraServiceError> CameraService::validate_slot(CameraSlot slot) const noexcept
{
    if (!_runtime) return std::unexpected(CameraServiceError::NoActiveRuntime);
    if (!valid_camera_slot(slot)) return std::unexpected(CameraServiceError::InvalidSlot);
    if (!_runtime->_config.owned_slots.contains(slot)) return std::unexpected(CameraServiceError::SlotNotOwned);
    return {};
}

std::expected<Camera, CameraServiceError> CameraService::camera() const
{
    if (!_runtime) return std::unexpected(CameraServiceError::NoActiveRuntime);
    return _runtime->presented_camera();
}

std::expected<Camera, CameraServiceError> CameraService::camera(CameraSlot slot) const
{
    if (auto result = validate_slot(slot); !result) return std::unexpected(result.error());
    return _runtime->slot_camera(slot);
}

std::expected<CameraSlot, CameraServiceError> CameraService::presented_slot() const
{
    if (!_runtime) return std::unexpected(CameraServiceError::NoActiveRuntime);
    return _runtime->presented_slot();
}

std::expected<CameraSlotSet, CameraServiceError> CameraService::owned_slots() const
{
    if (!_runtime) return std::unexpected(CameraServiceError::NoActiveRuntime);
    return _runtime->_config.owned_slots;
}

std::expected<void, CameraServiceError> CameraService::set_center(CameraSlot slot, const elysia::core::Vector2& center)
{
    return apply(validate_slot(slot), [&] { _runtime->set_center(slot, center); });
}

std::expected<void, CameraServiceError> CameraService::set_zoom(CameraSlot slot, float zoom)
{
    return apply(validate_slot(slot), [&] { _runtime->set_zoom(slot, zoom); });
}

std::expected<void, CameraServiceError> CameraService::set_focus(CameraSlot slot, std::optional<CameraFocus> focus)
{
    return apply(validate_slot(slot), [&] { _runtime->set_focus(slot, std::move(focus)); });
}

std::expected<void, CameraServiceError> CameraService::set_world_bounds(CameraSlot slot, std::optional<elysia::core::Rect> bounds)
{
    return apply(validate_slot(slot), [&] { _runtime->set_world_bounds(slot, std::move(bounds)); });
}

std::expected<void, CameraServiceError> CameraService::set_follow_strategy(CameraSlot slot, std::unique_ptr<IFollowStrategy> strategy)
{
    return apply(validate_slot(slot), [&] { _runtime->set_follow_strategy(slot, std::move(strategy)); });
}

std::expected<void, CameraServiceError> CameraService::request_shake(const CameraShakeParams& params)
{
    if (!_runtime) return std::unexpected(CameraServiceError::NoActiveRuntime);
    return request_shake(_runtime->_config.default_request_slot.value_or(_runtime->_config.initial_slot), params);
}

std::expected<void, CameraServiceError> CameraService::request_shake(CameraSlot slot, const CameraShakeParams& params)
{
    return apply(validate_slot(slot), [&] { _runtime->request_shake(slot, params); });
}

std::expected<void, CameraServiceError> CameraService::snap_to_focus(CameraSlot slot)
{
    return apply(validate_slot(slot), [&] { _runtime->snap_to_focus(slot); });
}

std::expected<void, CameraServiceError> CameraService::clear_effects(CameraSlot slot)
{
    return apply(validate_slot(slot), [&] { _runtime->clear_effects(slot); });
}

std::expected<void, CameraServiceError> CameraService::cut_to(CameraSlot slot)
{
    return apply(validate_slot(slot), [&] { _runtime->cut_to(slot); });
}

std::expected<CameraBlendOperation, CameraServiceError> CameraService::blend_to(CameraSlot slot, const CameraBlendSpec& spec)
{
    if (auto result = validate_slot(slot); !result) return std::unexpected(result.error());
    try { validate_camera_blend(spec); }
    catch (const std::invalid_argument&) { return std::unexpected(CameraServiceError::InvalidArgument); }
    auto state = std::make_shared<detail::CameraOperationState>();
    state->generation = _generation;
    const auto id = _runtime->blend_to(slot, spec);
    if (id) _runtime->_blend_operation = state;
    else
    {
        state->status = CameraOperationStatus::Completed;
        state->reached_target = true;
    }
    return CameraBlendOperation(std::move(state), id.value_or(CameraBlendId{}), slot);
}

std::expected<CameraMotionOperation, CameraServiceError> CameraService::move_to(
    CameraSlot slot, const CameraPoseTarget& target, double duration_seconds,
    CameraEasing easing, CameraMotionEndBehavior end_behavior)
{
    return play_path(slot, CameraMotionSpec{{CameraPathNode{target, duration_seconds, easing}}, end_behavior});
}

std::expected<CameraMotionOperation, CameraServiceError> CameraService::play_path(CameraSlot slot, const CameraMotionSpec& motion)
{
    if (auto result = validate_slot(slot); !result) return std::unexpected(result.error());
    try { validate_camera_motion(motion); }
    catch (const std::invalid_argument&) { return std::unexpected(CameraServiceError::InvalidArgument); }
    auto state = std::make_shared<detail::CameraOperationState>();
    state->generation = _generation;
    const auto id = _runtime->play_path(slot, motion);
    _runtime->_motion_operations[static_cast<std::size_t>(slot)] = state;
    return CameraMotionOperation(std::move(state), id, slot);
}

template <typename Id>
std::expected<void, CameraServiceError> CameraService::validate_operation(const CameraOperation<Id>& operation) const noexcept
{
    if (!operation._state || operation._state->generation != _generation || !_runtime)
        return std::unexpected(CameraServiceError::StaleOperation);
    if (!active(operation.status())) return std::unexpected(CameraServiceError::OperationFinished);
    const auto& owned = [&]() -> const auto& {
        if constexpr (std::is_same_v<Id, CameraMotionId>)
            return _runtime->_motion_operations[static_cast<std::size_t>(operation._slot)];
        else return _runtime->_blend_operation;
    }();
    if (owned != operation._state) return std::unexpected(CameraServiceError::StaleOperation);
    return {};
}

std::expected<void, CameraServiceError> CameraService::pause_motion(const CameraMotionOperation& operation)
{
    if (auto result = validate_operation(operation); !result) return result;
    if (!_runtime->pause_motion(operation._id)) return std::unexpected(CameraServiceError::OperationFinished);
    return {};
}

std::expected<void, CameraServiceError> CameraService::resume_motion(const CameraMotionOperation& operation)
{
    if (auto result = validate_operation(operation); !result) return result;
    if (!_runtime->resume_motion(operation._id)) return std::unexpected(CameraServiceError::OperationFinished);
    return {};
}

std::expected<void, CameraServiceError> CameraService::cancel_motion(const CameraMotionOperation& operation)
{
    if (auto result = validate_operation(operation); !result) return result;
    if (!_runtime->cancel_motion(operation._id)) return std::unexpected(CameraServiceError::OperationFinished);
    return {};
}

std::expected<void, CameraServiceError> CameraService::pause_blend(const CameraBlendOperation& operation)
{
    if (auto result = validate_operation(operation); !result) return result;
    if (!_runtime->pause_blend(operation._id)) return std::unexpected(CameraServiceError::OperationFinished);
    return {};
}

std::expected<void, CameraServiceError> CameraService::resume_blend(const CameraBlendOperation& operation)
{
    if (auto result = validate_operation(operation); !result) return result;
    if (!_runtime->resume_blend(operation._id)) return std::unexpected(CameraServiceError::OperationFinished);
    return {};
}

std::expected<void, CameraServiceError> CameraService::cancel_blend(const CameraBlendOperation& operation)
{
    if (auto result = validate_operation(operation); !result) return result;
    if (!_runtime->cancel_blend(operation._id)) return std::unexpected(CameraServiceError::OperationFinished);
    return {};
}
}
