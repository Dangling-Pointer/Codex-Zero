#pragma once
#include "sound_playback_types.h"
#include <cmath>

namespace elysia::audio
{
inline AudioResult<void> validate_sound_options(std::string_view key,const SoundPlayOptions& options)
{
    const auto finite_position=[](elysia::core::Vector2 p){return std::isfinite(p.x)&&std::isfinite(p.y);};
    if(key.empty()||!std::isfinite(options.gain)||options.gain<0||options.loops.value_or(0)<-1||
        sound_group_index(options.group)>=kSoundGroupCount||
        (options.scope!=SoundScope::Scene&&options.scope!=SoundScope::Global)||
        (options.pause_policy!=SoundPausePolicy::FollowScene&&options.pause_policy!=SoundPausePolicy::ContinueWhenPaused)||
        (options.spatial&&(options.scope==SoundScope::Global||!finite_position(options.spatial->position)||!valid_sound_distance(options.spatial->distance))))
        return std::unexpected(audio_failure(AudioError::InvalidArgument,"Invalid sound request configuration.",std::string(key)));
    return {};
}
}
