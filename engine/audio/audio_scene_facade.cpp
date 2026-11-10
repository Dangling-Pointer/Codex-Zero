#include "audio_service.h"
#include "../scene/runtime/scene_audio_runtime.h"
#include <stdexcept>

namespace elysia::audio
{
namespace { AudioFailure no_scene() { return audio_failure(AudioError::NoActiveScene,"No attached scene audio context."); } }
void AudioService::attach_runtime(elysia::scene::SceneAudioRuntime& runtime)
{
    if(_scene_runtime) throw std::logic_error("Another scene audio runtime is already attached.");
    _scene_runtime=&runtime;
}
void AudioService::detach_runtime(elysia::scene::SceneAudioRuntime& runtime) noexcept { if(_scene_runtime==&runtime) _scene_runtime=nullptr; }
AudioResult<void> AudioService::set_listener_position(elysia::core::Vector2 position)
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->set_listener_position(position);
}
AudioResult<elysia::core::Vector2> AudioService::listener_position() const
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->listener_position();
}
AudioResult<LoopSoundId> AudioService::create_loop(const LoopSoundRequest& request)
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    if(!_initialized) return std::unexpected(audio_failure(AudioError::NotInitialized,"Audio service is not initialized."));
    return _scene_runtime->create_loop(request);
}
AudioResult<void> AudioService::set_loop_position(LoopSoundId id,elysia::core::Vector2 position)
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->set_loop_position(id,position);
}
AudioResult<void> AudioService::set_loop_gain(LoopSoundId id,double gain)
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->set_loop_gain(id,gain);
}
AudioResult<void> AudioService::set_loop_enabled(LoopSoundId id,bool enabled)
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->set_loop_enabled(id,enabled);
}
AudioResult<void> AudioService::destroy_loop(LoopSoundId id)
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->destroy_loop(id);
}
AudioResult<LoopSoundState> AudioService::loop_state(LoopSoundId id) const
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->loop_state(id);
}
AudioResult<void> AudioService::clear_scene_sounds()
{
    if(!_scene_runtime) return std::unexpected(no_scene());
    return _scene_runtime->clear_sounds();
}
}
