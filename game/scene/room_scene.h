#pragma once

#include "../../engine/gameplay/scene/gameplay_scene.h"

namespace game::characters
{
class PlayerCharacter;
}

namespace game::scene
{
class RoomScene final : public elysia::gameplay::GameplayScene
{
public:
    RoomScene() = default;
    ~RoomScene() override = default;

    void on_enter(const elysia::scene::ScenePayload& payload) override;
    void on_exit() override;
    void reset() override;
    void on_input(const elysia::input::RawInputFrame& input,
                  const std::vector<elysia::input::RawInputEvent>& events) override;

protected:
    [[nodiscard]] std::optional<elysia::core::Rect> resolve_camera_focus_rect() const override;

private:
    void clear_room() noexcept;
    game::characters::PlayerCharacter* _player = nullptr;
    elysia::core::GameObject* _room_boundary = nullptr;
};
} // namespace game::scene
