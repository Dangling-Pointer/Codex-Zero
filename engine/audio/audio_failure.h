#pragma once
#include "../core/diagnostics/failure_diagnostic.h"
#include <expected>
#include <cstdint>

namespace elysia::audio
{
enum class AudioError
{
    NotInitialized, NoActiveScene, InvalidArgument, MissingResource, InvalidHandle,
    CooldownActive, GroupLimitReached, ChannelLimitReached, BackendFailure
};
[[nodiscard]] constexpr std::string_view audio_error_name(AudioError code) noexcept
{
    switch(code)
    {
    case AudioError::NotInitialized: return "NotInitialized";
    case AudioError::NoActiveScene: return "NoActiveScene";
    case AudioError::InvalidArgument: return "InvalidArgument";
    case AudioError::MissingResource: return "MissingResource";
    case AudioError::InvalidHandle: return "InvalidHandle";
    case AudioError::CooldownActive: return "CooldownActive";
    case AudioError::GroupLimitReached: return "GroupLimitReached";
    case AudioError::ChannelLimitReached: return "ChannelLimitReached";
    case AudioError::BackendFailure: return "BackendFailure";
    }
    return "Unknown";
}
struct AudioFailure
{
    AudioError code;
    elysia::core::FailureDiagnostic diagnostic;
};
template<class T> using AudioResult = std::expected<T,AudioFailure>;

inline AudioFailure audio_failure(AudioError code,std::string message,std::string key = {},
    std::source_location origin = std::source_location::current())
{
    return {code,elysia::core::make_failure_diagnostic(std::move(message),std::move(key),{},origin)};
}
inline void collect_audio_failure(AudioResult<void>& result,const AudioResult<void>& next)
{
    if (next) return;
    if (result) result = std::unexpected(next.error());
    else
    {
        result.error().diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry(
            "audio_cleanup",{},{},{},{},next.error().diagnostic.message,next.error().diagnostic.origin));
        const auto& entries=next.error().diagnostic.entries;
        result.error().diagnostic.entries.insert(result.error().diagnostic.entries.end(),entries.begin(),entries.end());
    }
}
void report_audio_failure(const AudioFailure& failure,std::uint64_t handle = 0) noexcept;
}
