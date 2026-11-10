#pragma once

#include "../../camera/camera_types.h"
#include <optional>

namespace elysia::scene
{
enum class CameraFocusMode
{
    Manual,
    ResolveEachFrame
};

struct CameraSceneConfig
{
    elysia::camera::CameraSlot initial_slot = elysia::camera::CameraSlot::Main;
    elysia::camera::CameraSlotSet owned_slots = elysia::camera::CameraSlot::Main;
    CameraFocusMode focus_mode = CameraFocusMode::Manual;
    bool advance_when_paused = false;
    std::optional<elysia::camera::CameraSlot> default_request_slot;
};
} // namespace elysia::scene
