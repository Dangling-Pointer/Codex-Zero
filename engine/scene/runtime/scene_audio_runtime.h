#pragma once
#include "scene_audio_config.h"
#include "../../audio/scene_sound_types.h"
#include <string_view>
#include <vector>

namespace elysia::scene
{
// Internal runtime owned by Scene; gameplay uses AudioService.
class SceneAudioRuntime final
{
public:
    explicit SceneAudioRuntime(SceneAudioConfig config={});
    ~SceneAudioRuntime();
    SceneAudioRuntime(const SceneAudioRuntime&)=delete;
    SceneAudioRuntime& operator=(const SceneAudioRuntime&)=delete;
    elysia::audio::AudioResult<void> set_listener_position(elysia::core::Vector2 position);
    elysia::core::Vector2 listener_position() const noexcept { return _listener; }
    elysia::audio::AudioResult<elysia::audio::SoundRequestResult> request_sound(std::string_view key,const elysia::audio::SoundPlayOptions& options={});
    elysia::audio::AudioResult<void> set_sound_gain(elysia::audio::SoundHandle handle,double gain);
    elysia::audio::AudioResult<void> set_sound_paused(elysia::audio::SoundHandle handle,bool paused);
    bool owns_sound(elysia::audio::SoundHandle handle) const noexcept;
    elysia::audio::AudioResult<elysia::audio::LoopSoundId> create_loop(const elysia::audio::LoopSoundRequest& request);
    elysia::audio::AudioResult<void> set_loop_position(elysia::audio::LoopSoundId id,elysia::core::Vector2 position);
    elysia::audio::AudioResult<void> set_loop_gain(elysia::audio::LoopSoundId id,double gain);
    elysia::audio::AudioResult<void> set_loop_enabled(elysia::audio::LoopSoundId id,bool enabled);
    elysia::audio::AudioResult<void> destroy_loop(elysia::audio::LoopSoundId id);
    elysia::audio::AudioResult<elysia::audio::LoopSoundState> loop_state(elysia::audio::LoopSoundId id) const;
    void update(double delta);
    void set_paused(bool paused);
    elysia::audio::AudioResult<void> clear_sounds();
    void clear() noexcept;
private:
    struct OwnedSound
    {
        elysia::audio::SoundHandle handle=0;
        double gain=1;
        elysia::audio::SoundPausePolicy pause_policy=elysia::audio::SoundPausePolicy::FollowScene;
        std::optional<elysia::audio::SpatialSoundConfig> spatial;
        bool paused=false;
        std::optional<elysia::audio::AudioError> reported_error;
    };
    struct Loop
    {
        elysia::audio::LoopSoundId id;
        elysia::audio::LoopSoundRequest request;
        elysia::audio::LoopSoundState state;
        double next_attempt=0;
    };
    elysia::audio::AudioResult<void> refresh_sound(const OwnedSound& sound);
    elysia::audio::AudioResult<void> refresh_loop(Loop& loop,bool report=true);
    elysia::audio::AudioResult<void> change_loop(Loop& loop,const elysia::audio::LoopSoundRequest& request);
    void loop_failure(Loop& loop,const elysia::audio::AudioFailure& failure);
    Loop* find_loop(elysia::audio::LoopSoundId id);
    double distance_to(elysia::core::Vector2 position) const noexcept;
    SceneAudioConfig _config;
    elysia::core::Vector2 _listener{};
    std::vector<OwnedSound> _sounds;
    std::vector<Loop> _loops;
    double _elapsed=0;
    bool _paused=false;
};
}
