#pragma once

#include "engine/ui/core/ui_element.h"
#include "game/gameflow/dungeon_graph_types.h"
#include "game/map/room_corridor_layout.h"

#include <span>

// Read-only HUD: the scene supplies location; this element only maps and draws it.
class FloorMinimapHud final : public elysia::ui::UiElement
{
public:
    FloorMinimapHud(const elysia::core::Rect& bounds, const Floor& floor);

    void set_player_location(RoomID room, elysia::core::Vector2 world_center,
                             std::span<const game::RoomCorridorLayout> corridors);
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& commands) const override;

private:
    [[nodiscard]] elysia::core::Vector2 screen_position(elysia::core::Vector2 grid) const noexcept;

    const Floor& _floor;
    elysia::core::Vector2 _grid_center;
    float _spacing = 32;
    std::optional<elysia::core::Vector2> _player_grid;
    std::optional<RoomID> _current_room;
};
