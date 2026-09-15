#include "input_system.h"

#include <cmath>

namespace elysia::input
{
namespace
{
constexpr float k_controller_activation_dead_zone = 0.22f;

[[nodiscard]] bool has_mouse_position(const RawInputEvent& event) noexcept
{
    return event.device == InputDevice::Mouse
        && (event.type == RawInputEventType::MouseMoved
            || event.type == RawInputEventType::MouseWheel
            || event.mouse_button != 0);
}
}

void InputSystem::initialize()
{
    reset_input_lifecycle();
    _controller_manager.initialize();
    _initialized = true;
}

void InputSystem::shutdown()
{
    _controller_manager.shutdown();
    reset_input_lifecycle();
    _initialized = false;
}

void InputSystem::set_development_input_capture(
    DevelopmentInputCapture capture) noexcept
{
    const DevelopmentInputCapture previous = _development_input_capture;
    _development_input_capture = capture;

    if (captures_development_input(capture, DevelopmentInputCapture::Keyboard)
        && !captures_development_input(previous, DevelopmentInputCapture::Keyboard))
    {
        _state.clear_keyboard();
    }
    if (captures_development_input(capture, DevelopmentInputCapture::Pointer)
        && !captures_development_input(previous, DevelopmentInputCapture::Pointer))
    {
        _state.clear_pointer();
        _mouse_delta_x = 0;
        _mouse_delta_y = 0;
    }
    if (captures_development_input(capture, DevelopmentInputCapture::Gamepad)
        && !captures_development_input(previous, DevelopmentInputCapture::Gamepad))
    {
        _state.clear_gamepad();
        _gamepad_translator.reset();
    }

    if (!captures_development_input(capture, DevelopmentInputCapture::Pointer)
        && captures_development_input(previous, DevelopmentInputCapture::Pointer))
    {
        refresh_mouse_position();
    }
}

void InputSystem::begin_frame()
{
    _state.begin_frame();
    _events.clear();
    _device_tracker.begin_frame();
    _mouse_delta_x = 0;
    _mouse_delta_y = 0;
}

void InputSystem::end_frame()
{
}

void InputSystem::process_event(const SDL_Event& event)
{
    _controller_manager.handle_event(event);

    if (event.type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        handle_controller_removed(event);
        return;
    }

    if (should_clear_state_for_event(event))
    {
        _state.clear();
        _device_tracker.reset();
        _gamepad_translator.reset();
        _active_controller_id.reset();
        return;
    }

    if (is_window_size_changed_event(event))
    {
        refresh_mouse_position();
        return;
    }

    if ((event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN
            || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP
            || event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
        && !should_accept_controller_event(event))
    {
        return;
    }

    if (is_event_captured(event))
        return;

    const InputDevice previous_device = _device_tracker.current_device();
    const InputDeviceUpdateResult device_update = _device_tracker.process_event(event);
    if (device_update.should_clear_state)
    {
        if (previous_device == InputDevice::Gamepad)
        {
            release_gamepad_state();
        }
        else
        {
            _state.clear();
        }
    }

    if (device_update.should_reset_gamepad_state)
    {
        _gamepad_translator.reset();
    }

    translate_event(event, device_update.event_device);
}

bool InputSystem::is_event_captured(const SDL_Event& event) const noexcept
{
    const auto captures = [this](DevelopmentInputCapture requested)
    {
        return captures_development_input(
            _development_input_capture, requested);
    };

    switch (event.type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_TEXT_EDITING:
        return captures(DevelopmentInputCapture::Keyboard);

    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return captures(DevelopmentInputCapture::Pointer);

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        return captures(DevelopmentInputCapture::Gamepad);

    default:
        return false;
    }
}

RawInputFrame InputSystem::frame() const
{
    return {
        _state,
        _device_tracker.current_device(),
        _device_tracker.device_switched_this_frame(),
        _mouse_x,
        _mouse_y,
        _mouse_delta_x,
        _mouse_delta_y
    };
}

const std::vector<RawInputEvent>& InputSystem::events() const
{
    return _events;
}

InputDevice InputSystem::current_device() const
{
    return _device_tracker.current_device();
}

void InputSystem::set_renderer(SDL_Renderer* renderer)
{
    _renderer = renderer;
}

void InputSystem::translate_event(const SDL_Event& event, InputDevice event_device)
{
    InputTranslator* translator = select_translator(event_device);
    if (!translator)
    {
        return;
    }

    SDL_Event logical_event = event;
    if (_renderer && (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP))
        SDL_ConvertEventToRenderCoordinates(_renderer,&logical_event);
    std::vector<RawInputEvent> input_events = translator->translate_event(logical_event);

    for (const RawInputEvent& input_event : input_events)
    {
        append_event(normalize_mouse_event(input_event));
    }
}

InputTranslator* InputSystem::select_translator(InputDevice device)
{
    if (device == InputDevice::Gamepad)
    {
        return &_gamepad_translator;
    }

    if (device == InputDevice::Keyboard || device == InputDevice::Mouse)
    {
        return &_keyboard_mouse_translator;
    }

    return nullptr;
}

RawInputEvent InputSystem::normalize_mouse_event(const RawInputEvent& event) const
{
    RawInputEvent converted_event = event;
    if (!has_mouse_position(event))
    {
        return converted_event;
    }

    // Motion/buttons were converted before translation; queried wheel positions are window coordinates.
    if (event.type == RawInputEventType::MouseWheel)
    {
        if (_has_mouse_position)
        {
            converted_event.mouse_x = _mouse_x;
            converted_event.mouse_y = _mouse_y;
        }
        else
        {
            float window_x=0.0f,window_y=0.0f;
            SDL_GetMouseState(&window_x,&window_y);
            convert_window_to_logical(
                window_x,
                window_y,
                converted_event.mouse_x,
                converted_event.mouse_y
            );
        }
    }

    if (event.type == RawInputEventType::MouseMoved)
    {
        if (_has_mouse_position)
        {
            converted_event.mouse_delta_x = converted_event.mouse_x - _mouse_x;
            converted_event.mouse_delta_y = converted_event.mouse_y - _mouse_y;
        }
        else
        {
            converted_event.mouse_delta_x = 0;
            converted_event.mouse_delta_y = 0;
        }
    }
    else
    {
        converted_event.mouse_delta_x = 0;
        converted_event.mouse_delta_y = 0;
    }

    return converted_event;
}

void InputSystem::update_mouse_frame_cache(const RawInputEvent& event)
{
    if (!has_mouse_position(event))
    {
        return;
    }

    _mouse_x = event.mouse_x;
    _mouse_y = event.mouse_y;

    if (event.type == RawInputEventType::MouseMoved)
    {
        _mouse_delta_x += event.mouse_delta_x;
        _mouse_delta_y += event.mouse_delta_y;
    }

    _has_mouse_position = true;
}

void InputSystem::refresh_mouse_position()
{
    RawInputEvent mouse_event;
    mouse_event.type = RawInputEventType::MouseMoved;
    mouse_event.device = InputDevice::Mouse;
    float window_x=0,window_y=0;
    SDL_GetMouseState(&window_x,&window_y);

    RawInputEvent converted_event = mouse_event;
    convert_window_to_logical(
        window_x,
        window_y,
        converted_event.mouse_x,
        converted_event.mouse_y
    );
    converted_event.mouse_delta_x = 0;
    converted_event.mouse_delta_y = 0;
    append_event(converted_event);
}

void InputSystem::convert_window_to_logical(float window_x, float window_y, int& logical_x, int& logical_y) const
{
    if (!_renderer)
    {
        logical_x = static_cast<int>(std::lround(window_x));
        logical_y = static_cast<int>(std::lround(window_y));
        return;
    }

    float converted_x = static_cast<float>(window_x);
    float converted_y = static_cast<float>(window_y);
    SDL_RenderCoordinatesFromWindow(_renderer, window_x, window_y, &converted_x, &converted_y);

    logical_x = static_cast<int>(std::lround(converted_x));
    logical_y = static_cast<int>(std::lround(converted_y));
}

void InputSystem::apply_event(const RawInputEvent& event)
{
    if (event.type == RawInputEventType::ControlPressed)
    {
        _state.set_pressed(event.control, true);
        return;
    }

    if (event.type == RawInputEventType::ControlReleased)
    {
        _state.set_pressed(event.control, false);
        return;
    }

    if (event.type == RawInputEventType::AxisChanged)
    {
        _state.set_axis(event.axis, event.axis_value);
    }
}

void InputSystem::append_event(const RawInputEvent& event)
{
    apply_event(event);
    update_mouse_frame_cache(event);
    _events.push_back(event);
}

bool InputSystem::should_accept_controller_event(const SDL_Event& event)
{
    const SDL_JoystickID controller_id =
        event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION
            ? event.gaxis.which
            : event.gbutton.which;
    const bool activation_event = is_controller_activation_event(event);

    if (_active_controller_id && *_active_controller_id == controller_id)
    {
        return _device_tracker.current_device() == InputDevice::Gamepad
            || activation_event;
    }

    if (!activation_event)
    {
        return false;
    }

    if (_active_controller_id)
    {
        release_gamepad_state();
        _gamepad_translator.reset();
    }

    _active_controller_id = controller_id;
    return true;
}

bool InputSystem::is_controller_activation_event(const SDL_Event& event) const
{
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
    {
        return true;
    }

    if (event.type != SDL_EVENT_GAMEPAD_AXIS_MOTION)
    {
        return false;
    }

    const float normalized_value =
        std::fabs(static_cast<float>(event.gaxis.value) / 32767.0f);
    return normalized_value > k_controller_activation_dead_zone;
}

void InputSystem::handle_controller_removed(const SDL_Event& event)
{
    if (!_active_controller_id || *_active_controller_id != event.gdevice.which)
    {
        return;
    }

    release_gamepad_state();
    _gamepad_translator.reset();
    _active_controller_id.reset();
    _device_tracker.deactivate(InputDevice::Gamepad);
}

void InputSystem::release_gamepad_state()
{
    for (int value = static_cast<int>(RawInputControl::GamepadSouth);
        value <= static_cast<int>(RawInputControl::GamepadTouchpad);
        ++value)
    {
        const RawInputControl control = static_cast<RawInputControl>(value);
        if (!_state.is_pressed(control))
        {
            continue;
        }

        RawInputEvent release_event;
        release_event.control = control;
        release_event.type = RawInputEventType::ControlReleased;
        release_event.device = InputDevice::Gamepad;
        append_event(release_event);
    }

    for (int value = static_cast<int>(RawInputAxis::GamepadLeftX);
        value < static_cast<int>(RawInputAxis::Count);
        ++value)
    {
        const RawInputAxis axis = static_cast<RawInputAxis>(value);
        if (_state.axis_value(axis) == 0.0f)
        {
            continue;
        }

        RawInputEvent axis_event;
        axis_event.axis = axis;
        axis_event.type = RawInputEventType::AxisChanged;
        axis_event.device = InputDevice::Gamepad;
        axis_event.axis_value = 0.0f;
        append_event(axis_event);
    }
}

void InputSystem::reset_input_lifecycle()
{
    _state.clear();
    _events.clear();
    _device_tracker.reset();
    _gamepad_translator.reset();
    _active_controller_id.reset();
    _mouse_x = 0;
    _mouse_y = 0;
    _mouse_delta_x = 0;
    _mouse_delta_y = 0;
    _has_mouse_position = false;
    _development_input_capture = DevelopmentInputCapture::None;
}

bool InputSystem::should_clear_state_for_event(const SDL_Event& event) const
{
    return event.type == SDL_EVENT_WINDOW_FOCUS_LOST;
}

bool InputSystem::is_window_size_changed_event(const SDL_Event& event) const
{
    return event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;
}

}
