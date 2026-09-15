#pragma once

#include "effect_types.h"
#include "../tools/singleton.h"

#define ELYSIA_EFFECTS (::elysia::effects::EffectService::instance())

namespace elysia::effects
{
class EffectService final : public elysia::tools::Singleton<EffectService>
{
	friend elysia::tools::Singleton<EffectService>;

public:
	[[nodiscard]] bool request_animation_effect(
		const AnimationEffectSpawnRequest& request);
	[[nodiscard]] bool request_floating_number_effect(
		const FloatingNumberEffectSpawnRequest& request);

private:
	EffectService() = default;
};
}
