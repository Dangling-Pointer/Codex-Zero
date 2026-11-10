#include "game_input_map.h"
#include "game_actions.h"
#include <stdexcept>

namespace game::input
{
elysia::input::InputActionMap make_player_input_map()
{
    using namespace elysia::input;
    InputActionMap map;
    const bool move = map.register_action(
        {actions::Move, InputActionValueType::Axis2D, 0.5f, 0.2f},
        {
            {actions::Move, Button2DInputBinding{RawInputControl::KeyA, RawInputControl::KeyD,
                RawInputControl::KeyW, RawInputControl::KeyS}},
            {actions::Move, Button2DInputBinding{RawInputControl::KeyLeft, RawInputControl::KeyRight,
                RawInputControl::KeyUp, RawInputControl::KeyDown}},
            {actions::Move, Button2DInputBinding{RawInputControl::GamepadDPadLeft, RawInputControl::GamepadDPadRight,
                RawInputControl::GamepadDPadUp, RawInputControl::GamepadDPadDown}},
            {actions::Move, Axis2DInputBinding{RawInputAxis::GamepadLeftX, RawInputAxis::GamepadLeftY, 1, 1}}
        });
    const bool fire = map.register_action(
        {actions::Fire, InputActionValueType::Button},
        {{actions::Fire, ButtonInputBinding{RawInputControl::MouseLeft}},
         {actions::Fire, ButtonInputBinding{RawInputControl::KeyF}}});
    if (!move || !fire || !map.valid())
        throw std::logic_error("Cannot register game player input map");
    return map;
}
}
