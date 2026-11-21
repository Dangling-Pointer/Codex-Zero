#include "game/ui/floor_minimap_hud.h"
#include "game/map/dungeon_room.h"
#include "engine/core/render/colors.h"

#include <algorithm>
#include <stdexcept>

namespace
{
using namespace elysia::core;

void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

Vector2 marker(const FloorMinimapHud& hud)
{
    std::vector<UiRenderCommand> commands;
    hud.submit_ui_render_commands(commands);
    check(!commands.empty() && commands.back().color == colors::red_500
        && commands.back().type == UiRenderCommandType::FillCircle, "minimap draws a red player dot last");
    return commands.back().circle_center;
}

void layout_and_mapping()
{
    Floor floor;
    floor.rooms[0][0] = Room{.id = 0, .type = RoomType::Start};
    floor.rooms[0][3] = Room{.id = 1, .type = RoomType::Event};
    floor.rooms[3][0] = Room{.id = 2, .type = RoomType::End};
    check(floor.connect({0, 0}, {3, 0}) && floor.connect({0, 0}, {0, 3}), "connect sparse L-shaped HUD fixture");
    FloorMinimapHud hud({0, 0, 220, 220}, floor);
    hud.set_player_location(0, {}, {});
    const auto start = marker(hud);
    hud.set_player_location(1, {}, {});
    const auto right = marker(hud);
    hud.set_player_location(2, {}, {});
    const auto down = marker(hud);
    check(start.x == down.x && start.y == right.y && right.x > start.x && down.y > start.y,
          "room blocks retain the floor's L shape and empty-cell spacing");

    std::vector<UiRenderCommand> commands;
    hud.submit_ui_render_commands(commands);
    check(std::ranges::count(commands, UiRenderCommandType::DrawLine, &UiRenderCommand::type) == 2,
          "draw each real long connection once, with no invented link between unconnected rooms");
    check(hud.input_capture() == elysia::input::InputCapture::None, "HUD never captures gameplay input");

    const game::RoomCorridorLayout exit{RoomDirection::Right, {100, 0, 200, 70}, {100, 10, 200, 50}};
    hud.set_player_location(0, {100, 35}, {&exit, 1});
    check(marker(hud).nearly_equals(start), "at the room mouth the marker remains on its room");
    hud.set_player_location(0, {150, 35}, {&exit, 1});
    check(marker(hud).nearly_equals(start + (right - start) * 0.25f), "halfway to the seam maps to a quarter edge");
    hud.set_player_location(0, exit.seam_center(), {&exit, 1});
    const auto midpoint = marker(hud);
    check(midpoint.nearly_equals((start + right) * 0.5f), "source seam maps to the edge midpoint");

    const game::RoomCorridorLayout entry{RoomDirection::Left, {-90, -20, 200, 70}, {-90, -10, 200, 50}};
    hud.set_player_location(1, entry.seam_center(), {&entry, 1});
    check(marker(hud).nearly_equals(midpoint), "target's translated entrance continues at the same HUD midpoint");
    hud.set_player_location(1, entry.seam_center() + Vector2{50, 0}, {&entry, 1});
    check(marker(hud).nearly_equals(start + (right - start) * 0.75f), "incoming corridor advances toward the target room");
    hud.set_player_location(0, {1000, 1000}, {&exit, 1});
    check(marker(hud).nearly_equals(start), "movement inside a room does not move its HUD dot");

    hud.set_player_location(999, {}, {});
    commands.clear();
    hud.submit_ui_render_commands(commands);
    check(std::ranges::none_of(commands, [](const auto& command) {
        return command.type == UiRenderCommandType::FillCircle && command.color == colors::red_500;
    }), "unknown room IDs hide the player marker");
}

void full_floor()
{
    Floor floor;
    for (int y = 0; y < Floor::height; ++y)
        for (int x = 0; x < Floor::width; ++x)
        {
            floor.rooms[y][x] = Room{.id = static_cast<RoomID>(y * Floor::width + x), .type = RoomType::Event};
            if (x > 0) check(floor.connect({x - 1, y}, {x, y}), "connect horizontal HUD fixture");
            if (y > 0) check(floor.connect({x, y - 1}, {x, y}), "connect vertical HUD fixture");
        }
    FloorMinimapHud hud({20, 30, 220, 220}, floor);
    std::vector<UiRenderCommand> commands;
    hud.submit_ui_render_commands(commands);
    check(std::ranges::count(commands, UiRenderCommandType::DrawLine, &UiRenderCommand::type) == 60,
          "full 6x6 floor has 60 undirected connections");
    for (const auto& command : commands)
    {
        if (command.type == UiRenderCommandType::FillRoundedRect)
            check(hud.screen_rect().contains(command.screen_rect), "all 36 blocks fit the HUD panel");
    }
}
}

void floor_minimap_tests()
{
    layout_and_mapping();
    full_floor();
}
