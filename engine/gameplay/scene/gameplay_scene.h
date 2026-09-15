#pragma once

#include "../../scene/scene.h"
#include "../collision/gameplay_collision_runtime.h"
#include "../input/gameplay_input_map.h"
#include "../input/contracts/gameplay_input_event_receiver.h"
#include "../input/contracts/gameplay_input_frame_receiver.h"

#include <vector>

namespace elysia::gameplay
{
class GameplayScene : public elysia::scene::Scene
{
public:
    GameplayScene();
    explicit GameplayScene(elysia::physics::PhysicsWorldConfig physics_config);

    void on_input(
        const elysia::input::RawInputFrame& input,
        const std::vector<elysia::input::RawInputEvent>& events) override;

protected:
    void set_gameplay_input_enabled(bool enabled) noexcept;
    [[nodiscard]] bool gameplay_input_enabled() const noexcept { return _gameplay_input_enabled; }
    [[nodiscard]] elysia::input::InputActionMap& gameplay_input_map() noexcept { return _gameplay_input_map; }
    [[nodiscard]] const elysia::input::InputActionMap& gameplay_input_map() const noexcept { return _gameplay_input_map; }
    void on_scene_object_registered(elysia::core::SceneObject& object) override;
    [[nodiscard]] collision::GameplayCollisionRuntime& collision_runtime() noexcept
    {
        return _collision_runtime;
    }

private:
    friend class elysia::scene::SceneManager;

    [[nodiscard]] bool activate_collision_runtime() noexcept;
    void deactivate_collision_runtime() noexcept;
    struct FrameReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        GameplayInputFrameReceiver* receiver = nullptr;
    };

    struct EventReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        GameplayInputEventReceiver* receiver = nullptr;
    };

    void prune_receivers();
    void dispatch_frame(const GameplayInputFrame& input);
    void dispatch_events(const std::vector<elysia::input::ActionInputEvent>& events);

    collision::GameplayCollisionRuntime _collision_runtime;
    elysia::input::InputActionMap _gameplay_input_map;
    std::vector<FrameReceiverEntry> _frame_receivers;
    std::vector<EventReceiverEntry> _event_receivers;
    bool _gameplay_input_enabled = true;
};
}
