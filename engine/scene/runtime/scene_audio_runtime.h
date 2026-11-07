#pragma once

#include "scene_audio_config.h"
#include "../../audio/sound_playback_types.h"
#include "../../audio/sound_distance.h"
#include "../../core/geometry/vector2.h"
#include <optional>
#include <string>
#include <vector>

namespace elysia::scene
{
enum class SceneSoundPausePolicy { FollowScene, ContinueWhenPaused };

struct LoopSoundId
{
    std::uint64_t value = 0;
    friend bool operator==(const LoopSoundId&,const LoopSoundId&) = default;
};

struct LoopSoundRequest
{
    std::string key;
    elysia::core::Vector2 position{};
    elysia::audio::SoundDistanceConfig distance;
    double gain = 1;
    elysia::audio::SoundGroup group = elysia::audio::SoundGroup::Ambient;
    bool enabled = true;
    SceneSoundPausePolicy pause_policy = SceneSoundPausePolicy::FollowScene;
};

enum class LoopSoundStatus
{
    Playing, Disabled, ScenePaused, Inaudible, OutOfRange, WaitingForVoice, MissingResource
};

struct LoopSoundState
{
    LoopSoundStatus status = LoopSoundStatus::WaitingForVoice;
    std::optional<elysia::audio::SoundHandle> voice;
};

// Scene owns this runtime. No GameObject or native audio pointer is retained.
class SceneAudioRuntime final
{
public:
    explicit SceneAudioRuntime(SceneAudioConfig config = {});
    ~SceneAudioRuntime();
    SceneAudioRuntime(const SceneAudioRuntime&) = delete;
    SceneAudioRuntime& operator=(const SceneAudioRuntime&) = delete;

    bool set_listener_position(elysia::core::Vector2 position);
    [[nodiscard]] elysia::core::Vector2 listener_position() const noexcept { return _listener; }
    [[nodiscard]] elysia::audio::SoundRequestResult request_sound(std::string_view key,
        const elysia::audio::SoundPlayOptions& options = {},
        SceneSoundPausePolicy pause_policy = SceneSoundPausePolicy::FollowScene);
    [[nodiscard]] elysia::audio::SoundRequestResult request_sound_at(std::string_view key,
        elysia::core::Vector2 position,const elysia::audio::SoundDistanceConfig& distance,
        const elysia::audio::SoundPlayOptions& options = {},
        SceneSoundPausePolicy pause_policy = SceneSoundPausePolicy::FollowScene);
    // Owned requests use these setters so distance/scene refresh preserves intent.
    bool set_sound_gain(elysia::audio::SoundHandle handle,double gain);
    bool set_sound_paused(elysia::audio::SoundHandle handle,bool paused);

    [[nodiscard]] std::optional<LoopSoundId> create_loop(const LoopSoundRequest& request);
    bool set_loop_position(LoopSoundId id,elysia::core::Vector2 position);
    bool set_loop_gain(LoopSoundId id,double gain);
    bool set_loop_enabled(LoopSoundId id,bool enabled);
    bool destroy_loop(LoopSoundId id);
    // Snapshot after the last update or setter. Queries never acquire a voice.
    [[nodiscard]] std::optional<LoopSoundState> loop_state(LoopSoundId id) const;

    void update(double delta_seconds);
    void set_paused(bool paused);
    // Drops ownership and resets listener/retry clock, preserving scene pause.
    void clear() noexcept;

private:
    struct OwnedSound
    {
        elysia::audio::SoundHandle handle = 0;
        double gain = 1;
        SceneSoundPausePolicy pause_policy = SceneSoundPausePolicy::FollowScene;
        std::optional<elysia::core::Vector2> position;
        elysia::audio::SoundDistanceConfig distance;
        bool paused = false;
    };
    struct Loop
    {
        LoopSoundId id;
        LoopSoundRequest request;
        LoopSoundState state;
        double next_attempt = 0;
        bool warned_missing = false;
    };
    elysia::audio::SoundRequestResult request_owned(std::string_view key,
        const elysia::audio::SoundPlayOptions& options,OwnedSound sound);
    void refresh_sound(const OwnedSound& sound);
    void refresh_loop(Loop& loop);
    Loop* find_loop(LoopSoundId id);
    double distance_to(elysia::core::Vector2 position) const noexcept;

    SceneAudioConfig _config;
    elysia::core::Vector2 _listener{};
    std::vector<OwnedSound> _sounds;
    std::vector<Loop> _loops;
    double _elapsed = 0;
    bool _paused = false;
};
}
