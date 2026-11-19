#include "floor_map_view.h"

#include "engine/localization/localization_service.h"
#include "engine/ui/widgets/ui_button.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using elysia::core::Rect;
using elysia::core::Vector2;
using elysia::ui::UiButton;
using elysia::ui::UiButtonStyleOverrides;
using elysia::ui::UiLayoutChildOptions;

class MapConnections final : public elysia::ui::UiElement
{
public:
    using Edge = std::pair<Vector2, Vector2>;

    MapConnections(const Rect& rect, std::vector<Edge> edges, Vector2 button_size)
        : UiElement(rect), _edges(std::move(edges)), _button_size(button_size) {}

    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const override
    {
        using namespace elysia::core;
        constexpr UiStrokeWidth stroke{UiStrokeWidthMode::Logical, 2.0f};

        for (const auto& [from, to] : _edges)
        {
            const auto delta = to - from;
            const bool horizontal = delta.y == 0.0f;
            const float length = std::abs(horizontal ? delta.x : delta.y);
            const float button_extent = horizontal ? _button_size.x : _button_size.y;
            const float gap = length - button_extent;
            if (gap <= 0.0f)
                continue;

            const auto direction = delta * (1.0f / length);
            const auto button_edge = direction * (button_extent * 0.5f);
            const auto start = from + button_edge;
            const auto end = to - button_edge;
            commands.push_back(make_ui_draw_line_command(start, end, colors::gray_500, stroke));

            // Each endpoint shows the direction a player can leave that room.
            const float arrow_size = std::min(6.0f, gap * 0.2f);
            const float arrow_offset = arrow_size + std::min(6.0f, gap * 0.08f);
            const auto inset = direction * arrow_offset;
            draw_arrow(commands, start + inset, direction, arrow_size, stroke);
            draw_arrow(commands, end - inset, -direction, arrow_size, stroke);
        }
    }

private:
    static void draw_arrow(std::vector<elysia::core::UiRenderCommand>& commands,
                           Vector2 tip, Vector2 direction, float size, elysia::core::UiStrokeWidth stroke)
    {
        using namespace elysia::core;
        const auto base = tip - direction * size;
        const Vector2 wing{-direction.y * size * 0.6f, direction.x * size * 0.6f};
        commands.push_back(make_ui_draw_line_command(base + wing, tip, colors::gray_300, stroke));
        commands.push_back(make_ui_draw_line_command(base - wing, tip, colors::gray_300, stroke));
    }

    std::vector<Edge> _edges;
    Vector2 _button_size;
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

UiButtonStyleOverrides room_button_style(const Room& room, bool is_current_room)
{
    using namespace elysia::core::colors;

    UiButtonStyleOverrides style;
    if (is_current_room)
    {
        style.chrome.background.disabled = blue_700;
        style.chrome.border.disabled = blue_300;
        style.text.disabled = white;
    }
    else if (room.visited)
    {
        style.chrome.background.idle = brown_700;
        style.chrome.background.focused = brown_500;
        style.chrome.background.active = brown_700;
        style.chrome.border.idle = orange_300;
        style.chrome.border.focused = yellow_300;
        style.chrome.border.active = orange_500;
        style.text.enabled = white;
    }
    else if (room.type == RoomType::End)
    {
        style.chrome.border.disabled = green_500;
        style.text.disabled = green_300;
    }
    return style;
}
}

FloorMapView::FloorMapView(const Rect& viewport, const Floor& floor,
                           const std::function<bool(RoomID)>& can_enter_room,
                           const std::function<void(RoomID)>& choose_room,
                           Visibility visibility)
    : UiWindow(viewport, 10)
{
    std::vector<FloorPosition> positions;
    for (int y = 0; y < Floor::height; ++y)
    {
        for (int x = 0; x < Floor::width; ++x)
        {
            if (floor.room_at({x, y}))
                positions.push_back({x, y});
        }
    }
    if (positions.empty())
        throw std::logic_error("Cannot display an empty floor");

    const auto [left, right] = std::ranges::minmax_element(positions, {}, &FloorPosition::x);
    const auto [top, bottom] = std::ranges::minmax_element(positions, {}, &FloorPosition::y);

    const auto width = viewport.width();
    const auto height = viewport.height();

    const float columns = static_cast<float>(right->x - left->x + 1);
    const float rows = static_cast<float>(bottom->y - top->y + 1);

    const Vector2 spacing{
        std::min(154.0f, width / (columns + 1)),
        std::min(106.0f, height / (rows + 1)),
    };

    const Vector2 button_size{spacing.x * 0.82f, spacing.y * 0.45f};

    const float center_x = (left->x + right->x) * 0.5f;
    const float center_y = (top->y + bottom->y) * 0.5f;

    const auto button_center = [&](FloorPosition position) {
        const Vector2 offset{
            (position.x - center_x) * spacing.x,
            (position.y - center_y) * spacing.y,
        };

        return viewport.center() + offset;
    };

    std::vector<MapConnections::Edge> edges;
    const bool show_all_rooms = visibility == Visibility::ShowAll;

    for (const auto position : positions)
    {
        const auto& room = *floor.room_at(position);

        if (!show_all_rooms && !room.discovered)
            continue;

        for (const auto& neighbor_position : floor.neighbors(position))
        {
            if (!neighbor_position)
                continue;

            const auto& neighbor = *floor.room_at(*neighbor_position);

            if (neighbor.id <= room.id) // Draw each undirected connection once.
                continue;

            const bool connection_discovered = neighbor.discovered && (room.visited || neighbor.visited);
            if (show_all_rooms || connection_discovered)
                edges.emplace_back(button_center(position), button_center(*neighbor_position));
        }
    }
    create_child<MapConnections>(viewport, std::move(edges), button_size);

    for (const auto position : positions)
    {
        const auto& room = *floor.room_at(position);

        if (!show_all_rooms && !room.discovered)
            continue;

        const auto rect = Rect::from_center(button_center(position), button_size);
        const UiLayoutChildOptions layout{
            ._margin = {.left = rect.x() - viewport.x(), .top = rect.y() - viewport.y()},
        };
        auto* button = create_child<UiButton>(layout, Rect{{}, button_size}, 1);

        auto label = std::to_string(room.id);

        if (room.id < 10)
            label.insert(label.begin(), '0');

        label += ' ';
        label += ELYSIA_LOCALIZATION->tr(room_label_key(room.type));

        button->set_text_content(elysia::ui::ui_raw_text(std::move(label)));
        button->set_enabled(can_enter_room(room.id));
        button->set_style_overrides(room_button_style(room, room.id == floor.current_room_id));
        button->set_on_click([choose_room, id = room.id] { choose_room(id); });
    }
}
