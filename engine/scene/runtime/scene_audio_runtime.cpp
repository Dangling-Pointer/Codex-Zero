#include "scene_audio_runtime.h"
#include "../../audio/audio_service.h"
#include "../../resources/resource_service.h"
#include "../../tools/logger.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace elysia::scene
{
namespace
{
bool valid_position(elysia::core::Vector2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool valid_gain(double gain) { return std::isfinite(gain) && gain >= 0; }
bool valid_policy(SceneSoundPausePolicy policy)
{
    return policy == SceneSoundPausePolicy::FollowScene || policy == SceneSoundPausePolicy::ContinueWhenPaused;
}
LoopSoundId next_loop_id()
{
    static std::atomic<std::uint64_t> next{1};
    auto value = next.load(std::memory_order_relaxed);
    do
    {
        if (value == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Loop sound identifier space exhausted.");
    } while (!next.compare_exchange_weak(value,value + 1,std::memory_order_relaxed));
    return {value};
}
}

SceneAudioRuntime::SceneAudioRuntime(SceneAudioConfig config) : _config(config)
{
    if (!std::isfinite(config.loop_retry_interval_seconds) || config.loop_retry_interval_seconds < 0.25)
        throw std::invalid_argument("Scene audio loop retry interval must be finite and at least 0.25 seconds.");
}

SceneAudioRuntime::~SceneAudioRuntime() { clear(); }

bool SceneAudioRuntime::set_listener_position(elysia::core::Vector2 position)
{
    if (!valid_position(position)) return false;
    _listener = position;
    return true;
}

double SceneAudioRuntime::distance_to(elysia::core::Vector2 position) const noexcept
{
    return std::hypot(double(position.x) - _listener.x,double(position.y) - _listener.y);
}

elysia::audio::SoundRequestResult SceneAudioRuntime::request_sound(std::string_view key,
    const elysia::audio::SoundPlayOptions& options,SceneSoundPausePolicy policy)
{
    return request_owned(key,options,{.gain = options.gain,.pause_policy = policy});
}

elysia::audio::SoundRequestResult SceneAudioRuntime::request_sound_at(std::string_view key,
    elysia::core::Vector2 position,const elysia::audio::SoundDistanceConfig& distance,
    const elysia::audio::SoundPlayOptions& options,SceneSoundPausePolicy policy)
{
    if (!valid_position(position) || !elysia::audio::valid_sound_distance(distance)) return {};
    return request_owned(key,options,{.gain = options.gain,.pause_policy = policy,.position = position,.distance = distance});
}

elysia::audio::SoundRequestResult SceneAudioRuntime::request_owned(std::string_view key,
    const elysia::audio::SoundPlayOptions& options,OwnedSound sound)
{
    if (!valid_gain(options.gain) || !valid_policy(sound.pause_policy)) return {};
    auto initial = options;
    if (sound.position) initial.gain *= elysia::audio::sound_distance_gain(distance_to(*sound.position),sound.distance);
    // A request made while paused must never start with an audible first sample.
    if (_paused && sound.pause_policy == SceneSoundPausePolicy::FollowScene) initial.gain = 0;
    _sounds.push_back(sound); // Allocate before starting playback.
    try
    {
        const auto result = ELYSIA_AUDIO->request_sound(key,initial);
        if (!result.handle) { _sounds.pop_back(); return result; }
        _sounds.back().handle = *result.handle;
        refresh_sound(_sounds.back());
        return result;
    }
    catch (...)
    {
        if (_sounds.back().handle) (void)ELYSIA_AUDIO->stop_sound(_sounds.back().handle);
        _sounds.pop_back();
        throw;
    }
}

void SceneAudioRuntime::refresh_sound(const OwnedSound& sound)
{
    const bool paused = sound.paused || (_paused && sound.pause_policy == SceneSoundPausePolicy::FollowScene);
    if (paused) (void)ELYSIA_AUDIO->set_sound_paused(sound.handle,true);
    const double gain = sound.gain * (sound.position ?
        elysia::audio::sound_distance_gain(distance_to(*sound.position),sound.distance) : 1);
    (void)ELYSIA_AUDIO->set_sound_gain(sound.handle,gain);
    if (!paused) (void)ELYSIA_AUDIO->set_sound_paused(sound.handle,false);
}

bool SceneAudioRuntime::set_sound_gain(elysia::audio::SoundHandle handle,double gain)
{
    if (!valid_gain(gain) || !ELYSIA_AUDIO->sound_active(handle)) return false;
    const auto it = std::ranges::find(_sounds,handle,&OwnedSound::handle);
    if (it == _sounds.end()) return false;
    it->gain = gain; refresh_sound(*it); return true;
}

bool SceneAudioRuntime::set_sound_paused(elysia::audio::SoundHandle handle,bool paused)
{
    if (!ELYSIA_AUDIO->sound_active(handle)) return false;
    const auto it = std::ranges::find(_sounds,handle,&OwnedSound::handle);
    if (it == _sounds.end()) return false;
    const bool effective = paused || (_paused && it->pause_policy == SceneSoundPausePolicy::FollowScene);
    if (!ELYSIA_AUDIO->set_sound_paused(handle,effective)) return false;
    it->paused = paused;
    return true;
}

std::optional<LoopSoundId> SceneAudioRuntime::create_loop(const LoopSoundRequest& request)
{
    if (request.key.empty() || !valid_position(request.position) || !valid_gain(request.gain) ||
        !valid_policy(request.pause_policy) || !elysia::audio::valid_sound_distance(request.distance) ||
        elysia::audio::sound_group_index(request.group) >= elysia::audio::kSoundGroupCount) return std::nullopt;
    const auto id = next_loop_id();
    _loops.push_back({.id = id,.request = request});
    try { refresh_loop(_loops.back()); }
    catch (...)
    {
        if (_loops.back().state.voice) (void)ELYSIA_AUDIO->stop_sound(*_loops.back().state.voice);
        _loops.pop_back();
        throw;
    }
    return id;
}

void SceneAudioRuntime::refresh_loop(Loop& loop)
{
    auto* audio = ELYSIA_AUDIO;
    if (loop.state.voice && !audio->sound_active(*loop.state.voice)) loop.state.voice.reset();
    const double distance = distance_to(loop.request.position);
    const double gain = loop.request.gain * elysia::audio::sound_distance_gain(distance,loop.request.distance);
    const bool scene_paused = _paused && loop.request.pause_policy == SceneSoundPausePolicy::FollowScene;
    const bool paused = scene_paused || !loop.request.enabled || gain <= 0;
    if (distance > loop.request.distance.release_distance)
    {
        if (loop.state.voice) (void)audio->stop_sound(*loop.state.voice);
        loop.state.voice.reset();
        loop.state.status = LoopSoundStatus::OutOfRange;
        return;
    }
    if (!loop.request.enabled) loop.state.status = LoopSoundStatus::Disabled;
    else if (scene_paused) loop.state.status = LoopSoundStatus::ScenePaused;
    else if (gain <= 0) loop.state.status = LoopSoundStatus::Inaudible;
    else loop.state.status = LoopSoundStatus::WaitingForVoice;

    if (!loop.state.voice && !paused)
    {
        if (!audio->is_initialized()) return;
        if (!ELYSIA_RESOURCES->has_sound(loop.request.key))
        {
            loop.state.status = LoopSoundStatus::MissingResource;
            if (!loop.warned_missing)
            {
                ELYSIA_LOG_WARN("audio.scene","Loop sound resource missing: " << loop.request.key);
                loop.warned_missing = true;
            }
            return;
        }
        loop.warned_missing = false;
        if (_elapsed >= loop.next_attempt)
        {
            loop.next_attempt = _elapsed + _config.loop_retry_interval_seconds;
            loop.state.voice = audio->request_sound(loop.request.key,
                {.loops = -1,.group = loop.request.group,.gain = gain}).handle;
        }
    }
    if (loop.state.voice)
    {
        if (paused) (void)audio->set_sound_paused(*loop.state.voice,true);
        (void)audio->set_sound_gain(*loop.state.voice,gain);
        if (!paused)
        {
            (void)audio->set_sound_paused(*loop.state.voice,false);
            loop.state.status = LoopSoundStatus::Playing;
        }
    }
}

SceneAudioRuntime::Loop* SceneAudioRuntime::find_loop(LoopSoundId id)
{
    const auto it = std::ranges::find(_loops,id,&Loop::id);
    return it == _loops.end() ? nullptr : &*it;
}

bool SceneAudioRuntime::set_loop_position(LoopSoundId id,elysia::core::Vector2 position)
{
    if (!valid_position(position)) return false;
    auto* loop = find_loop(id);
    if (!loop) return false;
    loop->request.position = position; refresh_loop(*loop); return true;
}

bool SceneAudioRuntime::set_loop_gain(LoopSoundId id,double gain)
{
    if (!valid_gain(gain)) return false;
    auto* loop = find_loop(id);
    if (!loop) return false;
    loop->request.gain = gain; refresh_loop(*loop); return true;
}

bool SceneAudioRuntime::set_loop_enabled(LoopSoundId id,bool enabled)
{
    auto* loop = find_loop(id);
    if (!loop) return false;
    loop->request.enabled = enabled; refresh_loop(*loop); return true;
}

bool SceneAudioRuntime::destroy_loop(LoopSoundId id)
{
    const auto it = std::ranges::find(_loops,id,&Loop::id);
    if (it == _loops.end()) return false;
    if (it->state.voice) (void)ELYSIA_AUDIO->stop_sound(*it->state.voice);
    _loops.erase(it); return true;
}

std::optional<LoopSoundState> SceneAudioRuntime::loop_state(LoopSoundId id) const
{
    const auto it = std::ranges::find(_loops,id,&Loop::id);
    return it == _loops.end() ? std::nullopt : std::optional{it->state};
}

void SceneAudioRuntime::update(double delta_seconds)
{
    _elapsed += elysia::audio::audio_delta(delta_seconds);
    std::erase_if(_sounds,[](const auto& sound) { return !ELYSIA_AUDIO->sound_active(sound.handle); });
    for (const auto& sound : _sounds) refresh_sound(sound);
    for (auto& loop : _loops) refresh_loop(loop);
}

void SceneAudioRuntime::set_paused(bool paused)
{
    _paused = paused;
    for (const auto& sound : _sounds) refresh_sound(sound);
    for (auto& loop : _loops) refresh_loop(loop);
}

void SceneAudioRuntime::clear() noexcept
{
    for (const auto& sound : _sounds)
        try { (void)ELYSIA_AUDIO->stop_sound(sound.handle); } catch (...) {}
    for (const auto& loop : _loops)
        if (loop.state.voice)
            try { (void)ELYSIA_AUDIO->stop_sound(*loop.state.voice); } catch (...) {}
    _sounds.clear(); _loops.clear(); _listener = {}; _elapsed = 0;
}
}
