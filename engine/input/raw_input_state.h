#pragma once

#include "raw_input_types.h"

#include <array>
#include <cstddef>

namespace elysia::input
{
class RawInputState
{
public:
    void begin_frame()
    {
        _previous = _current;
    }

    void clear()
    {
        _current.fill(false);
        _previous.fill(false);
        _axes.fill(0.0f);
    }

    void clear_keyboard()
    {
        clear_control_range(RawInputControl::KeyA, RawInputControl::KeyF12);
    }

    void clear_pointer()
    {
        clear_control_range(RawInputControl::MouseLeft, RawInputControl::MouseX2);
    }

    void clear_gamepad()
    {
        clear_control_range(
            RawInputControl::GamepadSouth,
            RawInputControl::GamepadTouchpad);
        _axes.fill(0.0f);
    }

    void set_pressed(RawInputControl control, bool pressed)
    {
        if (!is_trackable_control(control))
        {
            return;
        }

        _current[index(control)] = pressed;
    }

    void set_axis(RawInputAxis axis, float value)
    {
        if (!is_trackable_axis(axis))
        {
            return;
        }

        _axes[axis_index(axis)] = value;
    }

    [[nodiscard]] bool is_pressed(RawInputControl control) const
    {
        if (!is_trackable_control(control))
        {
            return false;
        }

        return _current[index(control)];
    }

    [[nodiscard]] bool is_just_pressed(RawInputControl control) const
    {
        if (!is_trackable_control(control))
        {
            return false;
        }

        const std::size_t i = index(control);
        return _current[i] && !_previous[i];
    }

    [[nodiscard]] bool is_just_released(RawInputControl control) const
    {
        if (!is_trackable_control(control))
        {
            return false;
        }

        const std::size_t i = index(control);
        return !_current[i] && _previous[i];
    }

    [[nodiscard]] float axis_value(RawInputAxis axis) const
    {
        if (!is_trackable_axis(axis))
        {
            return 0.0f;
        }

        return _axes[axis_index(axis)];
    }

private:
    void clear_control_range(
        RawInputControl first,
        RawInputControl last)
    {
        const std::size_t begin = index(first);
        const std::size_t end = index(last);
        for (std::size_t i = begin; i <= end; ++i)
        {
            _current[i] = false;
            _previous[i] = false;
        }
    }

    static constexpr bool is_trackable_control(RawInputControl control)
    {
        return is_keyboard_control(control)
            || is_mouse_button_control(control)
            || is_gamepad_button_control(control);
    }

    static constexpr bool is_trackable_axis(RawInputAxis axis)
    {
        return axis != RawInputAxis::None && axis != RawInputAxis::Count;
    }

    static constexpr std::size_t index(RawInputControl control)
    {
        return static_cast<std::size_t>(control);
    }

    static constexpr std::size_t axis_index(RawInputAxis axis)
    {
        return static_cast<std::size_t>(axis);
    }

private:
    std::array<bool, static_cast<std::size_t>(RawInputControl::Count)> _current{};
    std::array<bool, static_cast<std::size_t>(RawInputControl::Count)> _previous{};
    std::array<float, static_cast<std::size_t>(RawInputAxis::Count)> _axes{};
};

}
