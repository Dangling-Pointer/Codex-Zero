#include "gameplay_input_map.h"

#include "gameplay_actions.h"

#include <stdexcept>

namespace elysia::gameplay
{
namespace
{
using namespace elysia::input;

InputBinding button(const InputActionId& action, RawInputControl control)
{
    return { action, ButtonInputBinding{ control } };
}

void require_registered(bool registered)
{
    if (!registered)
        throw std::logic_error("Invalid standard gameplay input action registration.");
}
}

elysia::input::InputActionMap make_default_gameplay_input_map()
{
    InputActionMap map;
    require_registered(map.register_action(
        { actions::Move, InputActionValueType::Axis2D, 0.5f, 0.2f },
        {
            { actions::Move, Button2DInputBinding{
                RawInputControl::KeyA, RawInputControl::KeyD,
                RawInputControl::KeyW, RawInputControl::KeyS } },
            { actions::Move, Button2DInputBinding{
                RawInputControl::KeyLeft, RawInputControl::KeyRight,
                RawInputControl::KeyUp, RawInputControl::KeyDown } },
            { actions::Move, Button2DInputBinding{
                RawInputControl::GamepadDPadLeft, RawInputControl::GamepadDPadRight,
                RawInputControl::GamepadDPadUp, RawInputControl::GamepadDPadDown } },
            { actions::Move, Axis2DInputBinding{
                RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY, 1.0f, 1.0f } }
        }));

    require_registered(map.register_action(
        { actions::Jump, InputActionValueType::Button },
        { button(actions::Jump, RawInputControl::KeySpace),
          button(actions::Jump, RawInputControl::GamepadSouth) }));
    require_registered(map.register_action(
        { actions::Primary, InputActionValueType::Button },
        { button(actions::Primary, RawInputControl::KeyJ),
          button(actions::Primary, RawInputControl::MouseLeft),
          button(actions::Primary, RawInputControl::GamepadWest) }));
    require_registered(map.register_action(
        { actions::Secondary, InputActionValueType::Button },
        { button(actions::Secondary, RawInputControl::KeyK),
          button(actions::Secondary, RawInputControl::GamepadNorth) }));
    require_registered(map.register_action(
        { actions::Guard, InputActionValueType::Button },
        { button(actions::Guard, RawInputControl::KeyL),
          button(actions::Guard, RawInputControl::MouseRight),
          button(actions::Guard, RawInputControl::GamepadLeftShoulder) }));
    require_registered(map.register_action(
        { actions::Dash, InputActionValueType::Button },
        { button(actions::Dash, RawInputControl::KeyLeftShift),
          button(actions::Dash, RawInputControl::KeyRightShift),
          button(actions::Dash, RawInputControl::GamepadRightShoulder) }));
    require_registered(map.register_action(
        { actions::Pause, InputActionValueType::Button },
        { button(actions::Pause, RawInputControl::KeyP),
          button(actions::Pause, RawInputControl::GamepadStart) }));
    return map;
}
}
