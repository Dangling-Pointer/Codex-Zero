#pragma once

#include "../../config/user_config_data.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <string>

namespace elysia::application::detail
{
struct ApplicationWindowOperations
{
    std::function<int(std::uint32_t)> set_fullscreen;
    std::function<void(int,int)> set_size;
    std::function<void()> center;
    std::function<std::string()> error_message;
};

[[nodiscard]] std::expected<void,std::string> apply_window_settings(
    const elysia::config::WindowSettings& settings,
    const ApplicationWindowOperations& operations);
}
