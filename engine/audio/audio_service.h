#pragma once
#include "audio_settings.h"
#include "mixer_backend.h"
#include "music_playback_controller.h"
#include "sound_playback_scheduler.h"
#include "scene_sound_types.h"
#include "../tools/singleton.h"
#include <array>
#include <string_view>

namespace elysia::scene { class Scene;class SceneAudioRuntime; }
#define ELYSIA_AUDIO (::elysia::audio::AudioService::instance())

namespace elysia::audio
{
class AudioService : public elysia::tools::Singleton<AudioService>
{
    friend elysia::tools::Singleton<AudioService>;
    friend class elysia::scene::Scene;
    friend class elysia::scene::SceneAudioRuntime;
public:
    [[nodiscard]] AudioResult<void> initialize(const AudioSettings& settings);
    void shutdown() noexcept;
    bool is_initialized() const noexcept { return _initialized; }
    bool scene_audio_available() const noexcept { return _scene_runtime!=nullptr; }
    [[nodiscard]] AudioResult<SoundRequestResult> request_sound(std::string_view key,const SoundPlayOptions& options={});
    [[nodiscard]] AudioResult<void> set_listener_position(elysia::core::Vector2 position);
    [[nodiscard]] AudioResult<elysia::core::Vector2> listener_position() const;
    [[nodiscard]] AudioResult<LoopSoundId> create_loop(const LoopSoundRequest& request);
    [[nodiscard]] AudioResult<void> set_loop_position(LoopSoundId id,elysia::core::Vector2 position);
    [[nodiscard]] AudioResult<void> set_loop_gain(LoopSoundId id,double gain);
    [[nodiscard]] AudioResult<void> set_loop_enabled(LoopSoundId id,bool enabled);
    [[nodiscard]] AudioResult<void> destroy_loop(LoopSoundId id);
    [[nodiscard]] AudioResult<LoopSoundState> loop_state(LoopSoundId id) const;
    [[nodiscard]] AudioResult<void> clear_scene_sounds();
    void update(double delta_seconds);
    [[nodiscard]] AudioResult<void> stop_sound(SoundHandle handle,std::chrono::milliseconds fade_out={});
    [[nodiscard]] AudioResult<void> set_sound_gain(SoundHandle handle,double gain);
    [[nodiscard]] AudioResult<void> set_sound_paused(SoundHandle handle,bool paused);
    bool sound_active(SoundHandle handle);
    void cancel_all_scheduled_sounds();
    [[nodiscard]] AudioResult<void> set_sound_group_config(SoundGroup group,const SoundGroupConfig& config);
    [[nodiscard]] AudioResult<SoundGroupConfig> sound_group_config(SoundGroup group) const;
    [[nodiscard]] AudioResult<void> set_sound_group_volume(SoundGroup group,int volume);
    [[nodiscard]] AudioResult<int> sound_group_volume(SoundGroup group) const;
    [[nodiscard]] AudioResult<void> play_music(std::string_view key,int loops=-1,std::chrono::milliseconds fade_in={});
    [[nodiscard]] AudioResult<void> transition_music(std::string_view key,const MusicTransitionOptions& options={});
    [[nodiscard]] AudioResult<void> stop_music(std::chrono::milliseconds fade_out={});
    [[nodiscard]] AudioResult<void> stop_all_sounds(std::chrono::milliseconds fade_out={});
    void set_master_volume(int volume);
    void set_music_volume(int volume);
    void set_sound_volume(int volume);
    const AudioSettings& settings() const;
private:
    void attach_runtime(elysia::scene::SceneAudioRuntime& runtime);
    void detach_runtime(elysia::scene::SceneAudioRuntime& runtime) noexcept;
    AudioResult<SoundRequestResult> request_sound_impl(std::string_view key,const SoundPlayOptions& options);
    AudioResult<void> set_sound_gain_impl(SoundHandle handle,double gain);
    AudioResult<void> set_sound_paused_impl(SoundHandle handle,bool paused);
    bool channel_playing(int channel) const;
    AudioResult<void> halt_channel(int channel);
    AudioResult<int> start_sound(std::string_view key,int loops,SoundGroup group,double gain);
    MusicPlaybackController::Backend music_backend();
    AudioResult<void> apply_music_volume(double gain) const;
    void apply_volumes();
    AudioResult<void> apply_sound_group_volume(SoundGroup group);
    AudioResult<void> apply_sound_channel_volume(int channel,SoundGroup group,double gain) const;
    static int clamp_volume(int volume);
    MIX_Track*& _music_track=detail::mixer_backend().music;
    std::array<MIX_Track*,kSoundChannelCount>& _tracks=detail::mixer_backend().tracks;
    AudioSettings _settings{};
    SoundPlaybackScheduler _sound_scheduler;
    MusicPlaybackController _music_controller;
    std::array<int,kSoundGroupCount> _sound_group_volumes{100,100,100,100};
    bool _initialized=false;
    elysia::scene::SceneAudioRuntime* _scene_runtime=nullptr;
};
}
