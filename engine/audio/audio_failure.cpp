#include "audio_failure.h"
#include "../tools/logger.h"

namespace elysia::audio
{
void report_audio_failure(const AudioFailure& failure,std::uint64_t handle) noexcept
{
    try
    {
        auto diagnostic=failure.diagnostic;
        if(handle) diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry(
            "sound_handle",std::to_string(handle)));
        elysia::tools::Logger::instance()->warn("audio",elysia::core::format_failure_diagnostic(
            diagnostic,"AUDIO-"+std::string(audio_error_name(failure.code)),"audio"),diagnostic.origin);
    }
    catch (...) {}
}
}
