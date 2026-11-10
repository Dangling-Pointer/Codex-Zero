#include "sound_playback_scheduler.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace elysia::audio
{
namespace
{
SoundHandle next_sound_handle()
{
    static std::atomic<SoundHandle> next{1};
    auto value=next.load(std::memory_order_relaxed);
    do {
        if(value==std::numeric_limits<SoundHandle>::max()) throw std::overflow_error("Sound handle space exhausted.");
    } while(!next.compare_exchange_weak(value,value+1,std::memory_order_relaxed));
    return value;
}
AudioFailure invalid_handle() { return audio_failure(AudioError::InvalidHandle,"Sound handle is no longer active."); }
}
AudioResult<void> SoundPlaybackScheduler::set_group_config(SoundGroup group,const SoundGroupConfig& config)
{
    if(sound_group_index(group)>=kSoundGroupCount || config.cooldown.count()<0 ||
        (config.max_simultaneous&&*config.max_simultaneous>sound_group_hard_limit(group)) ||
        (config.overflow_policy!=SoundOverflowPolicy::IgnoreNew&&config.overflow_policy!=SoundOverflowPolicy::ReplaceOldest))
        return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound group configuration."));
    _group_configs[sound_group_index(group)]=config;
    return {};
}
AudioResult<SoundGroupConfig> SoundPlaybackScheduler::group_config(SoundGroup group) const
{
    if(sound_group_index(group)>=kSoundGroupCount)
        return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound group."));
    return _group_configs[sound_group_index(group)];
}
AudioResult<SoundRequestResult> SoundPlaybackScheduler::request_sound(std::string_view key,const SoundPlayOptions& options,
    const StartSoundCallback& start,const ChannelPlayingCallback& playing,const StopSoundCallback& stop)
{
    if(key.empty()||!std::isfinite(options.gain)||options.gain<0||options.loops.value_or(0)<-1||sound_group_index(options.group)>=kSoundGroupCount)
        return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound playback parameters.",std::string(key)));
    const auto handle=next_sound_handle();
    if(options.start_delay.count()<=0)
    {
        auto result=try_start_sound(handle,key,options,start,playing,stop);
        if(!result) return std::unexpected(result.error());
        return SoundRequestResult{SoundRequestStatus::Started,handle};
    }
    _pending_sounds.push_back({handle,std::string(key),options,_elapsed_seconds+options.start_delay.count()/1000.0});
    return SoundRequestResult{SoundRequestStatus::Scheduled,handle};
}
void SoundPlaybackScheduler::update(double delta,const StartSoundCallback& start,const ChannelPlayingCallback& playing,
    const StopSoundCallback& stop,const VolumeCallback& volume,const FailureCallback& failed)
{
    delta=audio_delta(delta);_elapsed_seconds+=delta;
    prune_finished_sounds(playing);
    auto it=_active_sounds.begin();
    while(it!=_active_sounds.end())
    {
        auto fade=it->fade;
        if(!it->paused) fade.update(delta);
        AudioResult<void> result;
        if(volume) result=volume(it->channel,it->group,fade.gain()*it->gain);
        if(result) it->fade=fade;
        if(result&&it->stopping&&it->fade.finished()&&stop)
        {
            result=stop(it->channel);
            if(result) { it=_active_sounds.erase(it);continue; }
        }
        if(!result)
        {
            if(failed&&it->reported_error!=result.error().code) failed(result.error(),it->handle);
            it->reported_error=result.error().code;
        }
        else it->reported_error.reset();
        ++it;
    }
    auto pending=_pending_sounds.begin();
    while(pending!=_pending_sounds.end())
    {
        if(pending->paused) { pending->due_time_seconds+=delta;++pending;continue; }
        if(pending->due_time_seconds>_elapsed_seconds) { ++pending;continue; }
        auto result=try_start_sound(pending->handle,pending->key,pending->options,start,playing,stop);
        if(!result&&failed)
        {
            result.error().diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry("sound",pending->key));
            failed(result.error(),pending->handle);
        }
        pending=_pending_sounds.erase(pending);
    }
}
AudioResult<void> SoundPlaybackScheduler::stop_sound(SoundHandle handle,const ChannelPlayingCallback& playing,
    const StopSoundCallback& stop,std::chrono::milliseconds fade_out)
{
    auto pending=std::ranges::find(_pending_sounds,handle,&PendingSound::handle);
    if(pending!=_pending_sounds.end()) { _pending_sounds.erase(pending);return {}; }
    prune_finished_sounds(playing);
    auto sound=std::ranges::find(_active_sounds,handle,&ActiveSound::handle);
    if(sound==_active_sounds.end()) return std::unexpected(invalid_handle());
    if(!stop) return std::unexpected(audio_failure(AudioError::BackendFailure,"Missing sound stop backend."));
    if(fade_out.count()<=0||sound->paused)
    {
        auto result=stop(sound->channel);
        if(!result) return result;
        _active_sounds.erase(sound);
    }
    else if(!sound->stopping) { sound->stopping=true;sound->fade.start(0,fade_out); }
    return {};
}
AudioResult<void> SoundPlaybackScheduler::stop_all_sounds(const ChannelPlayingCallback& playing,
    const StopSoundCallback& stop,std::chrono::milliseconds fade_out)
{
    prune_finished_sounds(playing);
    std::vector<SoundHandle> handles;
    for(const auto& sound:_active_sounds) handles.push_back(sound.handle);
    AudioResult<void> result;
    for(auto handle:handles) collect_audio_failure(result,stop_sound(handle,playing,stop,fade_out));
    return result;
}
void SoundPlaybackScheduler::cancel_all_scheduled_sounds() { _pending_sounds.clear(); }
void SoundPlaybackScheduler::clear_active_sounds() { _active_sounds.clear(); }
void SoundPlaybackScheduler::for_each_active_channel(SoundGroup group,const ChannelPlayingCallback& playing,const ActiveChannelCallback& callback)
{
    prune_finished_sounds(playing);
    for(const auto& sound:_active_sounds) if(sound.group==group) callback(sound.channel,sound.fade.gain()*sound.gain);
}
void SoundPlaybackScheduler::reset()
{
    _active_sounds.clear();_pending_sounds.clear();_last_started_seconds_by_key.clear();_elapsed_seconds=0;
}
AudioResult<void> SoundPlaybackScheduler::try_start_sound(SoundHandle handle,std::string_view key,const SoundPlayOptions& options,
    const StartSoundCallback& start,const ChannelPlayingCallback& playing,const StopSoundCallback& stop)
{
    prune_finished_sounds(playing);
    const auto& config=_group_configs[sound_group_index(options.group)];
    auto last=_last_started_seconds_by_key.find(std::string(key));
    if(last!=_last_started_seconds_by_key.end()&&_elapsed_seconds-last->second<config.cooldown.count()/1000.0)
        return std::unexpected(audio_failure(AudioError::CooldownActive,"Sound cooldown is active.",std::string(key)));
    auto limit=config.max_simultaneous.value_or(sound_group_hard_limit(options.group));
    if(active_count(options.group)>=limit)
    {
        if(limit==0||config.overflow_policy==SoundOverflowPolicy::IgnoreNew||!stop)
            return std::unexpected(audio_failure(AudioError::GroupLimitReached,"Sound group capacity reached.",std::string(key)));
        auto oldest=std::ranges::find(_active_sounds,options.group,&ActiveSound::group);
        if(oldest==_active_sounds.end()) return std::unexpected(audio_failure(AudioError::GroupLimitReached,"Sound group has no replaceable voice.",std::string(key)));
        auto stopped=stop(oldest->channel);
        if(!stopped) return stopped;
        _active_sounds.erase(oldest);
    }
    if(_active_sounds.size()>=kSoundChannelCount)
        return std::unexpected(audio_failure(AudioError::ChannelLimitReached,"No sound channels available.",std::string(key)));
    AudioFade fade;fade.reset(options.fade_in.count()>0?0:1);fade.start(1,options.fade_in);
    if(!start) return std::unexpected(audio_failure(AudioError::BackendFailure,"Missing sound start backend.",std::string(key)));
    auto channel=start(key,options.loops.value_or(0),options.group,fade.gain()*options.gain);
    if(!channel) return std::unexpected(channel.error());
    if(*channel<0||*channel>=static_cast<int>(kSoundChannelCount))
        return std::unexpected(audio_failure(AudioError::BackendFailure,"Backend returned an invalid channel.",std::string(key)));
    try
    {
        _active_sounds.push_back({handle,*channel,options.group,fade,false,options.gain,false});
        _last_started_seconds_by_key[std::string(key)]=_elapsed_seconds;
    }
    catch(...)
    {
        std::erase_if(_active_sounds,[handle](const auto& sound){return sound.handle==handle;});
        if(stop) { auto stopped=stop(*channel);if(!stopped) report_audio_failure(stopped.error(),handle); }
        throw;
    }
    return {};
}
void SoundPlaybackScheduler::prune_finished_sounds(const ChannelPlayingCallback& playing)
{
    std::erase_if(_active_sounds,[&](const auto& sound){return !playing(sound.channel);});
}
std::size_t SoundPlaybackScheduler::active_count(SoundGroup group) const
{
    return std::ranges::count(_active_sounds,group,&ActiveSound::group);
}
AudioResult<void> SoundPlaybackScheduler::set_gain(SoundHandle handle,double gain,const ChannelPlayingCallback& playing,const VolumeCallback& volume)
{
    if(!std::isfinite(gain)||gain<0) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound gain."));
    for(auto& pending:_pending_sounds) if(pending.handle==handle) { pending.options.gain=gain;return {}; }
    prune_finished_sounds(playing);
    for(auto& sound:_active_sounds) if(sound.handle==handle)
    {
        if(volume) { auto result=volume(sound.channel,sound.group,sound.fade.gain()*gain);if(!result) return result; }
        sound.gain=gain;return {};
    }
    return std::unexpected(invalid_handle());
}
AudioResult<void> SoundPlaybackScheduler::set_paused(SoundHandle handle,bool paused,const ChannelPlayingCallback& playing,const PauseCallback& pause)
{
    for(auto& pending:_pending_sounds) if(pending.handle==handle) { pending.paused=paused;return {}; }
    prune_finished_sounds(playing);
    for(auto& sound:_active_sounds) if(sound.handle==handle)
    {
        if(sound.paused==paused) return {};
        if(!pause) return std::unexpected(audio_failure(AudioError::BackendFailure,"Missing sound pause backend."));
        auto result=pause(sound.channel,paused);
        if(!result) return result;
        sound.paused=paused;return {};
    }
    return std::unexpected(invalid_handle());
}
bool SoundPlaybackScheduler::is_active(SoundHandle handle,const ChannelPlayingCallback& playing)
{
    prune_finished_sounds(playing);
    return std::ranges::any_of(_active_sounds,[handle](const auto& s){return s.handle==handle;})||
        std::ranges::any_of(_pending_sounds,[handle](const auto& s){return s.handle==handle;});
}
}
