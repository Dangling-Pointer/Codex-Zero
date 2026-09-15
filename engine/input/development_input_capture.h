#pragma once

#include <cstdint>

namespace elysia::input
{
enum class DevelopmentInputCapture : std::uint8_t
{
    None = 0,
    Keyboard = 1u << 0,
    Pointer = 1u << 1,
    Gamepad = 1u << 2
};

[[nodiscard]] constexpr DevelopmentInputCapture operator|(
    DevelopmentInputCapture first,
    DevelopmentInputCapture second) noexcept
{
    return static_cast<DevelopmentInputCapture>(
        static_cast<std::uint8_t>(first)
        | static_cast<std::uint8_t>(second));
}

[[nodiscard]] constexpr DevelopmentInputCapture operator&(
    DevelopmentInputCapture first,
    DevelopmentInputCapture second) noexcept
{
    return static_cast<DevelopmentInputCapture>(
        static_cast<std::uint8_t>(first)
        & static_cast<std::uint8_t>(second));
}

[[nodiscard]] constexpr bool captures_development_input(
    DevelopmentInputCapture capture,
    DevelopmentInputCapture requested) noexcept
{
    return (capture & requested) != DevelopmentInputCapture::None;
}
}
