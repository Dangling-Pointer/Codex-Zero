#include "builtin_resources.h"

namespace elysia::builtin
{
BuiltinResources::~BuiltinResources()
{
    shutdown();
}

std::expected<void, std::string> BuiltinResources::initialize(
    SDL_Renderer* renderer,
    const BuiltinAssetCatalog& catalog,
    std::span<const int> point_sizes,
    const elysia::audio::AudioSettings& audio_settings)
{
    auto result = _assets.initialize(renderer, catalog, point_sizes);
    if (!result)
        return result;
    _audio.bind(_assets, audio_settings);
    return {};
}

void BuiltinResources::shutdown() noexcept
{
    _audio.unbind();
    _assets.shutdown();
}

bool BuiltinResources::is_initialized() const noexcept
{
    return _assets.is_initialized() && _audio.bound();
}

SDL_Texture* BuiltinResources::find_texture(BuiltinTextureId id) const noexcept
{
    return _assets.find_texture(id);
}

TTF_Font* BuiltinResources::find_font(BuiltinFontId id,int point_size) const noexcept
{
    return _assets.find_font(id, point_size);
}

const std::string* BuiltinResources::find_translation(
    BuiltinLocaleId locale,std::string_view key) const noexcept
{
    return _assets.find_translation(locale, key);
}

const BuiltinAnimationDefinition* BuiltinResources::find_animation(
    BuiltinAnimationId id) const noexcept
{
    return _assets.find_animation(id);
}

MIX_Audio* BuiltinResources::find_sound(BuiltinSoundId id) const noexcept
{
    return _assets.find_sound(id);
}

MIX_Audio* BuiltinResources::find_music(BuiltinMusicId id) const noexcept
{
    return _assets.find_music(id);
}

std::unique_ptr<elysia::animation::Animation> BuiltinResources::create_animation(
    BuiltinAnimationId id) const
{
    return _assets.create_animation(id);
}

int BuiltinResources::play_sound(BuiltinSoundId id,int loops) const
{
    return _audio.play_sound(id, loops);
}

bool BuiltinResources::play_music(BuiltinMusicId id,int loops) const
{
    return _audio.play_music(id, loops);
}

void BuiltinResources::stop_music() const noexcept
{
    _audio.stop_music();
}

void BuiltinResources::set_master_volume(int volume) noexcept
{
    _audio.set_master_volume(volume);
}

void BuiltinResources::set_music_volume(int volume) noexcept
{
    _audio.set_music_volume(volume);
}

void BuiltinResources::set_sound_volume(int volume) noexcept
{
    _audio.set_sound_volume(volume);
}

const elysia::audio::AudioSettings& BuiltinResources::audio_settings() const noexcept
{
    return _audio.settings();
}

std::size_t BuiltinResources::texture_count() const noexcept { return _assets.texture_count(); }
std::size_t BuiltinResources::font_count() const noexcept { return _assets.font_count(); }
std::size_t BuiltinResources::locale_count() const noexcept { return _assets.locale_count(); }
std::size_t BuiltinResources::animation_count() const noexcept { return _assets.animation_count(); }
std::size_t BuiltinResources::sound_count() const noexcept { return _assets.sound_count(); }
std::size_t BuiltinResources::music_count() const noexcept { return _assets.music_count(); }
}
