#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "routing/scene_request.h"
#include "routing/scene_request_observer.h"
#include "runtime/scene_runtime_context.h"

#include "../camera/camera_manager.h"
#include "../core/depth_layer.h"
#include "../core/game_object.h"
#include "../core/event/subject.h"
#include "../core/interface/updatable.h"
#include "../physics/physics_world.h"
#include "../input/contracts/raw_input_event_receiver.h"
#include "../input/contracts/raw_input_frame_receiver.h"
#include "../input/raw_input_frame.h"
#include "../input/raw_input_types.h"
#include "../object_query/runtime/game_object_query_runtime.h"
#include "../ui/core/ui_element.h"
#include "../ui/input/contracts/ui_input_event_receiver.h"
#include "../ui/input/contracts/ui_input_frame_receiver.h"
#include "../ui/input/ui_input_router.h"

namespace elysia::scene
{
class SceneFactory;
class SceneManager;

class Scene
    : public elysia::core::Subject<SceneRequestObserver>
    , public elysia::object_query::IGameObjectQueryRuntime
{
public:
    Scene() = default;
    explicit Scene(elysia::physics::PhysicsWorldConfig physics_config)
        : _physics_world(physics_config) {}
    virtual ~Scene() = default;

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;

    virtual void on_enter(const ScenePayload& payload) = 0;
    virtual void on_exit() = 0;
    virtual void reset() = 0;

    virtual void on_update(double delta);
    virtual void on_render(SDL_Renderer* renderer);
    virtual void on_input(const elysia::input::RawInputFrame& input, const std::vector<elysia::input::RawInputEvent>& events);

    void pause() { _paused = true; }
    void resume() { _paused = false; }
    [[nodiscard]] const elysia::camera::Camera& camera() const noexcept;
    [[nodiscard]] elysia::camera::CameraSlot render_camera_slot() const noexcept;

    template <typename T, typename... Args>
    T* create_and_add_object(Args&&... args)
    {
        static_assert(
            std::is_base_of_v<elysia::core::GameObject, T> || std::is_base_of_v<elysia::ui::UiElement, T>,
            "T must derive from elysia::core::GameObject or elysia::ui::UiElement.");

        return add_object(std::make_unique<T>(std::forward<Args>(args)...)
        );
    }

    template <typename T>
    T* add_object(std::unique_ptr<T> object)
    {
        static_assert(
            std::is_base_of_v<elysia::core::SceneObject, T>,
            "T must derive from elysia::core::SceneObject.");

        static_assert(
            std::is_base_of_v<elysia::core::GameObject, T> || std::is_base_of_v<elysia::ui::UiElement, T>,
            "T must derive from elysia::core::GameObject or elysia::ui::UiElement.");

        if (!object)
            return nullptr;

        T* raw_object = object.get();
        bool added = false;

        if constexpr (std::is_base_of_v<elysia::core::GameObject, T>)
            added = add_game_object(std::move(object));
        else if constexpr (std::is_base_of_v<elysia::ui::UiElement, T>)
            added = add_ui_root(std::move(object));

        if (!added)
            return nullptr;

        register_scene_object_interfaces(raw_object);

        return raw_object;
    }

protected:
    void notify_scene_request(const SceneRequest& request);
    void request_scene_switch(const SceneRoute& route);
    void request_scene_switch(
        SceneKey target,
        const ScenePayload& payload = {},
        SceneReloadMode reload_mode = SceneReloadMode::Reuse
    );
    void request_quit();
    [[nodiscard]] const SceneRuntimeContext& runtime_context() const;
    void set_render_camera_slot(elysia::camera::CameraSlot slot) noexcept;
    [[nodiscard]] elysia::physics::PhysicsWorld& physics_world() noexcept;
    [[nodiscard]] const elysia::physics::PhysicsWorld& physics_world() const noexcept;
    virtual void on_scene_object_registered(elysia::core::SceneObject& object);
    [[nodiscard]] virtual std::optional<elysia::camera::CameraFocus> resolve_camera_focus() const;
    [[nodiscard]] virtual std::optional<elysia::core::Rect> resolve_camera_focus_rect() const;

protected:
    bool _paused = false;

private:
    friend class SceneFactory;
    friend class SceneManager;

    void bind_runtime_context(const SceneRuntimeContext& context) noexcept;
    void clear_runtime_context() noexcept;
    void register_scene_object_interfaces(elysia::core::SceneObject* object);
    void visit_game_objects(
        elysia::core::DepthLayerMask layers,
        const elysia::object_query::GameObjectVisitor& visitor) const override;
    void dispatch_ui_frame(const elysia::ui::UiInputFrame& input);
    void dispatch_ui_events(const std::vector<elysia::ui::UiInputEvent>& events);
    void remove_destroyed_objects();
    bool add_game_object(std::unique_ptr<elysia::core::GameObject> object);
    bool add_ui_root(std::unique_ptr<elysia::ui::UiElement> object);

    struct UpdatableEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::core::Updatable* updatable = nullptr;
    };

    struct RawInputFrameReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::input::RawInputFrameReceiver* receiver = nullptr;
    };

    struct RawInputEventReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::input::RawInputEventReceiver* receiver = nullptr;
    };

    struct UiInputFrameReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::ui::UiInputFrameReceiver* receiver = nullptr;
    };

    struct UiInputEventReceiverEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::ui::UiInputEventReceiver* receiver = nullptr;
    };

    struct PhysicsRegistrationEntry
    {
        elysia::core::SceneObject* object = nullptr;
        elysia::physics::PhysicsObjectHandle handle{};
    };

private:
    std::array<std::vector<std::unique_ptr<elysia::core::GameObject>>,
        static_cast<size_t>(elysia::core::DepthLayer::Count)> _object_layers;
    std::vector<std::unique_ptr<elysia::ui::UiElement>> _ui_roots;

    std::vector<UpdatableEntry> _updatables;
    std::vector<RawInputFrameReceiverEntry> _frame_receivers;
    std::vector<RawInputEventReceiverEntry> _event_receivers;
    std::vector<UiInputFrameReceiverEntry> _ui_frame_receivers;
    std::vector<UiInputEventReceiverEntry> _ui_event_receivers;
    std::vector<PhysicsRegistrationEntry> _physics_registrations;

    elysia::ui::UiInputRouter _ui_input_router;
    elysia::camera::CameraSlot _render_camera_slot = elysia::camera::CameraSlot::Main;
    elysia::physics::PhysicsWorld _physics_world;
    const SceneRuntimeContext* _runtime_context = nullptr;
};
}
