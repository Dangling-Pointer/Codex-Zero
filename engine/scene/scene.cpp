#include "scene.h"

#include "detail/scene_input_order.h"

#include "../core/interface/updatable.h"
#include "../core/render/debug_draw_projection.h"
#include "../core/render/render_command_projection.h"
#include "../core/render/sdl_render_command_executor.h"
#include "../input/contracts/raw_input_event_receiver.h"
#include "../physics/physics_debug_draw.h"
#include "../physics/contracts/physics_participant.h"
#include "../tools/debug_draw.h"
#include "../tools/logger.h"
#include "../input/contracts/raw_input_frame_receiver.h"
#include "../ui/core/ui_render_command_range_utils.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace elysia::scene
{
    namespace
    {
        template <typename Entry>
        void erase_destroyed_entries(std::vector<Entry> &entries)
        {
            std::erase_if(entries, [](const Entry &entry)
                          { return !entry.object || entry.object->is_destroyed(); });
        }

        [[nodiscard]] elysia::physics::PhysicsDebugCapture physics_debug_capture(
            const elysia::tools::DebugDraw &debug_draw) noexcept
        {
            using elysia::physics::PhysicsDebugCapture;
            using elysia::tools::DebugDrawCategory;

            PhysicsDebugCapture capture = PhysicsDebugCapture::None;
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsCollider) || debug_draw.is_enabled(DebugDrawCategory::PhysicsPoseHistory))
            {
                capture |= PhysicsDebugCapture::Shapes;
            }
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsBroadPhase))
                capture |= PhysicsDebugCapture::BroadPhase;
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsContact) || debug_draw.is_enabled(DebugDrawCategory::PhysicsContactNormal))
            {
                capture |= PhysicsDebugCapture::Contacts;
            }
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsVelocity))
                capture |= PhysicsDebugCapture::Velocities;
            if (debug_draw.is_enabled(DebugDrawCategory::PhysicsJoint))
                capture |= PhysicsDebugCapture::Joints;
            return capture;
        }
    }

    Scene::~Scene()
    {
        for (auto &layer : _object_layers)
            layer.clear();
    }

    void Scene::on_input(const elysia::input::RawInputFrame &input, const std::vector<elysia::input::RawInputEvent> &events)
    {
        for (const RawInputFrameReceiverEntry &entry : _frame_receivers)
        {
            elysia::core::SceneObject *object = entry.object;

            if (!object || object->is_destroyed() || !object->is_active())
                continue;

            if (_paused && !object->receive_input_when_paused())
                continue;

            entry.receiver->on_raw_input_frame(input);
        }

        for (const elysia::input::RawInputEvent &input_event : events)
        {
            for (const RawInputEventReceiverEntry &entry : _event_receivers)
            {
                elysia::core::SceneObject *object = entry.object;

                if (!object || object->is_destroyed() || !object->is_active())
                    continue;

                if (_paused && !object->receive_input_when_paused())
                    continue;

                if (entry.receiver->on_raw_input_event(input_event))
                    break;
            }
        }

        const elysia::ui::UiInputFrame ui_input = _ui_input_router.route_frame(input);
        dispatch_ui_frame(ui_input);

        for (const elysia::input::RawInputEvent &raw_event : events)
        {
            dispatch_ui_events(_ui_input_router.route_event(raw_event));
        }

        dispatch_ui_events(_ui_input_router.synthesize_events(input));
    }

    void Scene::on_update(double delta)
    {
        for (const UpdatableEntry &entry : _updatables)
        {
            elysia::core::SceneObject *object = entry.object;

            if (!object || object->is_destroyed() || !object->is_active())
                continue;

            if (_paused && !object->update_when_paused())
                continue;

            entry.updatable->update(delta);
        }

        for (const std::unique_ptr<elysia::ui::UiElement> &ui_root : _ui_roots)
        {
            if (!ui_root || ui_root->is_destroyed() || !ui_root->is_active())
                continue;
            if (_paused && !ui_root->update_when_paused())
                continue;
            ui_root->update_presentation_animations(delta);
        }

        elysia::tools::DebugDraw *physics_debug_draw =
            elysia::tools::DebugDraw::instance();
        const auto debug_capture = physics_debug_capture(*physics_debug_draw);
        _physics_world.set_debug_capture(debug_capture);

        if (!_paused)
            (void)_physics_world.advance(delta);

        const auto physics_categories =
            elysia::tools::DebugDrawCategory::PhysicsCollider | elysia::tools::DebugDrawCategory::PhysicsContact | elysia::tools::DebugDrawCategory::PhysicsContactNormal | elysia::tools::DebugDrawCategory::PhysicsBroadPhase | elysia::tools::DebugDrawCategory::PhysicsPoseHistory | elysia::tools::DebugDrawCategory::PhysicsVelocity | elysia::tools::DebugDrawCategory::PhysicsJoint;
        physics_debug_draw->clear_categories(physics_categories);
        if (debug_capture != elysia::physics::PhysicsDebugCapture::None)
            elysia::physics::submit_physics_debug_snapshot(
                _physics_world.debug_snapshot(), *physics_debug_draw);

        auto *camera_manager = elysia::camera::CameraManager::instance();
        camera_manager->set_focus(
            elysia::camera::CameraSlot::Main,
            resolve_camera_focus());
        camera_manager->update(delta);

        remove_destroyed_objects();
    }

    void Scene::on_render(SDL_Renderer *renderer)
    {
        if (!renderer)
            return;

        std::vector<elysia::core::RenderCommand> render_commands;
        std::vector<elysia::core::ScreenRenderCommand> projected_render_commands;
        std::vector<elysia::core::UiRenderCommand> ui_render_commands;
        render_commands.reserve(256);
        projected_render_commands.reserve(256);
        ui_render_commands.reserve(256);

        for (const auto &layer : _object_layers)
        {
            render_commands.clear();
            projected_render_commands.clear();

            for (const std::unique_ptr<elysia::core::GameObject> &obj : layer)
            {
                if (!obj || obj->is_destroyed() || !obj->is_visible())
                    continue;

                obj->submit_render_commands(render_commands);
            }

            elysia::core::project_render_commands_to_screen(
                render_commands,
                camera(),
                projected_render_commands);
            elysia::core::execute_render_commands(renderer, projected_render_commands);
        }

        elysia::tools::DebugDraw *debug_draw =
            elysia::tools::DebugDraw::instance();
        if (debug_draw->enabled())
        {
            std::vector<elysia::core::UiRenderCommand> debug_draw_commands;
            debug_draw_commands.reserve(debug_draw->commands().size());
            elysia::core::append_projected_debug_draw_commands(
                debug_draw->commands(),
                debug_draw->enabled_categories(),
                camera(),
                debug_draw_commands);
            elysia::core::execute_render_commands(renderer, debug_draw_commands);
        }

        ui_render_commands.clear();
        for (const auto &ui_root : _ui_roots)
        {
            if (!ui_root || ui_root->is_destroyed() || !ui_root->is_visible())
                continue;

            const std::size_t begin = ui_render_commands.size();
            ui_root->submit_ui_render_commands(ui_render_commands);
            elysia::ui::render_command_range_utils::apply_translation_to_range(
                ui_render_commands, begin, ui_root->presentation_translation());
        }

        elysia::core::execute_render_commands(renderer, ui_render_commands);
    }

    void Scene::register_scene_object_interfaces(elysia::core::SceneObject *object)
    {
        if (!object)
            return;

        if (elysia::core::Updatable *updatable = dynamic_cast<elysia::core::Updatable *>(object))
            _updatables.push_back(UpdatableEntry{object, updatable});

        if (elysia::input::RawInputFrameReceiver *receiver = dynamic_cast<elysia::input::RawInputFrameReceiver *>(object))
            _frame_receivers.push_back(RawInputFrameReceiverEntry{object, receiver});

        if (elysia::input::RawInputEventReceiver *receiver = dynamic_cast<elysia::input::RawInputEventReceiver *>(object))
        {
            scene_input_order::insert_receiver_entry_sorted(
                _event_receivers,
                RawInputEventReceiverEntry{object, receiver});
        }

        if (elysia::ui::UiInputFrameReceiver *receiver = dynamic_cast<elysia::ui::UiInputFrameReceiver *>(object))
        {
            scene_input_order::insert_receiver_entry_sorted(
                _ui_frame_receivers,
                UiInputFrameReceiverEntry{object, receiver});
        }

        if (elysia::ui::UiInputEventReceiver *receiver = dynamic_cast<elysia::ui::UiInputEventReceiver *>(object))
        {
            scene_input_order::insert_receiver_entry_sorted(
                _ui_event_receivers,
                UiInputEventReceiverEntry{object, receiver});
        }

        elysia::core::GameObject *game_object = dynamic_cast<elysia::core::GameObject *>(object);
        if (game_object)
        {
            auto *participant = dynamic_cast<elysia::physics::PhysicsParticipant *>(object);
            if (participant)
            {
                const elysia::physics::PhysicsObjectHandle handle =
                    _physics_world.register_object(
                        *game_object,
                        participant->body_definition(),
                        participant->collider_definitions());
                if (handle.is_valid())
                {
                    participant->bind_physics(_physics_world, handle);
                    _physics_registrations.push_back(
                        PhysicsRegistrationEntry{object, handle});
                }
                else
                {
                    ELYSIA_LOG_ERROR(
                        "collision",
                        "Scene physics registration failed for a GameObject.");
                }
            }
        }

        on_scene_object_registered(*object);
    }

    void Scene::visit_game_objects(
        elysia::core::DepthLayerMask layers,
        const elysia::object_query::GameObjectVisitor &visitor) const
    {
        for (std::size_t layer_index = 0; layer_index < _object_layers.size(); ++layer_index)
        {
            const auto depth_layer = static_cast<elysia::core::DepthLayer>(layer_index);
            if (!layers.contains(depth_layer))
                continue;

            const auto &layer = _object_layers[layer_index];
            for (const std::unique_ptr<elysia::core::GameObject> &object : layer)
            {
                if (object && !visitor(*object))
                    return;
            }
        }
    }

    void Scene::dispatch_ui_frame(const elysia::ui::UiInputFrame &input)
    {
        for (const UiInputFrameReceiverEntry &entry : _ui_frame_receivers)
        {
            elysia::core::SceneObject *object = entry.object;

            if (!object || object->is_destroyed() || !object->is_active())
                continue;

            if (_paused && !object->receive_input_when_paused())
                continue;

            entry.receiver->on_ui_input_frame(input);
        }
    }

    void Scene::dispatch_ui_events(const std::vector<elysia::ui::UiInputEvent> &events)
    {
        for (const elysia::ui::UiInputEvent &ui_event : events)
        {
            for (const UiInputEventReceiverEntry &entry : _ui_event_receivers)
            {
                elysia::core::SceneObject *object = entry.object;

                if (!object || object->is_destroyed() || !object->is_active())
                    continue;

                if (_paused && !object->receive_input_when_paused())
                    continue;

                if (entry.receiver->on_ui_input_event(ui_event))
                    break;
            }
        }
    }

    void Scene::remove_destroyed_objects()
    {
        for (const PhysicsRegistrationEntry &entry : _physics_registrations)
        {
            if (entry.object && entry.object->is_destroyed())
                (void)_physics_world.unregister_object(entry.handle);
        }

        erase_destroyed_entries(_updatables);
        erase_destroyed_entries(_frame_receivers);
        erase_destroyed_entries(_event_receivers);
        erase_destroyed_entries(_ui_frame_receivers);
        erase_destroyed_entries(_ui_event_receivers);
        erase_destroyed_entries(_physics_registrations);

        for (auto &layer : _object_layers)
        {
            std::erase_if(layer, [](const std::unique_ptr<elysia::core::GameObject> &object)
                          { return !object || object->is_destroyed(); });
        }

        std::erase_if(_ui_roots, [](const std::unique_ptr<elysia::ui::UiElement> &object)
                      { return !object || object->is_destroyed(); });
    }

    void Scene::notify_scene_request(const SceneRequest &request)
    {
        notify_observers(
            [&](SceneRequestObserver &observer)
            {
                observer.on_scene_request(request);
            });
    }

    void Scene::request_scene_switch(
        const SceneRoute &route)
    {
        SceneRequest request;
        request.type = SceneRequestType::Switch;
        request.route = route;

        notify_scene_request(request);
    }

    void Scene::request_scene_switch(
        SceneKey target,
        const ScenePayload &payload,
        SceneReloadMode reload_mode)
    {
        request_scene_switch(SceneRoute{
            .target = target,
            .payload = payload,
            .reload_mode = reload_mode});
    }

    void Scene::request_quit()
    {
        SceneRequest request;
        request.type = SceneRequestType::Quit;

        notify_scene_request(request);
    }

    void Scene::on_scene_object_registered(elysia::core::SceneObject &object)
    {
        (void)object;
    }

    const SceneRuntimeContext &Scene::runtime_context() const
    {
        if (!_runtime_context)
            throw std::logic_error("Scene::runtime_context called before a runtime context was bound.");

        return *_runtime_context;
    }

    void Scene::bind_runtime_context(const SceneRuntimeContext &context) noexcept
    {
        _runtime_context = &context;
    }

    void Scene::clear_runtime_context() noexcept
    {
        _runtime_context = nullptr;
    }

    const elysia::camera::Camera &Scene::camera() const noexcept
    {
        return elysia::camera::CameraManager::instance()->camera(_render_camera_slot);
    }

    elysia::camera::CameraSlot Scene::render_camera_slot() const noexcept
    {
        return _render_camera_slot;
    }

    void Scene::set_render_camera_slot(elysia::camera::CameraSlot slot) noexcept
    {
        assert(slot != elysia::camera::CameraSlot::Count);
        if (slot == elysia::camera::CameraSlot::Count)
            return;

        _render_camera_slot = slot;
    }

    elysia::physics::PhysicsWorld &Scene::physics_world() noexcept
    {
        return _physics_world;
    }

    const elysia::physics::PhysicsWorld &Scene::physics_world() const noexcept
    {
        return _physics_world;
    }

    std::optional<elysia::camera::CameraFocus> Scene::resolve_camera_focus() const
    {
        const auto rect = resolve_camera_focus_rect();
        return rect ? std::optional(elysia::camera::CameraFocus{*rect, *rect}) : std::nullopt;
    }

    std::optional<elysia::core::Rect> Scene::resolve_camera_focus_rect() const
    {
        return std::nullopt;
    }

    bool Scene::add_game_object(std::unique_ptr<elysia::core::GameObject> object)
    {
        if (!object)
            return false;

        const size_t layer_index = static_cast<size_t>(object->depth_layer());

        if (layer_index >= _object_layers.size())
            return false;

        std::vector<std::unique_ptr<elysia::core::GameObject>> &layer = _object_layers[layer_index];

        auto iter = std::upper_bound(
            layer.begin(),
            layer.end(),
            object->order_in_layer(),
            [](int order, const std::unique_ptr<elysia::core::GameObject> &existing)
            {
                return order < existing->order_in_layer();
            });

        layer.insert(iter, std::move(object));
        return true;
    }

    bool Scene::add_ui_root(std::unique_ptr<elysia::ui::UiElement> object)
    {
        if (!object)
            return false;

        auto iter = std::upper_bound(
            _ui_roots.begin(),
            _ui_roots.end(),
            object->order(),
            [](int order, const std::unique_ptr<elysia::ui::UiElement> &existing)
            {
                return order < existing->order();
            });

        _ui_roots.insert(iter, std::move(object));
        return true;
    }
}
