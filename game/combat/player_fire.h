#pragma once
#include "game/characters/player_character.h"
#include "engine/camera/camera.h"
class Wand;

namespace game
{
[[nodiscard]] elysia::core::Vector2 player_fire_direction(
    const PlayerCharacter& player, const elysia::camera::Camera& camera,
    const PlayerCharacter::FireRequest& pointer) noexcept;
void fire_player_requests(PlayerCharacter& player, const elysia::camera::Camera& camera, Wand& wand);
}
