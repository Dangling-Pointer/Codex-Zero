#pragma once

#include <SDL3/SDL.h>

#include <vector>

namespace elysia::input
{
class ControllerManager
{
public:
    ControllerManager() = default;
    ~ControllerManager();

    ControllerManager(const ControllerManager&) = delete;
    ControllerManager& operator=(const ControllerManager&) = delete;

    ControllerManager(ControllerManager&&) = delete;
    ControllerManager& operator=(ControllerManager&&) = delete;

    void initialize();
    void shutdown();
    [[nodiscard]] bool is_initialized() const noexcept { return _initialized; }
    void handle_event(const SDL_Event& event);

private:
    void open_connected_controllers();
    void open_controller(SDL_JoystickID joystick_id);
    void close_controller(SDL_JoystickID joystick_id);
    void close_all_controllers();

private:
    std::vector<SDL_Gamepad*> _controllers;
    bool _initialized = false;
};

}
