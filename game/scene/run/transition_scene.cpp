#include "transition_scene.h"

#include "game/gameflow/game_flow_manager.h"
#include "game/scene/scene_keys.h"
#include "game/ui/floor_map_view.h"

#include <stdexcept>

void TransitionScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto& flow = *GameFlowManager::instance();
    const auto* entry = elysia::scene::try_scene_payload<FloorMapPayload>(payload);
    if (!entry || entry->from_room != flow.get_current_room_id())
        throw std::invalid_argument("Floor map requires the current room and player context");

    _entry = *entry;
    const auto width = static_cast<float>(runtime_context().logical_width());
    const auto height = static_cast<float>(runtime_context().logical_height());

    _map_view = create_and_add_object<FloorMapView>(
        elysia::core::Rect{0, 0, width, height},
        flow.current_floor(),
        [&flow](RoomID id) { return flow.can_enter_room(id); },
        [this](RoomID id) { choose_room(id); });
    if (!_map_view)
        throw std::runtime_error("Cannot create floor map view");

    _map_view->set_on_cancel([this] { return_to_menu(); });
}

void TransitionScene::on_exit() { clear_map(); }
void TransitionScene::on_reset() { clear_map(); }

void TransitionScene::clear_map() noexcept
{
    if (_map_view)
        _map_view->destroy();
    _map_view = nullptr;
    _entry.reset();
    _route_requested = false;
}

void TransitionScene::choose_room(RoomID room_id)
{
    if (_route_requested || !_entry)
        return;

    const auto& flow = *GameFlowManager::instance();

    if (_entry->from_room != flow.get_current_room_id() || !flow.can_enter_room(room_id))
        return;

    request_scene_switch(make_run_room_route(room_id, _entry->character, _entry->from_room));
    _route_requested = true;
}

void TransitionScene::return_to_menu()
{
    if (_route_requested)
        return;
    request_scene_switch(MainMenu);
    _route_requested = true;
}

void TransitionScene::on_shortcuts(
    const elysia::input::RawInputFrame&, const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const auto& event : events)
    {
        if (event.type != elysia::input::RawInputEventType::ControlPressed
            || event.control != elysia::input::RawInputControl::KeyEscape)
            continue;

        consume_input(event);
        return_to_menu();
        return;
    }
}
