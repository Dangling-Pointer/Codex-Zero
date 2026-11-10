#include "scene_audio_runtime.h"
#include "../../audio/audio_service.h"
#include "../../audio/audio_validation.h"
#include "../../resources/resource_service.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace elysia::scene
{
using namespace elysia::audio;
namespace
{
bool valid_position(elysia::core::Vector2 p) { return std::isfinite(p.x)&&std::isfinite(p.y); }
bool valid_gain(double gain) { return std::isfinite(gain)&&gain>=0; }
AudioFailure invalid_handle() { return audio_failure(AudioError::InvalidHandle,"No matching owned sound or loop source."); }
LoopSoundId next_loop_id()
{
    static std::atomic<std::uint64_t> next{1};auto value=next.load(std::memory_order_relaxed);
    do { if(value==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Loop sound identifier space exhausted."); }
    while(!next.compare_exchange_weak(value,value+1,std::memory_order_relaxed));return {value};
}
}
SceneAudioRuntime::SceneAudioRuntime(SceneAudioConfig config):_config(config)
{
    if(!std::isfinite(config.loop_retry_interval_seconds)||config.loop_retry_interval_seconds<.25)
        throw std::invalid_argument("Scene audio loop retry interval must be finite and at least 0.25 seconds.");
}
SceneAudioRuntime::~SceneAudioRuntime() { clear(); }
AudioResult<void> SceneAudioRuntime::set_listener_position(elysia::core::Vector2 position)
{
    if(!valid_position(position)) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid listener position."));
    _listener=position;return {};
}
double SceneAudioRuntime::distance_to(elysia::core::Vector2 position) const noexcept
{
    return std::hypot(double(position.x)-_listener.x,double(position.y)-_listener.y);
}
AudioResult<SoundRequestResult> SceneAudioRuntime::request_sound(std::string_view key,const SoundPlayOptions& options)
{
    if(auto valid=validate_sound_options(key,options);!valid) return std::unexpected(valid.error());
    if(options.scope!=SoundScope::Scene) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Owned runtime requires scene scope."));
    OwnedSound sound{.gain=options.gain,.pause_policy=options.pause_policy,.spatial=options.spatial};
    auto prepared=options;prepared.spatial.reset();
    if(sound.spatial) prepared.gain*=sound_distance_gain(distance_to(sound.spatial->position),sound.spatial->distance);
    if(_paused&&sound.pause_policy==SoundPausePolicy::FollowScene) prepared.gain=0;
    _sounds.push_back(sound);
    try
    {
        auto result=ELYSIA_AUDIO->request_sound_impl(key,prepared);
        if(!result) { _sounds.pop_back();return result; }
        _sounds.back().handle=result->handle;
        auto applied=refresh_sound(_sounds.back());
        if(!applied)
        {
            collect_audio_failure(applied,ELYSIA_AUDIO->stop_sound(result->handle));
            _sounds.pop_back();return std::unexpected(applied.error());
        }
        return result;
    }
    catch(...)
    {
        if(_sounds.back().handle) (void)ELYSIA_AUDIO->stop_sound(_sounds.back().handle);
        _sounds.pop_back();throw;
    }
}
AudioResult<void> SceneAudioRuntime::refresh_sound(const OwnedSound& sound)
{
    const bool paused=sound.paused||(_paused&&sound.pause_policy==SoundPausePolicy::FollowScene);
    if(paused) { auto r=ELYSIA_AUDIO->set_sound_paused_impl(sound.handle,true);if(!r) return r; }
    const double gain=sound.gain*(sound.spatial?sound_distance_gain(distance_to(sound.spatial->position),sound.spatial->distance):1);
    auto r=ELYSIA_AUDIO->set_sound_gain_impl(sound.handle,gain);if(!r) return r;
    return paused?AudioResult<void>{}:ELYSIA_AUDIO->set_sound_paused_impl(sound.handle,false);
}
AudioResult<void> SceneAudioRuntime::set_sound_gain(SoundHandle handle,double gain)
{
    if(!valid_gain(gain)) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound gain."));
    auto it=std::ranges::find(_sounds,handle,&OwnedSound::handle);
    if(it==_sounds.end()||!ELYSIA_AUDIO->sound_active(handle)) return std::unexpected(invalid_handle());
    auto effective=gain*(it->spatial?sound_distance_gain(distance_to(it->spatial->position),it->spatial->distance):1);
    auto result=ELYSIA_AUDIO->set_sound_gain_impl(handle,effective);
    if(result) it->gain=gain;
    return result;
}
bool SceneAudioRuntime::owns_sound(SoundHandle handle) const noexcept
{
    return std::ranges::any_of(_sounds,[handle](const auto& sound){return sound.handle==handle;});
}
AudioResult<void> SceneAudioRuntime::set_sound_paused(SoundHandle handle,bool paused)
{
    auto it=std::ranges::find(_sounds,handle,&OwnedSound::handle);
    if(it==_sounds.end()||!ELYSIA_AUDIO->sound_active(handle)) return std::unexpected(invalid_handle());
    auto result=ELYSIA_AUDIO->set_sound_paused_impl(handle,paused||(_paused&&it->pause_policy==SoundPausePolicy::FollowScene));
    if(result) it->paused=paused;
    return result;
}
AudioResult<LoopSoundId> SceneAudioRuntime::create_loop(const LoopSoundRequest& request)
{
    SoundPlayOptions options{.group=request.group,.gain=request.gain,.pause_policy=request.pause_policy,
        .spatial=SpatialSoundConfig{request.position,request.distance}};
    if(auto valid=validate_sound_options(request.key,options);!valid) return std::unexpected(valid.error());
    if(!ELYSIA_AUDIO->is_initialized()) return std::unexpected(audio_failure(AudioError::NotInitialized,"Audio service is not initialized."));
    auto id=next_loop_id();_loops.push_back({.id=id,.request=request});
    try { (void)refresh_loop(_loops.back()); } // Acquisition failures are retained in last_error.
    catch(...) { if(_loops.back().state.voice) (void)ELYSIA_AUDIO->stop_sound(*_loops.back().state.voice);_loops.pop_back();throw; }
    return id;
}
void SceneAudioRuntime::loop_failure(Loop& loop,const AudioFailure& failure)
{
    if(!loop.state.last_error||loop.state.last_error->code!=failure.code||loop.state.last_error->diagnostic.message!=failure.diagnostic.message)
        report_audio_failure(failure,loop.state.voice.value_or(0));
    loop.state.last_error=failure;
}
AudioResult<void> SceneAudioRuntime::refresh_loop(Loop& loop,bool report)
{
    auto* audio=ELYSIA_AUDIO;
    if(loop.state.voice&&!audio->sound_active(*loop.state.voice)) loop.state.voice.reset();
    const double distance=distance_to(loop.request.position);
    const double gain=loop.request.gain*sound_distance_gain(distance,loop.request.distance);
    const bool scene_paused=_paused&&loop.request.pause_policy==SoundPausePolicy::FollowScene;
    const bool paused=scene_paused||!loop.request.enabled||gain<=0;
    if(distance>loop.request.distance.release_distance)
    {
        loop.state.status=LoopSoundStatus::OutOfRange;
        if(loop.state.voice)
        {
            auto result=audio->stop_sound(*loop.state.voice);
            if(!result) { if(report) loop_failure(loop,result.error());return result; }
            loop.state.voice.reset();
        }
        loop.state.last_error.reset();return {};
    }
    loop.state.status=!loop.request.enabled?LoopSoundStatus::Disabled:scene_paused?LoopSoundStatus::ScenePaused:
        gain<=0?LoopSoundStatus::Inaudible:LoopSoundStatus::WaitingForVoice;
    if(!loop.state.voice&&!paused)
    {
        if(!audio->is_initialized()) { loop_failure(loop,audio_failure(AudioError::NotInitialized,"Audio service is not initialized."));return {}; }
        if(!ELYSIA_RESOURCES->has_sound(loop.request.key))
        {
            loop.state.status=LoopSoundStatus::MissingResource;
            loop_failure(loop,audio_failure(AudioError::MissingResource,"Loop sound resource is not loaded.",loop.request.key));return {};
        }
        if(_elapsed>=loop.next_attempt)
        {
            loop.next_attempt=_elapsed+_config.loop_retry_interval_seconds;
            auto result=audio->request_sound_impl(loop.request.key,{.loops=-1,.group=loop.request.group,.gain=gain});
            if(!result) { loop_failure(loop,result.error());return {}; }
            loop.state.voice=result->handle;
        }
        else return {};
    }
    if(loop.state.voice)
    {
        AudioResult<void> result;
        if(paused) result=audio->set_sound_paused_impl(*loop.state.voice,true);
        if(result) result=audio->set_sound_gain_impl(*loop.state.voice,gain);
        if(result&&!paused) result=audio->set_sound_paused_impl(*loop.state.voice,false);
        if(!result) { if(report) loop_failure(loop,result.error());return result; }
        if(!paused) loop.state.status=LoopSoundStatus::Playing;
    }
    loop.state.last_error.reset();
    return {};
}
AudioResult<void> SceneAudioRuntime::change_loop(Loop& loop,const LoopSoundRequest& request)
{
    auto candidate=loop;
    candidate.request=request;
    auto result=refresh_loop(candidate,false);
    if(result) loop=std::move(candidate);
    else collect_audio_failure(result,refresh_loop(loop,false));
    return result;
}
SceneAudioRuntime::Loop* SceneAudioRuntime::find_loop(LoopSoundId id)
{
    auto it=std::ranges::find(_loops,id,&Loop::id);return it==_loops.end()?nullptr:&*it;
}
AudioResult<void> SceneAudioRuntime::set_loop_position(LoopSoundId id,elysia::core::Vector2 position)
{
    if(!valid_position(position)) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid source position."));
    auto* loop=find_loop(id);if(!loop) return std::unexpected(invalid_handle());
    auto request=loop->request;request.position=position;
    return change_loop(*loop,request);
}
AudioResult<void> SceneAudioRuntime::set_loop_gain(LoopSoundId id,double gain)
{
    if(!valid_gain(gain)) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid source gain."));
    auto* loop=find_loop(id);if(!loop) return std::unexpected(invalid_handle());
    auto request=loop->request;request.gain=gain;
    return change_loop(*loop,request);
}
AudioResult<void> SceneAudioRuntime::set_loop_enabled(LoopSoundId id,bool enabled)
{
    auto* loop=find_loop(id);if(!loop) return std::unexpected(invalid_handle());
    auto request=loop->request;request.enabled=enabled;
    return change_loop(*loop,request);
}
AudioResult<void> SceneAudioRuntime::destroy_loop(LoopSoundId id)
{
    auto it=std::ranges::find(_loops,id,&Loop::id);if(it==_loops.end()) return std::unexpected(invalid_handle());
    AudioResult<void> result;
    if(it->state.voice&&ELYSIA_AUDIO->sound_active(*it->state.voice)) result=ELYSIA_AUDIO->stop_sound(*it->state.voice);
    _loops.erase(it);return result;
}
AudioResult<LoopSoundState> SceneAudioRuntime::loop_state(LoopSoundId id) const
{
    auto it=std::ranges::find(_loops,id,&Loop::id);
    if(it==_loops.end()) return std::unexpected(invalid_handle());
    return it->state;
}
void SceneAudioRuntime::update(double delta)
{
    _elapsed+=audio_delta(delta);
    std::erase_if(_sounds,[](const auto& s){return !ELYSIA_AUDIO->sound_active(s.handle);});
    for(auto& sound:_sounds)
    {
        auto result=refresh_sound(sound);
        if(!result) { if(sound.reported_error!=result.error().code) report_audio_failure(result.error(),sound.handle);sound.reported_error=result.error().code; }
        else sound.reported_error.reset();
    }
    for(auto& loop:_loops) (void)refresh_loop(loop); // Each refresh records its asynchronous diagnostic.
}
void SceneAudioRuntime::set_paused(bool paused) { _paused=paused;update(0); }
AudioResult<void> SceneAudioRuntime::clear_sounds()
{
    AudioResult<void> result;
    for(const auto& sound:_sounds) if(ELYSIA_AUDIO->sound_active(sound.handle)) collect_audio_failure(result,ELYSIA_AUDIO->stop_sound(sound.handle));
    for(const auto& loop:_loops) if(loop.state.voice&&ELYSIA_AUDIO->sound_active(*loop.state.voice)) collect_audio_failure(result,ELYSIA_AUDIO->stop_sound(*loop.state.voice));
    _sounds.clear();_loops.clear();_elapsed=0;return result;
}
void SceneAudioRuntime::clear() noexcept
{
    try { auto result=clear_sounds();if(!result) report_audio_failure(result.error()); } catch(...) { _sounds.clear();_loops.clear(); }
    _listener={};_elapsed=0;
}
}
