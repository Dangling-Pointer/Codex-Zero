#pragma once

#include "../../../input/action/input_action_types.h"

namespace elysia::gameplay
{
class GameplayInputEventReceiver
{
public:
    virtual ~GameplayInputEventReceiver() = default;
    virtual bool on_gameplay_input_event(const elysia::input::ActionInputEvent& event) = 0;
};
}
