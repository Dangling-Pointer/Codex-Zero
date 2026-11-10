#pragma once

#include "../core/diagnostics/failure_diagnostic.h"

namespace elysia::effects
{
enum class EffectRequestError
{
    NoActiveScene,
    InvalidRequest,
    MissingEffectDefinition,
    MissingAnimation,
    MissingTexture,
    RuntimeUnavailable,
    CreationFailed,
    SceneRejected
};

struct EffectRequestFailure
{
    EffectRequestError code;
    elysia::core::FailureDiagnostic diagnostic;
};

[[nodiscard]] inline EffectRequestFailure make_effect_request_failure(
    EffectRequestError code, std::string message, std::string key = {},
    std::source_location origin = std::source_location::current())
{
    return {code, elysia::core::make_failure_diagnostic(
        std::move(message), std::move(key), {}, origin)};
}

[[nodiscard]] inline std::string_view effect_request_error_name(EffectRequestError code) noexcept
{
    switch (code)
    {
    case EffectRequestError::NoActiveScene: return "NoActiveScene";
    case EffectRequestError::InvalidRequest: return "InvalidRequest";
    case EffectRequestError::MissingEffectDefinition: return "MissingEffectDefinition";
    case EffectRequestError::MissingAnimation: return "MissingAnimation";
    case EffectRequestError::MissingTexture: return "MissingTexture";
    case EffectRequestError::RuntimeUnavailable: return "RuntimeUnavailable";
    case EffectRequestError::CreationFailed: return "CreationFailed";
    case EffectRequestError::SceneRejected: return "SceneRejected";
    }
    return "Unknown";
}

[[nodiscard]] inline std::string format_effect_request_failure(const EffectRequestFailure& failure)
{
    return elysia::core::format_failure_diagnostic(
        failure.diagnostic, effect_request_error_name(failure.code), "effects");
}
}
