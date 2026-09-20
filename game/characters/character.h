#pragma once

#include "../../engine/core/game_object.h"
#include "../../engine/core/interface/updatable.h"
#include "../../engine/physics/contracts/physics_participant.h"
#include "../../engine/physics/contracts/physics_step_participant.h"

#include <span>

class Character
	: public elysia::core::GameObject,
	  public elysia::core::Updatable,
	  public elysia::physics::PhysicsParticipant,
	  public elysia::physics::PhysicsStepParticipant
{

	enum class Facing
	{
		Left,
		Right,
	};

	enum class State
	{
		Alive,
		Dead
	};
	enum class AnimationState
	{
		Idle,
		Moving,
		Attack,
		Hurt,
		Death
	};

public:
	Character(elysia::core::Vector2 start_position) noexcept;
	~Character() override = default;

	void update(double delta) override;
	void fixed_update(double fixed_delta_seconds) override;

	void submit_render_commands(std::vector<elysia::core::RenderCommand> &out_commands) const override;

	[[nodiscard]] elysia::physics::BodyDefinition body_definition() const override;
	[[nodiscard]] std::span<const elysia::physics::Collider> collider_definitions() const override;

private:
	elysia::physics::Collider _body_collider;

	bool _facing_left = false;
	// elysia::gameplay::collision::TeamId team = elysia::gameplay::collision::teams::Neutral;
	//  elysia::core::Vector2 _desired_velocity = elysia::core::Vector2::zero();
	elysia::core::Rect _collision_rect{};
	float _move_speed = 240.0f;
	float _hp = 100.0f;
	bool _is_dead = false;
};
