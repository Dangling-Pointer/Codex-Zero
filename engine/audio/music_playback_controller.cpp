#include "music_playback_controller.h"

namespace elysia::audio
{
void MusicPlaybackController::reset()
{
    _key.clear();_pending.reset();_fade.reset();_stopping=false;_reported_error.reset();
}
AudioResult<void> MusicPlaybackController::start(const Request& request,const Backend& backend)
{
    AudioFade fade;fade.reset(request.fade_in.count()>0?0:1);fade.start(1,request.fade_in);
    auto result=backend.start(request.key,request.loops,fade.gain());
    if(!result) return result;
    _key=request.key;_fade=fade;_stopping=false;_pending.reset();_reported_error.reset();
    return {};
}
AudioResult<void> MusicPlaybackController::start_pending(const Backend& backend)
{
    if(!_pending) { reset();return {}; }
    const auto request=*_pending;
    auto result=start(request,backend);
    if(!result) { collect_audio_failure(result,backend.stop());reset(); }
    return result;
}
AudioResult<void> MusicPlaybackController::play(std::string_view key,int loops,std::chrono::milliseconds fade_in,const Backend& backend)
{
    if(key.empty()||loops<-1) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid music request.",std::string(key)));
    return start({std::string(key),loops,fade_in},backend);
}
AudioResult<void> MusicPlaybackController::transition(std::string_view key,const MusicTransitionOptions& options,const Backend& backend)
{
    if(key.empty()||options.loops<-1) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid music transition.",std::string(key)));
    if(!backend.playing()) return start({std::string(key),options.loops,options.fade_in},backend);
    if(_key==key)
    {
        auto fade=_fade;fade.start(1,options.fade_in);
        auto result=backend.volume(fade.gain());if(!result) return result;
        _pending.reset();_stopping=false;_fade=fade;return {};
    }
    auto fade=_fade;
    if(!_stopping) fade.start(0,options.fade_out);
    if(fade.finished()) return start({std::string(key),options.loops,options.fade_in},backend);
    _pending=Request{std::string(key),options.loops,options.fade_in};_fade=fade;_stopping=true;
    return {};
}
AudioResult<void> MusicPlaybackController::stop(std::chrono::milliseconds fade_out,const Backend& backend)
{
    if(fade_out.count()<=0||!backend.playing())
    {
        auto result=backend.stop();if(!result) return result;
        reset();return {};
    }
    _pending.reset();
    if(!_stopping) { _stopping=true;_fade.start(0,fade_out); }
    return {};
}
void MusicPlaybackController::update(double seconds,const Backend& backend,const FailureCallback& failed)
{
    if(_key.empty()&&!_pending&&!_stopping) return;
    auto notify=[&](const AudioFailure& failure){if(failed&&_reported_error!=failure.code) failed(failure);_reported_error=failure.code;};
    if(!backend.playing()) { auto result=start_pending(backend);if(!result) notify(result.error());return; }
    auto fade=_fade;fade.update(seconds);
    auto result=backend.volume(fade.gain());
    if(!result) { notify(result.error());return; }
    _fade=fade;
    if(_stopping&&_fade.finished())
    {
        if(_pending) result=start_pending(backend);
        else { result=backend.stop();if(result) reset(); }
        if(!result) notify(result.error());
        else _reported_error.reset();
    }
    else _reported_error.reset();
}
}
