#pragma once

#include "sound_playback_types.h"
#include "sound_distance.h"
#include "../core/geometry/vector2.h"
#include <optional>
#include <string>

namespace elysia::audio
{
struct LoopSoundId
{
    std::uint64_t value = 0;
    friend bool operator==(const LoopSoundId&,const LoopSoundId&) = default;
};

struct LoopSoundRequest
{
    std::string key;
    elysia::core::Vector2 position{};
    SoundDistanceConfig distance;
    double gain = 1;
    SoundGroup group = SoundGroup::Ambient;
    bool enabled = true;
    SoundPausePolicy pause_policy = SoundPausePolicy::FollowScene;
};

enum class LoopSoundStatus
{
    Playing, Disabled, ScenePaused, Inaudible, OutOfRange, WaitingForVoice, MissingResource
};

struct LoopSoundState
{
    LoopSoundStatus status = LoopSoundStatus::WaitingForVoice;
    std::optional<SoundHandle> voice;
    std::optional<AudioFailure> last_error;
};
}
