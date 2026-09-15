#pragma once

#include "../resources/builtin_resource_ids.h"
#include "../../audio/audio_settings.h"

namespace elysia::builtin
{
class BuiltinAssetCache;

class BuiltinAudioPlayer
{
public:
    void bind(const BuiltinAssetCache& cache,const elysia::audio::AudioSettings& settings) noexcept;
    void unbind() noexcept;

    [[nodiscard]] bool bound() const noexcept;
    [[nodiscard]] int play_sound(BuiltinSoundId id, int loops = 0) const;
    [[nodiscard]] bool play_music(BuiltinMusicId id, int loops = -1) const;
    void stop_music() const noexcept;

    void set_master_volume(int volume) noexcept;
    void set_music_volume(int volume) noexcept;
    void set_sound_volume(int volume) noexcept;
    [[nodiscard]] const elysia::audio::AudioSettings& settings() const noexcept;

private:
    [[nodiscard]] static int clamp_volume(int volume) noexcept;

private:
    const BuiltinAssetCache* _cache = nullptr;
    elysia::audio::AudioSettings _settings{};
};
}
