#pragma once

#include "../animation/animation_effect_factory.h"
#include "../effect_types.h"
#include "../number/floating_number_effect_factory.h"
#include "../../resources/resource_types.h"
#include "../../tools/singleton.h"
#include "../effect_registration_failure.h"

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace elysia::scene
{
class Scene;
class SceneManager;
}

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::effects
{
class EffectService;

class EffectManager : public elysia::tools::Singleton<EffectManager>
{
	friend elysia::tools::Singleton<EffectManager>;
	friend class EffectService;
	friend class elysia::scene::SceneManager;

public:
	void set_runtime_dependencies(
		SDL_Renderer* renderer,
		const elysia::typography::FontResolver* font_resolver) noexcept;

	[[nodiscard]] std::expected<void,EffectRegistrationFailure> register_animation_effect(
		const elysia::resources::AnimationEffectBuildRequest& request);
	[[nodiscard]] std::expected<void,EffectRegistrationFailure> register_animation_effect(
		const std::vector<elysia::resources::AnimationEffectBuildRequest>& requests);

	[[nodiscard]] const AnimationEffectDefinition* find_animation_effect_definition(
		std::string_view key) const;
	void clear_content() noexcept;

private:
	[[nodiscard]] bool dispatch(const AnimationEffectSpawnRequest& request);
	[[nodiscard]] bool dispatch(const FloatingNumberEffectSpawnRequest& request);

	void bind_active_scene(elysia::scene::Scene& scene) noexcept;
	void unbind_active_scene(const elysia::scene::Scene& scene) noexcept;

	std::unordered_map<std::string,AnimationEffectDefinition> _animation_effect_definitions;
	AnimationEffectFactory _animation_effect_factory;
	FloatingNumberEffectFactory _floating_number_effect_factory;
	elysia::scene::Scene* _active_scene = nullptr;
};
}
