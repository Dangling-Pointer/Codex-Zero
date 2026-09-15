#include "application_window_settings.h"

#include <SDL3/SDL.h>

namespace elysia::application::detail
{
std::expected<void,std::string> apply_window_settings(
    const elysia::config::WindowSettings& settings,
    const ApplicationWindowOperations& operations)
{
    if (settings.windowed_size.width <= 0
        || settings.windowed_size.height <= 0)
        return std::unexpected("Windowed size must be positive.");
    if (!operations.set_fullscreen
        || !operations.set_size
        || !operations.center
        || !operations.error_message)
        return std::unexpected("Window operations are unavailable.");

    switch (settings.mode)
    {
    case elysia::config::WindowMode::Windowed:
        if (operations.set_fullscreen(0) != 0)
            return std::unexpected(operations.error_message());
        operations.set_size(
            settings.windowed_size.width,
            settings.windowed_size.height);
        operations.center();
        return {};
    case elysia::config::WindowMode::BorderlessFullscreen:
        if (operations.set_fullscreen(SDL_WINDOW_FULLSCREEN) != 0)
            return std::unexpected(operations.error_message());
        return {};
    }
    return std::unexpected("Unknown window mode.");
}
}
