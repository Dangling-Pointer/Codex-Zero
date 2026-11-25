#pragma once

#include "engine/gameplay/scene/gameplay_scene.h"
#include "engine/physics/tile/tile_collision_world.h"
#include "game/combat/combat_system.h"
#include "game/combat/projectile_manager.h"
#include "room_scene_payload.h"
#include "game/combat/wand/wand.h"
#include "game/map/room_corridor_layout.h"
#include "game/map/dungeon_room.h"
#include "game/map/room_query.h"

class PlayerCharacter;
class Portal;
class FloorMinimapHud;
class Gate;

// Observers of scene-owned objects; ownership stays with Scene.
struct RoomBuildResult
{
    DungeonRoom* map = nullptr;
    elysia::physics::ITileCollisionWorld* collision_world = nullptr;
    elysia::core::Vector2 player_spawn;
    std::vector<game::RoomCorridorLayout> corridors;
    bool corridor_only = false;
};

class RoomScene : public elysia::gameplay::GameplayScene
{
public:
    RoomScene();
    ~RoomScene() noexcept override;
    void on_enter(const elysia::scene::ScenePayload& payload) final;
    void on_exit() final;
    void on_reset() final;
    void on_before_update(double delta) override;
    void on_after_update(double delta) override;
    [[nodiscard]] std::optional<CharacterContext> capture_character_context() const;

protected:
    void on_game_fixed_update(std::uint64_t tick, double delta) override;
    void on_shortcuts(const elysia::input::RawInputFrame&,
                      const std::vector<elysia::input::RawInputEvent>& events) override;

    virtual RoomBuildResult build_room(const RoomBuildConfig& config) = 0;

    virtual void create_room_content() {}
    [[nodiscard]] virtual bool should_create_portal() const noexcept { return true; }
    [[nodiscard]] virtual bool should_create_gates() const noexcept { return true; }
    [[nodiscard]] virtual bool can_leave_room() const { return true; }

    [[nodiscard]] const Room* run_room() const;
    [[nodiscard]] const std::optional<CorridorTraversal>& corridor_traversal() const noexcept { return _corridor; }

    [[nodiscard]] const elysia::core::GameObject* room_map() const noexcept {return _room.map;}
    [[nodiscard]] const game::RoomQuery& room_query() const noexcept { return *_room_query; }

    [[nodiscard]] PlayerCharacter* player() const noexcept { return _player; }
    [[nodiscard]] Portal* portal() const noexcept { return _portal; }
    [[nodiscard]] const RoomBuildConfig& room_config() const noexcept { return _config; }
    [[nodiscard]] CombatSystem& combat_system() noexcept { return _combat; }

    [[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus(
        elysia::camera::CameraSlot slot) const override;

    void on_scene_object_registered(elysia::core::SceneObject& object) override;
    void on_control_target_removing(elysia::core::SceneObject& object) override;

private:
    void build_room_objects(const CharacterContext& character);
    void clear_room() noexcept;
    void leave_through_portal();
    [[nodiscard]] bool try_leave_through_boundary();
    void update_minimap();
    void create_corridor_gates();
    void update_gates();

    std::optional<RoomID> _room_id;
    std::optional<RoomID> _from_room;
    std::optional<CorridorTraversal> _corridor;

    elysia::gameplay::ControllerHandle _player_controller;
    PlayerCharacter* _player = nullptr;

    Portal* _portal = nullptr;
    FloorMinimapHud* _minimap = nullptr;
    std::vector<Gate*> _gates; // Scene-owned observers, like the player and portal.

    RoomBuildResult _room;
    std::optional<game::RoomQuery> _room_query;
    RoomBuildConfig _config;

    bool _active = false;
    bool _transition_requested = false;
    double _start_gate_delay = 0;
    elysia::core::Vector2 _previous_player_center;

    Wand _test_wand;
    ProjectileManager _projectiles;
    CombatSystem _combat{collision_runtime()};
};
