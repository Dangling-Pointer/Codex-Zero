#pragma once

#include "raw_input_types.h"

#include <SDL3/SDL.h>

namespace elysia::input
{
struct InputDeviceUpdateResult
{
    InputDevice event_device = InputDevice::Unknown;
    bool should_clear_state = false;
    bool should_reset_gamepad_state = false;
};

class InputDeviceTracker
{
public:
    void begin_frame();
    InputDeviceUpdateResult process_event(const SDL_Event& event);
    InputDevice current_device() const;
    bool device_switched_this_frame() const;
    void deactivate(InputDevice device);
    void reset();

private:
    InputDevice detect_event_device(const SDL_Event& event) const;
    bool is_keyboard_or_mouse(InputDevice device) const;

private:
    InputDevice _current_device = InputDevice::Unknown;
    bool _device_switched_this_frame = false;
};

}
