#pragma once
#include "../../engine/gameplay/scene/gameplay_scene.h"
#include "../combat/wand/wand.h"
#include "../combat/projectile_manager.h"
#include "../enemy_generation/enemy_generator.h"
#include "../enemy_generation/enemy_spawn_layout.h"
#include <optional>
#include "game/combat/combat_system.h"

class PlayerCharacter;
class DungeonRoom;

class GameScene : public elysia::gameplay::GameplayScene
{
public:
	GameScene();
	~GameScene() noexcept override;

	void on_enter(const elysia::scene::ScenePayload &payload) override;
	void on_exit() override;
	void on_reset() override;
	void on_before_update(double delta) override;
	void on_shortcuts(const elysia::input::RawInputFrame &input,
				  const std::vector<elysia::input::RawInputEvent> &events) override;

protected:
    void on_game_fixed_update(std::uint64_t tick, double delta) override;
    [[nodiscard]] CombatSystem& combat_system() noexcept { return _combat; }
    
	void on_scene_object_registered(elysia::core::SceneObject& object) override;
    void on_control_target_removing(elysia::core::SceneObject& object) override;

	[[nodiscard]] std::optional<elysia::camera::CameraFocus> resolve_camera_focus(elysia::camera::CameraSlot slot) const override;

private:
	void clear_room() noexcept;

private:
	elysia::gameplay::ControllerHandle _player_controller;
	PlayerCharacter *_player = nullptr;
	DungeonRoom *_room = nullptr;

	// tmp
	Wand _test_wand;
	ProjectileManager _projectiles;
    CombatSystem _combat{collision_runtime()};
	EnemyGenerator _enemy_generator;
	EnemySpawnLayout _enemy_spawn_layout;
};
