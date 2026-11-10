#pragma once

#include "../effect_request_failure.h"
#include "../effect_types.h"
#include "floating_number_glyph_cache.h"

#include <expected>
#include <memory>

struct SDL_Renderer;

namespace elysia::typography
{
class FontResolver;
}

namespace elysia::effects
{
class FloatingNumberEffectFactory
{
public:
	void set_runtime_dependencies(
		SDL_Renderer* renderer,
		const elysia::typography::FontResolver* font_resolver) noexcept;
	void clear_cache() noexcept;

	[[nodiscard]] std::expected<std::unique_ptr<FloatingNumberEffect>, EffectRequestFailure> create(
		const FloatingNumberEffectSpawnRequest& request);

private:
	FloatingNumberGlyphCache _glyph_cache;
	SDL_Renderer* _renderer = nullptr;
	const elysia::typography::FontResolver* _font_resolver = nullptr;
};
}
