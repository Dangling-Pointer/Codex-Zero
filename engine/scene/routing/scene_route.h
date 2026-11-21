#pragma once

#include "scene_key.h"
#include "scene_payload.h"

namespace elysia::scene
{
enum class SceneReloadMode
{
    Reuse,
    Reset,
    Recreate
};

enum class HeldInputOnEnter
{
    Suppress,
    // Carry over gameplay state still held by the same player, without replaying input events.
    Preserve
};

struct SceneRoute
{
    SceneKey target = SceneKeys::Invalid;
    ScenePayload payload{};
    SceneReloadMode reload_mode = SceneReloadMode::Reuse;
    // Applied on the destination's first normal input frame.
    HeldInputOnEnter held_input_on_enter = HeldInputOnEnter::Suppress;
};
}
