#include "gameplay_scene.h"

#include "../../scene/detail/scene_input_order.h"
#include "../collision/gameplay_collision_service.h"

#include <algorithm>

namespace elysia::gameplay
{
GameplayScene::GameplayScene()
    : _collision_runtime(physics_world()),
      _gameplay_input_map(make_default_gameplay_input_map())
{
}

GameplayScene::GameplayScene(elysia::physics::PhysicsWorldConfig physics_config)
    : elysia::scene::Scene(physics_config),
      _collision_runtime(physics_world()),
      _gameplay_input_map(make_default_gameplay_input_map())
{
}

bool GameplayScene::activate_collision_runtime() noexcept
{
    return collision::GameplayCollisionService::instance()->attach_runtime(
        _collision_runtime);
}

void GameplayScene::deactivate_collision_runtime() noexcept
{
    (void)collision::GameplayCollisionService::instance()->detach_runtime(
        _collision_runtime);
}

void GameplayScene::set_gameplay_input_enabled(bool enabled) noexcept
{
    if (_gameplay_input_enabled == enabled)
        return;
    _gameplay_input_enabled = enabled;
    _gameplay_input_map.reset_state();
}

void GameplayScene::on_input(
    const elysia::input::RawInputFrame& input,
    const std::vector<elysia::input::RawInputEvent>& events)
{
    elysia::scene::Scene::on_input(input, events);
    prune_receivers();
    if (!_gameplay_input_enabled)
        return;

    elysia::input::ActionInputResult result = _gameplay_input_map.resolve(input);
    dispatch_frame(GameplayInputFrame(std::move(result.frame)));
    dispatch_events(result.events);
}

void GameplayScene::on_scene_object_registered(elysia::core::SceneObject& object)
{
    elysia::scene::Scene::on_scene_object_registered(object);
    if (auto* receiver = dynamic_cast<GameplayInputFrameReceiver*>(&object))
    {
        elysia::scene::scene_input_order::insert_receiver_entry_sorted(
            _frame_receivers, FrameReceiverEntry{ &object, receiver });
    }
    if (auto* receiver = dynamic_cast<GameplayInputEventReceiver*>(&object))
    {
        elysia::scene::scene_input_order::insert_receiver_entry_sorted(
            _event_receivers, EventReceiverEntry{ &object, receiver });
    }
}

void GameplayScene::prune_receivers()
{
    const auto destroyed = [](const auto& entry)
    {
        return !entry.object || entry.object->is_destroyed();
    };
    std::erase_if(_frame_receivers, destroyed);
    std::erase_if(_event_receivers, destroyed);
}

void GameplayScene::dispatch_frame(const GameplayInputFrame& input)
{
    for (const FrameReceiverEntry& entry : _frame_receivers)
    {
        if (!entry.object || entry.object->is_destroyed() || !entry.object->is_active())
            continue;
        if (_paused && !entry.object->receive_input_when_paused())
            continue;
        entry.receiver->on_gameplay_input_frame(input);
    }
}

void GameplayScene::dispatch_events(const std::vector<elysia::input::ActionInputEvent>& events)
{
    for (const elysia::input::ActionInputEvent& event : events)
    {
        for (const EventReceiverEntry& entry : _event_receivers)
        {
            if (!entry.object || entry.object->is_destroyed() || !entry.object->is_active())
                continue;
            if (_paused && !entry.object->receive_input_when_paused())
                continue;
            if (entry.receiver->on_gameplay_input_event(event))
                break;
        }
    }
}
}
