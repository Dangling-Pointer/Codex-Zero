#include "builtin_audio_player.h"
#include "../../audio/mixer_backend.h"

#include "../resources/builtin_asset_cache.h"
#include "../../tools/logger.h"

#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>

namespace elysia::builtin
{
void BuiltinAudioPlayer::bind(const BuiltinAssetCache& cache,const elysia::audio::AudioSettings& settings) noexcept
{
    _cache = &cache;
    _settings.master_volume = clamp_volume(settings.master_volume);
    _settings.music_volume = clamp_volume(settings.music_volume);
    _settings.sound_volume = clamp_volume(settings.sound_volume);
}

void BuiltinAudioPlayer::unbind() noexcept
{
    _cache = nullptr;
}

bool BuiltinAudioPlayer::bound() const noexcept
{
    return _cache != nullptr;
}

int BuiltinAudioPlayer::play_sound(BuiltinSoundId id, int loops) const
{
    if (!_cache)
    {
        ELYSIA_LOG_WARN("builtin","Play sound failed: audio player is not bound.");
        return -1;
    }

    MIX_Audio* sound = _cache->find_sound(id);
    if (!sound)
    {
        ELYSIA_LOG_WARN("builtin","Play sound failed: built-in sound does not exist: "
            << builtin_resource_name(id));
        return -1;
    }

    auto& backend = elysia::audio::detail::mixer_backend();
    if (!backend.initialize()) return -1;
    int channel = 0;
    while (channel < static_cast<int>(backend.tracks.size()) && MIX_TrackPlaying(backend.tracks[channel])) ++channel;
    if (channel == static_cast<int>(backend.tracks.size())) return -1;
    MIX_SetTrackGain(backend.tracks[channel],(_settings.master_volume * _settings.sound_volume) / 10000.0f);
    if (!backend.play(backend.tracks[channel],sound,loops)) return -1;

    return channel;
}

bool BuiltinAudioPlayer::play_music(BuiltinMusicId id, int loops) const
{
    if (!_cache)
    {
        ELYSIA_LOG_WARN("builtin","Play music failed: audio player is not bound.");
        return false;
    }

    MIX_Audio* music = _cache->find_music(id);
    if (!music)
    {
        ELYSIA_LOG_WARN("builtin","Play music failed: built-in music does not exist: "
            << builtin_resource_name(id));
        return false;
    }

    auto& backend = elysia::audio::detail::mixer_backend();
    if (!backend.initialize()) return false;
    MIX_SetTrackGain(backend.music,(_settings.master_volume * _settings.music_volume) / 10000.0f);
    if (!backend.play(backend.music,music,loops)) return false;

    return true;
}

void BuiltinAudioPlayer::stop_music() const noexcept
{
    auto& backend = elysia::audio::detail::mixer_backend();
    backend.stop(backend.music);
}

void BuiltinAudioPlayer::set_master_volume(int volume) noexcept
{
    _settings.master_volume = clamp_volume(volume);
}

void BuiltinAudioPlayer::set_music_volume(int volume) noexcept
{
    _settings.music_volume = clamp_volume(volume);
}

void BuiltinAudioPlayer::set_sound_volume(int volume) noexcept
{
    _settings.sound_volume = clamp_volume(volume);
}

const elysia::audio::AudioSettings& BuiltinAudioPlayer::settings() const noexcept
{
    return _settings;
}

int BuiltinAudioPlayer::clamp_volume(int volume) noexcept
{
    return std::clamp(volume,0,100);
}

}
