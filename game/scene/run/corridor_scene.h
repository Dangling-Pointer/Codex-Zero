#pragma once

#include "room_scene.h"

class CorridorScene : public RoomScene
{
protected:
    RoomBuildResult build_room(const RoomBuildConfig& config) override;
    [[nodiscard]] bool should_create_gates() const noexcept override { return false; }
    void on_game_fixed_update(std::uint64_t tick, double delta) override;

};

[[nodiscard]] elysia::scene::SceneRoute make_corridor_test_route(const CharacterContext& character);
