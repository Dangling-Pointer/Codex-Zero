#include "floating_number_effect_factory.h"

#include "../../typography/font_resolver.h"

#include <algorithm>
#include <cmath>

namespace elysia::effects
{
namespace
{
bool is_finite_vector(const elysia::core::Vector2& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y);
}
}

void FloatingNumberEffectFactory::set_runtime_dependencies(
	SDL_Renderer* renderer,
	const elysia::typography::FontResolver* font_resolver) noexcept
{
	if (_renderer == renderer && _font_resolver == font_resolver)
		return;

	_glyph_cache.reset();
	_renderer = renderer;
	_font_resolver = font_resolver;
}

void FloatingNumberEffectFactory::clear_cache() noexcept
{
	_glyph_cache.reset();
}

std::expected<std::unique_ptr<FloatingNumberEffect>, EffectRequestFailure> FloatingNumberEffectFactory::create(
	const FloatingNumberEffectSpawnRequest& request)
{
	if (request.text.empty())
	{
		return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
			"Create floating number effect failed: text is empty."));
	}

	if (!is_finite_vector(request.position))
		return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
			"Floating number position must be finite."));
	if (!std::isfinite(request.target_height) || request.target_height <= 0.0f)
		return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
			"Floating number target height must be finite and positive."));
	if (!std::isfinite(request.start_delay_seconds))
		return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
			"Floating number start delay must be finite."));
	if (!std::isfinite(request.time_scale) || request.time_scale < 0.0)
		return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
			"Floating number time scale must be finite and nonnegative."));
	if (!std::isfinite(request.lifetime_seconds) || request.lifetime_seconds <= 0.0)
		return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
			"Floating number lifetime must be finite and positive."));
	if (!FloatingNumberEffect::is_valid_effects(request.effects))
		return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
			"Floating number motion, scale, fade or effect time range is invalid."));

	for (const char ch : request.text)
	{
		if (!FloatingNumberGlyphCache::supports(ch))
		{
			return std::unexpected(make_effect_request_failure(EffectRequestError::InvalidRequest,
				"Floating number text contains an unsupported character: " + std::string(1, ch)));
		}
	}

	if (!_renderer)
	{
		return std::unexpected(make_effect_request_failure(EffectRequestError::RuntimeUnavailable,
			"Create floating number effect failed: renderer is unavailable."));
	}
	if (!_font_resolver)
	{
		return std::unexpected(make_effect_request_failure(EffectRequestError::RuntimeUnavailable,
			"Create floating number effect failed: FontResolver is unavailable."));
	}

	const auto resolved_font = _font_resolver->resolve_effect(
		elysia::typography::EffectTypographyRole::FloatingNumber);
	if (!resolved_font)
	{
		return std::unexpected(make_effect_request_failure(EffectRequestError::RuntimeUnavailable,
			"Floating number font resolution failed: " + resolved_font.error().message));
	}

	if (!_glyph_cache.configure(_renderer,resolved_font->font,resolved_font->generation))
	{
		return std::unexpected(make_effect_request_failure(EffectRequestError::RuntimeUnavailable,
			"Create floating number effect failed: glyph cache dependencies are unavailable."));
	}

	auto glyphs = _glyph_cache.resolve(request.text,request.color);
	if (!glyphs.has_value())
	{
		return std::unexpected(std::move(glyphs.error()));
	}

	std::unique_ptr<FloatingNumberEffect> effect = std::make_unique<FloatingNumberEffect>(
		std::move(*glyphs),request.position,request.alignment,request.target_height,
		request.lifetime_seconds,request.effects,request.on_finished);
	effect->set_start_delay(std::max(0.0,request.start_delay_seconds));
	effect->set_time_scale(request.time_scale);
	return effect;
}
}
