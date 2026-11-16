#include "transition_scene.h"

#include "engine/localization/localization_service.h"
#include "engine/ui/widgets/ui_button.h"
#include "engine/ui/window/ui_window.h"
#include "game/gameflow/game_flow_manager.h"
#include "game/scene/scene_keys.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
using elysia::core::Rect;
using elysia::core::Vector2;
using elysia::ui::UiButton;
using elysia::ui::UiLayoutChildOptions;
using elysia::ui::UiWindow;

class MapConnections final : public elysia::ui::UiElement
{
public:
    using Edge = std::pair<Vector2, Vector2>;

    MapConnections(const Rect& rect, std::vector<Edge> edges)
        : UiElement(rect), _edges(std::move(edges)) {}

    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const override
    {
        for (const auto& [from, to] : _edges)
        {
            commands.push_back(elysia::core::make_ui_draw_line_command(
                from, to, elysia::core::colors::gray_500,
                elysia::core::UiStrokeWidth{elysia::core::UiStrokeWidthMode::Logical, 2.0f}));
        }
    }

private:
    std::vector<Edge> _edges;
};

std::string_view room_label_key(RoomType type)
{
    switch (type)
    {
    case RoomType::Start: return "floor_map.start";
    case RoomType::End: return "floor_map.end";
    case RoomType::Combat: return "floor_map.combat";
    case RoomType::Elite: return "floor_map.elite";
    case RoomType::Shop: return "floor_map.shop";
    case RoomType::Event: return "floor_map.event";
    case RoomType::Rest: return "floor_map.rest";
    default: return "floor_map.room";
    }
}
}

void TransitionScene::on_enter(const elysia::scene::ScenePayload& payload)
{
    const auto* entry = elysia::scene::try_scene_payload<FloorMapPayload>(payload);
    if (!entry || entry->from_room != GameFlowManager::instance()->get_current_room_id())
        throw std::invalid_argument("Floor map requires the current room and player context");

    _entry = *entry;
    build_map();
}

void TransitionScene::on_exit() { clear_map(); }
void TransitionScene::on_reset() { clear_map(); }

void TransitionScene::build_map()
{
    const auto& flow = *GameFlowManager::instance();
    const auto rooms = flow.rooms();
    const auto [left, right] = std::ranges::minmax_element(rooms, {}, &Room::x);
    const auto [top, bottom] = std::ranges::minmax_element(rooms, {}, &Room::y);
    const auto width = static_cast<float>(runtime_context().logical_width());
    const auto height = static_cast<float>(runtime_context().logical_height());
    const Rect viewport{0, 0, width, height};
    _window = create_and_add_object<UiWindow>(viewport, 10);
    if (!_window)
        throw std::runtime_error("Cannot create floor map window");

    const float columns = static_cast<float>(right->x - left->x + 1);
    const float rows = static_cast<float>(bottom->y - top->y + 1);
    const Vector2 spacing{std::min(154.0f, width / (columns + 1)),
                          std::min(106.0f, height / (rows + 1))};
    const Vector2 button_size{spacing.x * 0.82f, spacing.y * 0.45f};
    const float center_x = (left->x + right->x) * 0.5f;
    const float center_y = (top->y + bottom->y) * 0.5f;
    const auto button_center = [&](const Room& room) {
        return viewport.center() + Vector2{(room.x - center_x) * spacing.x,
                                           (room.y - center_y) * spacing.y};
    };

    std::vector<MapConnections::Edge> edges;
    for (const auto& room : rooms)
    {
        if (!room.discovered)
            continue;
        for (const RoomID id : room.neighbors)
        {
            if (id <= room.id)
                continue;
            const auto& neighbor = *flow.find_room(id);
            if (neighbor.discovered && (room.visited || neighbor.visited))
                edges.emplace_back(button_center(room), button_center(neighbor));
        }
    }
    _window->create_child<MapConnections>(viewport, std::move(edges));

    for (const auto& room : rooms)
    {
        if (!room.discovered)
            continue;
        const auto rect = Rect::from_center(button_center(room), button_size);
        const UiLayoutChildOptions layout{._margin = {.left = rect.x(), .top = rect.y()}};
        auto* button = _window->create_child<UiButton>(layout, Rect{{}, button_size}, 1);
        auto label = room.id < 10 ? "0" + std::to_string(room.id) : std::to_string(room.id);
        label += ' ';
        label += ELYSIA_LOCALIZATION->tr(room_label_key(room.type));
        button->set_text_content(elysia::ui::ui_raw_text(std::move(label)));
        button->set_enabled(flow.can_enter_room(room.id));

        elysia::ui::UiButtonStyleOverrides style;
        if (room.id == flow.get_current_room_id())
        {
            style.chrome.background.disabled = elysia::core::colors::blue_700;
            style.chrome.border.disabled = elysia::core::colors::blue_300;
            style.text.disabled = elysia::core::colors::white;
        }
        else if (room.type == RoomType::End)
        {
            style.chrome.border.disabled = elysia::core::colors::green_500;
            style.text.disabled = elysia::core::colors::green_300;
        }
        button->set_style_overrides(style);
        button->set_on_click([this, id = room.id] { choose_room(id); });
    }
    _window->set_on_cancel([this] { return_to_menu(); });
}

void TransitionScene::clear_map() noexcept
{
    if (_window)
        _window->destroy();
    _window = nullptr;
    _entry.reset();
    _route_requested = false;
}

void TransitionScene::choose_room(RoomID room_id)
{
    const auto& flow = *GameFlowManager::instance();
    if (_route_requested || !_entry || _entry->from_room != flow.get_current_room_id()
        || !flow.can_enter_room(room_id))
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

void TransitionScene::on_shortcuts(const elysia::input::RawInputFrame&,
                                    const std::vector<elysia::input::RawInputEvent>& events)
{
    for (const auto& event : events)
    {
        if (event.type == elysia::input::RawInputEventType::ControlPressed
            && event.control == elysia::input::RawInputControl::KeyEscape)
        {
            consume_input(event);
            return_to_menu();
            return;
        }
    }
}
