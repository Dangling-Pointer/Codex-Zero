#pragma once
#include "sound_playback_types.h"
#include "audio_fade.h"
#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::audio
{
class SoundPlaybackScheduler
{
public:
    using StartSoundCallback=std::function<AudioResult<int>(std::string_view,int,SoundGroup,double)>;
    using ChannelPlayingCallback=std::function<bool(int)>;
    using StopSoundCallback=std::function<AudioResult<void>(int)>;
    using ActiveChannelCallback=std::function<void(int,double)>;
    using VolumeCallback=std::function<AudioResult<void>(int,SoundGroup,double)>;
    using PauseCallback=std::function<AudioResult<void>(int,bool)>;
    using FailureCallback=std::function<void(const AudioFailure&,SoundHandle)>;

    AudioResult<void> set_group_config(SoundGroup group,const SoundGroupConfig& config);
    AudioResult<SoundGroupConfig> group_config(SoundGroup group) const;
    AudioResult<SoundRequestResult> request_sound(std::string_view key,const SoundPlayOptions& options,
        const StartSoundCallback& start,const ChannelPlayingCallback& playing,const StopSoundCallback& stop={});
    void update(double delta,const StartSoundCallback& start,const ChannelPlayingCallback& playing,
        const StopSoundCallback& stop={},const VolumeCallback& volume={},const FailureCallback& failed={});
    AudioResult<void> stop_sound(SoundHandle handle,const ChannelPlayingCallback& playing,
        const StopSoundCallback& stop,std::chrono::milliseconds fade_out={});
    AudioResult<void> stop_all_sounds(const ChannelPlayingCallback& playing,const StopSoundCallback& stop,
        std::chrono::milliseconds fade_out={});
    void cancel_all_scheduled_sounds();
    AudioResult<void> set_gain(SoundHandle handle,double gain,const ChannelPlayingCallback& playing,const VolumeCallback& volume);
    AudioResult<void> set_paused(SoundHandle handle,bool paused,const ChannelPlayingCallback& playing,const PauseCallback& pause);
    bool is_active(SoundHandle handle,const ChannelPlayingCallback& playing);
    void clear_active_sounds();
    void for_each_active_channel(SoundGroup group,const ChannelPlayingCallback& playing,const ActiveChannelCallback& callback);
    void reset();
private:
    struct ActiveSound
    {
        SoundHandle handle=0; int channel=-1; SoundGroup group=SoundGroup::Extra;
        AudioFade fade; bool stopping=false; double gain=1; bool paused=false;
        std::optional<AudioError> reported_error;
    };
    struct PendingSound
    {
        SoundHandle handle=0; std::string key; SoundPlayOptions options;
        double due_time_seconds=0; bool paused=false;
    };
    AudioResult<void> try_start_sound(SoundHandle handle,std::string_view key,const SoundPlayOptions& options,
        const StartSoundCallback& start,const ChannelPlayingCallback& playing,const StopSoundCallback& stop);
    void prune_finished_sounds(const ChannelPlayingCallback& playing);
    std::size_t active_count(SoundGroup group) const;
    std::array<SoundGroupConfig,kSoundGroupCount> _group_configs{};
    std::vector<ActiveSound> _active_sounds;
    std::vector<PendingSound> _pending_sounds;
    std::unordered_map<std::string,double> _last_started_seconds_by_key;
    double _elapsed_seconds=0;
};
}
