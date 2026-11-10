#pragma once
#include "engine/scene/scene_manager.h"
#include "engine/io/loaders/asset_config_types.h"
#include "engine/gameplay/control/controller_service.h"
#include <stdexcept>

namespace test
{
class EmptyScene : public elysia::scene::Scene
{
    void on_enter(const elysia::scene::ScenePayload&) override {}
    void on_exit() override {}
    void on_reset() override {}
};
// Capture the instance built and owned by the manager, without exposing engine internals.
template<class T> class ObservedScene : public T
{
public:
    explicit ObservedScene(T*& instance) { instance = this; }
};
template<class T> class SceneHarness
{
    elysia::io::ContentRegistry content;
    elysia::scene::SceneRuntimeContext context{nullptr, content, 1280, 720};
    T* instance = nullptr;
public:
    elysia::scene::SceneManager manager;
    explicit SceneHarness(const elysia::scene::ScenePayload& payload = {})
    {
        manager.initialize(context);
        auto* controllers = elysia::gameplay::ControllerService::instance();
        if (!controllers->session_active() && !controllers->begin_session())
            throw std::runtime_error("Cannot start test controller session");
        manager.template register_game_scene<ObservedScene<T>>(1, std::ref(instance));
        manager.template register_game_scene<EmptyScene>(2);
        manager.start({.target = 1, .payload = payload});
        require_running();
        if (manager.current_scene_key() != 1) throw std::runtime_error("Initial scene failed");
    }
    T& scene() { return *instance; }
    void require_running()
    {
        if (manager.state() != elysia::scene::SceneManagerState::Running)
            throw std::runtime_error("SceneManager faulted");
    }
    void update(double delta) { manager.on_update(delta); require_running(); }
    void input(const elysia::input::InputSnapshot& snapshot)
    { manager.on_input(snapshot); require_running(); }
    void input(const elysia::input::RawInputFrame& frame,
               std::vector<elysia::input::RawInputEvent> events)
    {
        using namespace elysia::input;
        for (auto& event : events)
        {
            event.source = InputSourceId::keyboard();
            event.device = InputDevice::Keyboard;
        }
        input(InputSnapshot{.sources = {{.source = InputSourceId::keyboard(), .frame = frame}},
                            .events = std::move(events)});
    }
    void route(elysia::scene::SceneRoute route)
    {
        manager.on_scene_request({elysia::scene::SceneRequestType::Switch, route});
        update(0);
    }
    void leave() { route({.target = 2}); }
    void enter(const elysia::scene::ScenePayload& payload = {},
               elysia::scene::SceneReloadMode mode = elysia::scene::SceneReloadMode::Reuse)
    { route({.target = 1, .payload = payload, .reload_mode = mode}); }
};
}
