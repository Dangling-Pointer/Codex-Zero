#pragma once

#include "camera_motion.h"

#include <memory>
#include <utility>

namespace elysia::camera
{
class CameraService;

enum class CameraServiceError
{
    NoActiveRuntime,
    InvalidSlot,
    SlotNotOwned,
    InvalidArgument,
    StaleOperation,
    OperationFinished
};

enum class CameraOperationStatus
{
    Playing,
    Paused,
    Holding,
    Completed,
    Cancelled,
    Superseded,
    SceneEnded
};

namespace detail
{
// Main-thread state only. This never retains a scene, runtime or controller.
struct CameraOperationState
{
    CameraOperationStatus status = CameraOperationStatus::Playing;
    bool reached_target = false;
    std::uint64_t generation = 0;
};
}

template <typename Id>
class CameraOperation final
{
public:
    [[nodiscard]] CameraOperationStatus status() const noexcept { return _state->status; }
    [[nodiscard]] bool reached_target() const noexcept { return _state->reached_target; }

private:
    friend class CameraService;
    CameraOperation(std::shared_ptr<detail::CameraOperationState> state, Id id, CameraSlot slot)
        : _state(std::move(state)), _id(id), _slot(slot) {}

    std::shared_ptr<detail::CameraOperationState> _state;
    Id _id;
    CameraSlot _slot;
};

using CameraMotionOperation = CameraOperation<CameraMotionId>;
using CameraBlendOperation = CameraOperation<CameraBlendId>;
}
