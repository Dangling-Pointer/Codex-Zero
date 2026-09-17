#pragma once

#include "../../engine/core/game_object.h"
#include "../../engine/core/interface/updatable.h"
#include "../../engine/physics/contracts/physics_participant.h"
#include "../../engine/physics/contracts/physics_step_participant.h"

#include <span>

namespace game::characters
{
class Character
	: public elysia::core::GameObject
	, public elysia::core::Updatable
	, public elysia::physics::PhysicsParticipant
	, public elysia::physics::PhysicsStepParticipant
{
public:
	Character(elysia::core::Vector2 start_position)noexcept ;
	~Character()override = default;

	void update(double delta) override;
	void fixed_update(double fixed_delta_seconds) override;

	void submit_render_commands(std::vector<elysia::core::RenderCommand>& out_commands) const override;

	[[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
	[[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

private:
	elysia::physics::Collider _body_collider;
	bool _facing_left = false;
};

}
