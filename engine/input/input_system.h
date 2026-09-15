#pragma once

#include "raw_input_frame.h"
#include "development_input_capture.h"
#include "input_device_tracker.h"
#include "controller_manager.h"

#include "translator/gamepad_input_translator.h"
#include "translator/keyboard_mouse_input_translator.h"
#include "translator/input_translator.h"

#include <SDL3/SDL.h>
#include <optional>
#include <vector>

namespace elysia::input
{
class InputSystem
{
public:
    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept { return _initialized; }
    void begin_frame();
    void end_frame();
    void process_event(const SDL_Event& event);
    void set_development_input_capture(
        DevelopmentInputCapture capture) noexcept;
    [[nodiscard]] DevelopmentInputCapture development_input_capture() const noexcept
    {
        return _development_input_capture;
    }
    RawInputFrame frame() const;
    const std::vector<RawInputEvent>& events() const;
    InputDevice current_device() const;
    void set_renderer(SDL_Renderer* renderer);

private:
    void translate_event(const SDL_Event& event, InputDevice event_device);
    InputTranslator* select_translator(InputDevice device);
    RawInputEvent normalize_mouse_event(const RawInputEvent& event) const;
    void update_mouse_frame_cache(const RawInputEvent& event);
    void refresh_mouse_position();
    void convert_window_to_logical(float window_x, float window_y, int& logical_x, int& logical_y) const;
    void apply_event(const RawInputEvent& event);
    void append_event(const RawInputEvent& event);
    bool should_accept_controller_event(const SDL_Event& event);
    bool is_controller_activation_event(const SDL_Event& event) const;
    void handle_controller_removed(const SDL_Event& event);
    void release_gamepad_state();
    void reset_input_lifecycle();
    bool should_clear_state_for_event(const SDL_Event& event) const;
    bool is_window_size_changed_event(const SDL_Event& event) const;
    [[nodiscard]] bool is_event_captured(const SDL_Event& event) const noexcept;

private:
    RawInputState _state;
    std::vector<RawInputEvent> _events;
    ControllerManager _controller_manager;
    InputDeviceTracker _device_tracker;
    SDL_Renderer* _renderer = nullptr;
    KeyboardMouseInputTranslator _keyboard_mouse_translator;
    GamepadInputTranslator _gamepad_translator;
    int _mouse_x = 0;
    int _mouse_y = 0;
    int _mouse_delta_x = 0;
    int _mouse_delta_y = 0;
    bool _has_mouse_position = false;
    std::optional<SDL_JoystickID> _active_controller_id;
    bool _initialized = false;
    DevelopmentInputCapture _development_input_capture =
        DevelopmentInputCapture::None;
};

}
