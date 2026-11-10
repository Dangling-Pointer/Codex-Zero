#include "audio_service.h"
#include "audio_validation.h"
#include "../resources/resource_service.h"
#include "../scene/runtime/scene_audio_runtime.h"
#include <algorithm>
#include <limits>

namespace elysia::audio
{
namespace
{
AudioFailure not_initialized() { return audio_failure(AudioError::NotInitialized,"Audio service is not initialized."); }
void report_result(const AudioResult<void>& result) { if(!result) report_audio_failure(result.error()); }
AudioResult<void> validate_music(std::string_view key,int loops)
{
    if(key.empty()||loops<-1) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid music request.",std::string(key)));
    if(!ELYSIA_RESOURCES->has_music(key)) return std::unexpected(audio_failure(AudioError::MissingResource,"Music resource is not loaded.",std::string(key)));
    return {};
}
}
AudioResult<void> AudioService::initialize(const AudioSettings& settings)
{
    if(_initialized) shutdown();
    auto result=detail::mixer_backend().initialize();
    if(!result) return result;
    _music_controller.reset();_sound_scheduler.reset();
    const auto previous_settings=_settings;
    const auto previous_groups=_sound_group_volumes;
    _settings={clamp_volume(settings.master_volume),clamp_volume(settings.music_volume),clamp_volume(settings.sound_volume)};
    _sound_group_volumes.fill(100);
    result=apply_music_volume(_music_controller.gain());
    if(!result)
    {
        _settings=previous_settings;_sound_group_volumes=previous_groups;
        return result;
    }
    _initialized=true;
    return {};
}
void AudioService::shutdown() noexcept
{
    if(_scene_runtime) _scene_runtime->clear();
    if(!_initialized) return;
    try { report_result(stop_music());report_result(stop_all_sounds()); }
    catch(...)
    {
        for(auto* track:_tracks) if(track) { (void)MIX_StopTrack(track,0);(void)MIX_SetTrackAudio(track,nullptr); }
        if(_music_track) { (void)MIX_StopTrack(_music_track,0);(void)MIX_SetTrackAudio(_music_track,nullptr); }
    }
    _sound_scheduler.reset();_music_controller.reset();_initialized=false;
}
AudioResult<SoundRequestResult> AudioService::request_sound(std::string_view key,const SoundPlayOptions& options)
{
    if(auto validation=validate_sound_options(key,options);!validation) return std::unexpected(validation.error());
    if(!_initialized) return std::unexpected(not_initialized());
    if(options.scope==SoundScope::Scene)
    {
        if(!_scene_runtime) return std::unexpected(audio_failure(AudioError::NoActiveScene,"No attached scene audio context.",std::string(key)));
        return _scene_runtime->request_sound(key,options);
    }
    return request_sound_impl(key,options);
}
AudioResult<SoundRequestResult> AudioService::request_sound_impl(std::string_view key,const SoundPlayOptions& options)
{
    if(!_initialized) return std::unexpected(not_initialized());
    if(!ELYSIA_RESOURCES->has_sound(key)) return std::unexpected(audio_failure(AudioError::MissingResource,"Sound resource is not loaded.",std::string(key)));
    return _sound_scheduler.request_sound(key,options,
        [this](std::string_view k,int loops,SoundGroup group,double gain){return start_sound(k,loops,group,gain);},
        [this](int c){return channel_playing(c);},[this](int c){return halt_channel(c);});
}
void AudioService::update(double delta)
{
    if(!_initialized) return;
    _sound_scheduler.update(delta,
        [this](std::string_view k,int loops,SoundGroup group,double gain){return start_sound(k,loops,group,gain);},
        [this](int c){return channel_playing(c);},[this](int c){return halt_channel(c);},
        [this](int c,SoundGroup group,double gain){return apply_sound_channel_volume(c,group,gain);},
        [](const AudioFailure& failure,SoundHandle handle){report_audio_failure(failure,handle);});
    _music_controller.update(delta,music_backend(),[](const AudioFailure& failure){report_audio_failure(failure);});
}
AudioResult<void> AudioService::set_sound_gain(SoundHandle handle,double gain)
{
    if(_scene_runtime&&_scene_runtime->owns_sound(handle)) return _scene_runtime->set_sound_gain(handle,gain);
    return set_sound_gain_impl(handle,gain);
}
AudioResult<void> AudioService::set_sound_gain_impl(SoundHandle handle,double gain)
{
    if(!_initialized) return std::unexpected(not_initialized());
    return _sound_scheduler.set_gain(handle,gain,[this](int c){return channel_playing(c);},
        [this](int c,SoundGroup group,double value){return apply_sound_channel_volume(c,group,value);});
}
AudioResult<void> AudioService::set_sound_paused(SoundHandle handle,bool paused)
{
    if(_scene_runtime&&_scene_runtime->owns_sound(handle)) return _scene_runtime->set_sound_paused(handle,paused);
    return set_sound_paused_impl(handle,paused);
}
AudioResult<void> AudioService::set_sound_paused_impl(SoundHandle handle,bool paused)
{
    if(!_initialized) return std::unexpected(not_initialized());
    return _sound_scheduler.set_paused(handle,paused,[this](int c){return channel_playing(c);},
        [this](int c,bool value)->AudioResult<void>{
            if(value?MIX_PauseTrack(_tracks[c]):MIX_ResumeTrack(_tracks[c])) return {};
            return std::unexpected(detail::mixer_failure(value?"Pause sound":"Resume sound"));
        });
}
bool AudioService::sound_active(SoundHandle handle)
{
    return _initialized&&_sound_scheduler.is_active(handle,[this](int c){return channel_playing(c);});
}
AudioResult<void> AudioService::stop_sound(SoundHandle handle,std::chrono::milliseconds fade)
{
    if(!_initialized) return std::unexpected(not_initialized());
    return _sound_scheduler.stop_sound(handle,[this](int c){return channel_playing(c);},[this](int c){return halt_channel(c);},fade);
}
void AudioService::cancel_all_scheduled_sounds() { _sound_scheduler.cancel_all_scheduled_sounds(); }
AudioResult<void> AudioService::set_sound_group_config(SoundGroup group,const SoundGroupConfig& config) { return _sound_scheduler.set_group_config(group,config); }
AudioResult<SoundGroupConfig> AudioService::sound_group_config(SoundGroup group) const { return _sound_scheduler.group_config(group); }
AudioResult<void> AudioService::set_sound_group_volume(SoundGroup group,int volume)
{
    if(sound_group_index(group)>=kSoundGroupCount) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound group."));
    auto& cached=_sound_group_volumes[sound_group_index(group)];const auto old=cached;
    cached=clamp_volume(volume);auto result=apply_sound_group_volume(group);
    if(!result) { cached=old;collect_audio_failure(result,apply_sound_group_volume(group)); }
    return result;
}
AudioResult<int> AudioService::sound_group_volume(SoundGroup group) const
{
    if(sound_group_index(group)>=kSoundGroupCount) return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound group."));
    return _sound_group_volumes[sound_group_index(group)];
}
AudioResult<void> AudioService::play_music(std::string_view key,int loops,std::chrono::milliseconds fade)
{
    if(!_initialized) return std::unexpected(not_initialized());
    if(auto valid=validate_music(key,loops);!valid) return valid;
    return _music_controller.play(key,loops,fade,music_backend());
}
AudioResult<void> AudioService::transition_music(std::string_view key,const MusicTransitionOptions& options)
{
    if(!_initialized) return std::unexpected(not_initialized());
    if(auto valid=validate_music(key,options.loops);!valid) return valid;
    return _music_controller.transition(key,options,music_backend());
}
AudioResult<void> AudioService::stop_music(std::chrono::milliseconds fade)
{
    if(!_initialized) return std::unexpected(not_initialized());
    return _music_controller.stop(fade,music_backend());
}
AudioResult<void> AudioService::stop_all_sounds(std::chrono::milliseconds fade)
{
    if(!_initialized) return std::unexpected(not_initialized());
    return _sound_scheduler.stop_all_sounds([this](int c){return channel_playing(c);},[this](int c){return halt_channel(c);},fade);
}
MusicPlaybackController::Backend AudioService::music_backend()
{
    return {
        [this](std::string_view key,int loops,double gain)->AudioResult<void>{
            if(!ELYSIA_RESOURCES->has_music(key)) return std::unexpected(audio_failure(AudioError::MissingResource,"Music resource is no longer loaded.",std::string(key)));
            // Prepare a replacement separately: a decoder/start failure must leave the
            // current track, playback position and controller state intact.
            auto* candidate=MIX_CreateTrack(detail::mixer_backend().mixer);
            if(!candidate) return std::unexpected(detail::mixer_failure("Create music playback track"));
            AudioResult<void> result;
            if(!MIX_SetTrackGain(candidate,float(_settings.master_volume*_settings.music_volume/10000.0*gain)))
                result=std::unexpected(detail::mixer_failure("Set initial music gain"));
            if(result) result=detail::MixerBackend::play(candidate,ELYSIA_RESOURCES->find_music(key),loops);
            if(!result)
            {
                MIX_DestroyTrack(candidate);
                result.error().diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry("music",std::string(key)));
                return result;
            }
            auto* previous=_music_track;
            _music_track=candidate;
            MIX_DestroyTrack(previous);
            return {};
        },
        [this]{return detail::MixerBackend::stop(_music_track);},
        [this]{return _music_track&&MIX_TrackPlaying(_music_track);},
        [this](double gain){return apply_music_volume(gain);}
    };
}
AudioResult<void> AudioService::apply_music_volume(double gain) const
{
    if(MIX_SetTrackGain(_music_track,float(_settings.master_volume*_settings.music_volume/10000.0*gain))) return {};
    return std::unexpected(detail::mixer_failure("Set music gain"));
}
void AudioService::set_master_volume(int value) { _settings.master_volume=clamp_volume(value);apply_volumes(); }
void AudioService::set_music_volume(int value) { _settings.music_volume=clamp_volume(value);apply_volumes(); }
void AudioService::set_sound_volume(int value) { _settings.sound_volume=clamp_volume(value);apply_volumes(); }
const AudioSettings& AudioService::settings() const { return _settings; }
AudioResult<int> AudioService::start_sound(std::string_view key,int loops,SoundGroup group,double gain)
{
    if(!ELYSIA_RESOURCES->has_sound(key)) return std::unexpected(audio_failure(AudioError::MissingResource,"Sound resource is no longer loaded.",std::string(key)));
    int channel=0;while(channel<int(kSoundChannelCount)&&channel_playing(channel)) ++channel;
    if(channel==int(kSoundChannelCount)) return std::unexpected(audio_failure(AudioError::ChannelLimitReached,"No free backend sound track.",std::string(key)));
    auto result=apply_sound_channel_volume(channel,group,gain);if(!result) return std::unexpected(result.error());
    result=detail::MixerBackend::play(_tracks[channel],ELYSIA_RESOURCES->find_sound(key),loops);
    if(!result) { result.error().diagnostic.entries.push_back(elysia::core::make_failure_diagnostic_entry("sound",std::string(key)));return std::unexpected(result.error()); }
    return channel;
}
void AudioService::apply_volumes()
{
    if(!_initialized) return;
    report_result(apply_music_volume(_music_controller.gain()));
    for(std::size_t i=0;i<kSoundGroupCount;++i) report_result(apply_sound_group_volume(static_cast<SoundGroup>(i)));
}
AudioResult<void> AudioService::apply_sound_group_volume(SoundGroup group)
{
    AudioResult<void> result;
    if(!_initialized) return result;
    _sound_scheduler.for_each_active_channel(group,[this](int c){return channel_playing(c);},
        [&](int c,double gain){collect_audio_failure(result,apply_sound_channel_volume(c,group,gain));});
    return result;
}
AudioResult<void> AudioService::apply_sound_channel_volume(int channel,SoundGroup group,double gain) const
{
    const double scale=_settings.master_volume*_settings.sound_volume*_sound_group_volumes[sound_group_index(group)]/1000000.0;
    const auto value=float(std::min(scale*gain,double(std::numeric_limits<float>::max())));
    if(MIX_SetTrackGain(_tracks[channel],value)) return {};
    return std::unexpected(detail::mixer_failure("Set sound gain"));
}
int AudioService::clamp_volume(int value) { return std::clamp(value,0,100); }
bool AudioService::channel_playing(int channel) const
{
    return channel>=0&&channel<int(_tracks.size())&&_tracks[channel]&&(MIX_TrackPlaying(_tracks[channel])||MIX_TrackPaused(_tracks[channel]));
}
AudioResult<void> AudioService::halt_channel(int channel)
{
    if(channel<0||channel>=int(_tracks.size())) return std::unexpected(audio_failure(AudioError::BackendFailure,"Invalid backend track index."));
    return detail::MixerBackend::stop(_tracks[channel]);
}
}
