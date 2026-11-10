#include "player_fire.h"
#include "projectile_service.h"
#include "wand/wand.h"
#include "engine/tools/logger.h"
#include <cmath>
#include <utility>

namespace game
{
elysia::core::Vector2 player_fire_direction(const PlayerCharacter& player,
    const elysia::camera::Camera& camera, const PlayerCharacter::FireRequest& pointer) noexcept
{
    if (pointer)
    {
        const auto direction = camera.screen_to_world(*pointer) - player.render_rect().center();
        if (std::isfinite(direction.x) && std::isfinite(direction.y) && !direction.is_zero())
            return direction.normalized();
    }
    return player.facing() == Character::Facing::Left
        ? elysia::core::Vector2{-1, 0} : elysia::core::Vector2{1, 0};
}

void fire_player_requests(PlayerCharacter& player, const elysia::camera::Camera& camera, Wand& wand)
{
    auto requests = player.take_fire_requests();
    if (player.is_dead() || player.is_destroyed()) return;
    for (const auto& pointer : requests)
    {
        auto shots = wand.attack(player_fire_direction(player, camera, pointer));
        // ProjectileManager resolves offsets from the body's top-left position.
        for (auto& shot : shots) shot.spawn_offset += player.size() * 0.5f;
        if (!ProjectileService::instance()->request_fire(
                {player.actor_id(), player.physics_handle(), std::move(shots)}))
            ELYSIA_LOG_WARN("gameplay", "Player projectile fire request rejected");
    }
}
}
