#pragma once

#include "engine/ui/window/ui_window.h"
#include "game/gameflow/dungeon_graph_types.h"

#include <functional>

class FloorMapView final : public elysia::ui::UiWindow
{
public:
    FloorMapView(const elysia::core::Rect& viewport, const Floor& floor,
                 const std::function<bool(RoomID)>& can_enter_room,
                 const std::function<void(RoomID)>& choose_room);
};
