#pragma once
#include "../../engine/gameplay/scene/gameplay_scene.h"
#include "../combat/wand/wand.h"
#include "../combat/projectile_manager.h"
#include <optional>
#include "game/combat/combat_system.h"

class PlayerCharacter;
class Enemy;
class DungeonRoom;

class GameScene : public elysia::gameplay::GameplayScene
{
public:
	GameScene() = default;
	~GameScene() noexcept override;

	void on_enter(const elysia::scene::ScenePayload &payload) override;
	void on_exit() override;
	void reset() override;
	void on_update(double delta) override;
	void on_input(const elysia::input::RawInputFrame &input,
				  const std::vector<elysia::input::RawInputEvent> &events) override;

protected:
    [[nodiscard]] CombatSystem& combat_system() noexcept { return _combat; }
    
	//TODO: Refactor detailed lifecycle management in the next engine update.
	//will be move down to engine next engine update
	void on_scene_object_registered(elysia::core::SceneObject& object) override;
    void on_scene_object_removing(elysia::core::SceneObject& object) override;
	//will be move down to engine next engine update

	[[nodiscard]] std::optional<elysia::core::Rect> resolve_camera_focus_rect() const override;

private:
	void clear_room() noexcept;

private:
	PlayerCharacter *_player = nullptr;
	Enemy *_enemy = nullptr;
	DungeonRoom *_room = nullptr;

	// tmp
	void fire_test_wand();
	Wand _test_wand;
	ProjectileManager _projectiles;
    CombatSystem _combat{collision_runtime()};
};
