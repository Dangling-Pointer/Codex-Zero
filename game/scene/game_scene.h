#pragma once
#include "../../engine/gameplay/scene/gameplay_scene.h"
#include "../combat/wand/wand.h"
#include <optional>

class PlayerCharacter;
class DungeonRoom;

class GameScene : public elysia::gameplay::GameplayScene
{
public:
	GameScene() = default;
	~GameScene() noexcept override = default;

	void on_enter(const elysia::scene::ScenePayload &payload) override;
	void on_exit() override;
	void reset() override;
	void on_input(const elysia::input::RawInputFrame &input,
				  const std::vector<elysia::input::RawInputEvent> &events) override;

protected:
	[[nodiscard]] std::optional<elysia::core::Rect> resolve_camera_focus_rect() const override;

private:
	void clear_room() noexcept;

private:
	PlayerCharacter *_player = nullptr;
	DungeonRoom *_room = nullptr;

	// tmp
	void fire_test_wand();
	Wand _test_wand;
};
