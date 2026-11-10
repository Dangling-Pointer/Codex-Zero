#pragma once
#include "sound_playback_types.h"
#include <SDL3_mixer/SDL_mixer.h>
#include <array>

namespace elysia::audio::detail
{
inline AudioFailure mixer_failure(std::string operation)
{
    return audio_failure(AudioError::BackendFailure,std::move(operation)+": "+SDL_GetError());
}
// Shared by built-in startup audio and project audio. Resource caches own MIX_Audio.
class MixerBackend
{
public:
    AudioResult<void> initialize()
    {
        if (mixer) return {};
        mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,nullptr);
        if (!mixer) return std::unexpected(mixer_failure("Create audio mixer"));
        music = MIX_CreateTrack(mixer);
        if (!music) { auto failure=mixer_failure("Create music track");shutdown();return std::unexpected(std::move(failure)); }
        for (auto& track : tracks)
        {
            track = MIX_CreateTrack(mixer);
            if (!track) { auto failure=mixer_failure("Create sound track");shutdown();return std::unexpected(std::move(failure)); }
        }
        return {};
    }
    void shutdown()
    {
        if (mixer) MIX_DestroyMixer(mixer);
        mixer = nullptr; music = nullptr; tracks.fill(nullptr);
    }
    static AudioResult<void> stop(MIX_Track* track)
    {
        if (!track) return {};
        AudioResult<void> result;
        if(!MIX_StopTrack(track,0)) result=std::unexpected(mixer_failure("Stop audio track"));
        if(!MIX_SetTrackAudio(track,nullptr)) collect_audio_failure(result,std::unexpected(mixer_failure("Detach audio track")));
        return result;
    }
    static AudioResult<void> play(MIX_Track* track,MIX_Audio* audio,int loops)
    {
        if (!track || !audio) return std::unexpected(audio_failure(AudioError::BackendFailure,"Missing playback track or decoded audio."));
        const auto options = SDL_CreateProperties();
        if (!options) return std::unexpected(mixer_failure("Create playback properties"));
        AudioResult<void> result;
        if(!SDL_SetNumberProperty(options,MIX_PROP_PLAY_LOOPS_NUMBER,loops)||!MIX_SetTrackAudio(track,audio)||!MIX_PlayTrack(track,options))
            result=std::unexpected(mixer_failure("Start audio track"));
        SDL_DestroyProperties(options);
        if (!result) collect_audio_failure(result,stop(track));
        return result;
    }
    void destroy_audio(MIX_Audio* audio)
    {
        if (!audio) return;
        if (music && MIX_GetTrackAudio(music) == audio) { auto result=stop(music);if(!result) report_audio_failure(result.error()); }
        for (auto* track : tracks)
            if (track && MIX_GetTrackAudio(track) == audio) { auto result=stop(track);if(!result) report_audio_failure(result.error()); }
        MIX_DestroyAudio(audio);
    }
    MIX_Mixer* mixer = nullptr;
    MIX_Track* music = nullptr;
    std::array<MIX_Track*,kSoundChannelCount> tracks{};
};
inline MixerBackend& mixer_backend() { static MixerBackend value; return value; }
}
