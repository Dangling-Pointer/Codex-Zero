#include "input_device_tracker.h"

namespace elysia::input
{
void InputDeviceTracker::begin_frame()
{
    _device_switched_this_frame = false;
}

void InputDeviceTracker::reset()
{
    _current_device = InputDevice::Unknown;
    _device_switched_this_frame = false;
}

void InputDeviceTracker::deactivate(InputDevice device)
{
    if (_current_device != device)
    {
        return;
    }

    _current_device = InputDevice::Unknown;
    _device_switched_this_frame = true;
}

InputDeviceUpdateResult InputDeviceTracker::process_event(const SDL_Event& event)
{
    InputDeviceUpdateResult result;

    const InputDevice event_device = detect_event_device(event);
    result.event_device = event_device;
    if (event_device == InputDevice::Unknown)
    {
        return result;
    }

    if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        return result;
    }

    if (_current_device == InputDevice::Unknown)
    {
        _current_device = event_device;
        return result;
    }

    if (event_device == InputDevice::Gamepad && _current_device != InputDevice::Gamepad)
    {
        _current_device = event_device;
        _device_switched_this_frame = true;
        result.should_clear_state = true;
        result.should_reset_gamepad_state = true;
        return result;
    }

    if (is_keyboard_or_mouse(event_device))
    {
        if (_current_device == InputDevice::Gamepad)
        {
            _device_switched_this_frame = true;
            result.should_clear_state = true;
            result.should_reset_gamepad_state = true;
        }

        _current_device = event_device;
    }

    return result;
}

InputDevice InputDeviceTracker::current_device() const
{
    return _current_device;
}

bool InputDeviceTracker::device_switched_this_frame() const
{
    return _device_switched_this_frame;
}

InputDevice InputDeviceTracker::detect_event_device(const SDL_Event& event) const
{
    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_EDITING:
    case SDL_EVENT_TEXT_INPUT:
        return InputDevice::Keyboard;

    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return InputDevice::Mouse;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return InputDevice::Gamepad;

    default:
        return InputDevice::Unknown;
    }
}

bool InputDeviceTracker::is_keyboard_or_mouse(InputDevice device) const
{
    return device == InputDevice::Keyboard || device == InputDevice::Mouse;
}

}
