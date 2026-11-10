#pragma once
#include "engine/gameplay/scene/gameplay_scene.h"
#include "game/input/game_input_map.h"
#include <stdexcept>
#include "engine/tools/logger.h"
namespace game
{
inline elysia::gameplay::GameplaySceneFeatures room_features()
{
    return {.physics = elysia::physics::PhysicsWorldConfig{}, .gameplay_collision = true,
            .camera = elysia::scene::CameraSceneConfig{
                .focus_mode = elysia::scene::CameraFocusMode::ResolveEachFrame}};
}
inline elysia::gameplay::ControllerHandle bind_player(elysia::gameplay::SceneControlContext& context, elysia::core::GameObject& player)
{
    using namespace elysia::gameplay;
    auto* service = ControllerService::instance();
    auto handle = service->create<LocalPlayerController>(
        {ControllerScope::Scene, context.token()}, elysia::input::PrimaryLocalPlayer,
        game::input::make_player_input_map());
    if (!handle) throw std::runtime_error("Cannot create player controller");
    if (!service->bind_target(*handle, context, player).succeeded())
    {
        (void)service->remove(*handle);
        throw std::runtime_error("Cannot bind player controller");
    }
    return *handle;
}
inline void release_player(elysia::gameplay::ControllerHandle& handle) noexcept
{
    if (!handle) return;
    try { (void)elysia::gameplay::ControllerService::instance()->remove(handle); }
    catch (...) { ELYSIA_LOG_ERROR("gameplay", "Player controller cleanup failed"); }
    handle = {};
}
}
