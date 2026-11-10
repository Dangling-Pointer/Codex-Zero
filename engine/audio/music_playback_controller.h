#pragma once
#include "audio_fade.h"
#include "audio_failure.h"
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace elysia::audio
{
struct MusicTransitionOptions
{
    int loops=-1;
    std::chrono::milliseconds fade_out{0},fade_in{0};
};
class MusicPlaybackController
{
public:
    struct Backend
    {
        std::function<AudioResult<void>(std::string_view,int,double)> start;
        std::function<AudioResult<void>()> stop;
        std::function<bool()> playing;
        std::function<AudioResult<void>(double)> volume;
    };
    using FailureCallback=std::function<void(const AudioFailure&)>;
    AudioResult<void> play(std::string_view key,int loops,std::chrono::milliseconds fade_in,const Backend& backend);
    AudioResult<void> transition(std::string_view key,const MusicTransitionOptions& options,const Backend& backend);
    AudioResult<void> stop(std::chrono::milliseconds fade_out,const Backend& backend);
    void update(double seconds,const Backend& backend,const FailureCallback& failed={});
    void reset();
    double gain() const { return _fade.gain(); }
private:
    struct Request { std::string key;int loops;std::chrono::milliseconds fade_in; };
    AudioResult<void> start(const Request& request,const Backend& backend);
    AudioResult<void> start_pending(const Backend& backend);
    std::string _key;
    std::optional<Request> _pending;
    AudioFade _fade;
    bool _stopping=false;
    std::optional<AudioError> _reported_error;
};
}
