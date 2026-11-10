#pragma once

#include "../effect_request_failure.h"
#include "../effect_types.h"

#include <expected>
#include <memory>

namespace elysia::effects
{
class AnimationEffectFactory
{
public:
	[[nodiscard]] std::expected<std::unique_ptr<AnimationEffect>, EffectRequestFailure> create(
		const AnimationEffectSpawnRequest& request,
		const AnimationEffectDefinition& definition) const;
};
}
