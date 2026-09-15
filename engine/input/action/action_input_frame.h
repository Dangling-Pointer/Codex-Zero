#pragma once

#include "input_action_types.h"

#include <unordered_map>

namespace elysia::input
{
class InputActionMap;

class ActionInputFrame
{
public:
    [[nodiscard]] bool contains(const InputActionId& action) const;
    [[nodiscard]] bool is_pressed(const InputActionId& action) const;
    [[nodiscard]] bool is_just_pressed(const InputActionId& action) const;
    [[nodiscard]] bool is_just_released(const InputActionId& action) const;
    [[nodiscard]] float axis1d(const InputActionId& action) const;
    [[nodiscard]] elysia::core::Vector2 axis2d(const InputActionId& action) const;
    [[nodiscard]] InputActionValue value(const InputActionId& action) const;

private:
    struct State
    {
        InputActionValueType value_type = InputActionValueType::Button;
        InputActionValue current;
        InputActionValue previous;
        float actuation_threshold = 0.5f;
    };

    [[nodiscard]] const State* find(const InputActionId& action) const;
    [[nodiscard]] static bool actuated(const State& state, const InputActionValue& value);

    friend class InputActionMap;
    std::unordered_map<InputActionId, State, InputActionIdHash> _states;
};
}
