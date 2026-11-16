#pragma once

#include "room_scene_payload.h"
#include "engine/scene/scene.h"

namespace elysia::ui { class UiWindow; }

class TransitionScene final : public elysia::scene::Scene
{
protected:
    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void on_reset() override;
    void on_shortcuts(const elysia::input::RawInputFrame&,
                      const std::vector<elysia::input::RawInputEvent>& events) override;

private:
    void build_map();
    void clear_map() noexcept;
    void choose_room(RoomID room_id);
    void return_to_menu();

    std::optional<FloorMapPayload> _entry;
    elysia::ui::UiWindow* _window = nullptr;
    bool _route_requested = false;
};
