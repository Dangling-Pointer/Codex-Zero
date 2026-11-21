#pragma once

#include "fixed_room_scene.h"

class CorridorScene : public FixedRoomScene
{
public:
    void on_before_update(double delta) override;

protected:
    RoomBuildResult build_room(const RoomBuildConfig& config) override;
    [[nodiscard]] bool should_create_portal() const noexcept override { return false; }

};

[[nodiscard]] elysia::scene::SceneRoute make_corridor_test_route(const CharacterContext& character);
