#pragma once

#include "gameplay_actions.h"
#include "../../input/action/action_input_frame.h"

#include <utility>

namespace elysia::gameplay
{
class GameplayInputFrame
{
public:
    explicit GameplayInputFrame(elysia::input::ActionInputFrame frame)
        : _frame(std::move(frame)) {}

    [[nodiscard]] const elysia::input::ActionInputFrame& actions() const noexcept { return _frame; }
    [[nodiscard]] elysia::core::Vector2 move() const { return _frame.axis2d(actions::Move); }
    [[nodiscard]] bool jump_pressed() const { return _frame.is_just_pressed(actions::Jump); }
    [[nodiscard]] bool primary_pressed() const { return _frame.is_just_pressed(actions::Primary); }
    [[nodiscard]] bool secondary_pressed() const { return _frame.is_just_pressed(actions::Secondary); }
    [[nodiscard]] bool guard_held() const { return _frame.is_pressed(actions::Guard); }
    [[nodiscard]] bool dash_pressed() const { return _frame.is_just_pressed(actions::Dash); }
    [[nodiscard]] bool pause_pressed() const { return _frame.is_just_pressed(actions::Pause); }

private:
    elysia::input::ActionInputFrame _frame;
};
}
