#pragma once

#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/physics/tile/tile_collision_world.h"
#include "game/combat/combat_system.h"
#include "game/combat/projectile_manager.h"
#include "room_scene_payload.h"

class PlayerCharacter;

// Observers of scene-owned objects; ownership stays with Scene.
struct RoomBuildResult
{
    elysia::core::GameObject* map = nullptr;
    elysia::physics::ITileCollisionWorld* collision_world = nullptr;
    elysia::core::Vector2 player_spawn;
};

class RoomScene : public elysia::gameplay::GameplayScene
{
public:
    ~RoomScene() noexcept override;
    void on_enter(const elysia::scene::ScenePayload& payload) final;
    void on_exit() final;
    void reset() final;
    void on_update(double delta) final;
    [[nodiscard]] std::optional<CharacterContext> capture_character_context() const;

protected:
    virtual RoomBuildResult build_room(const RoomBuildConfig& config) = 0;
    virtual void create_room_content() {}
    [[nodiscard]] PlayerCharacter* player() const noexcept { return _player; }
    [[nodiscard]] const RoomBuildConfig& room_config() const noexcept { return _config; }
    [[nodiscard]] CombatSystem& combat_system() noexcept { return _combat; }
    [[nodiscard]] std::optional<elysia::core::Rect> resolve_camera_focus_rect() const override;
    void on_scene_object_registered(elysia::core::SceneObject& object) override;
    void on_scene_object_removing(elysia::core::SceneObject& object) override;

private:
    static void validate_payload(const RoomScenePayload& payload);
    void clear_room() noexcept;
    PlayerCharacter* _player = nullptr;
    RoomBuildResult _room;
    RoomBuildConfig _config;
    bool _active = false;
    ProjectileManager _projectiles;
    CombatSystem _combat{collision_runtime()};
};
