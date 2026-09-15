#include "engine/core/render/sdl_texture_size.h"
#include "ui_number.h"

#include "../../style/ui_style_defaults.h"
#include "../../../core/render/glyph_run_layout.h"
#include "../../../core/render/render_command.h"
#include "../../../localization/localization_service.h"
#include "../../../localization/localized_text_style.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace elysia::ui
{
using elysia::typography::UiTypographyRole;

namespace
{
constexpr double kNegativeZeroTolerance = 1e-9;
}

UiNumber::UiNumber(const elysia::core::Rect& rect,int order) noexcept
    : UiElement(rect,order)
{
    reset();
}

UiNumber::UiNumber(
    const elysia::core::Vector2& position,
    const elysia::core::Vector2& size,
    int order
) noexcept
    : UiElement(position,size,order)
{
    reset();
}

UiNumber::UiNumber(
    const elysia::core::Vector2& center,
    const elysia::core::Vector2& size,
    UiFromCenterTag,
    int order
) noexcept
    : UiElement(center,size,from_center,order)
{
    reset();
}

void UiNumber::reset() noexcept
{
    UiElement::reset();
    _value = 0.0;
    _style_state.reset(UiStyleDefaults::number());
    _typography_role = UiTypographyRole::Number;
    _font_source_override.reset();
    _horizontal_align = TextHorizontalAlign::Left;
    _vertical_align = TextVerticalAlign::Top;
    _padding = 0;
    _digit_spacing = 0.0f;
    _fixed_glyph_advance.reset();
    _target_height.reset();
    _decimal_places = 0;
    _trim_trailing_zeros = true;
    _keep_decimal_point = false;
    _suffix = UiNumberSuffix::None;
}

void UiNumber::submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const
{
    if (!is_visible())
        return;

    const elysia::core::Rect& number_rect = screen_rect();
    if (number_rect.is_empty())
        return;

    const UiNumberStyle& style = _style_state.effective_style();
    if (style.draw_background)
        out_commands.push_back(elysia::core::make_ui_fill_rect_command(number_rect,apply_opacity(style.background),style.corner_radius));

    const std::string text = formatted_text();
    if (text.empty())
        return;

    const elysia::core::Rect available_rect = content_rect();
    if (available_rect.is_empty())
        return;

    elysia::localization::LocalizationService* localization_service =
        ELYSIA_LOCALIZATION;

    const UiResolvedTextStyle typography = resolve_ui_typography(_typography_role);
    elysia::localization::LocalizedTextStyle text_style;
    text_style.typography_role = _typography_role;
    text_style.font_source_override = _font_source_override;
    text_style.color = style.text;

    std::vector<SDL_Texture*> textures;
    std::vector<elysia::core::Vector2> source_sizes;
    textures.reserve(text.size());
    source_sizes.reserve(text.size());
    for (const char ch : text)
    {
        SDL_Texture* texture = localization_service->get_raw_text_texture(
            std::string_view(&ch,1),
            text_style
        );
        if (!texture)
            continue;

        int texture_width = 0;
        int texture_height = 0;
        if (!elysia::core::texture_pixel_size(texture,&texture_width,&texture_height)
            || texture_width <= 0
            || texture_height <= 0)
        {
            continue;
        }

        textures.push_back(texture);
        source_sizes.emplace_back(
            static_cast<float>(texture_width),
            static_cast<float>(texture_height)
        );
    }

    const float target_height = _target_height.has_value()
        ? *_target_height
        : available_rect.height();
    const elysia::core::GlyphRunLayout layout = elysia::core::layout_glyph_run(
        source_sizes,
        elysia::core::GlyphRunLayoutOptions{
            .target_height = target_height,
            .spacing = _digit_spacing,
            .fixed_advance = _fixed_glyph_advance
        }
    );
    if (layout.glyphs.empty())
        return;

    float origin_x = available_rect.x();
    switch (_horizontal_align)
    {
    case TextHorizontalAlign::Center:
        origin_x = available_rect.center().x - layout.width * 0.5f;
        break;
    case TextHorizontalAlign::Right:
        origin_x = available_rect.right() - layout.width;
        break;
    case TextHorizontalAlign::Left:
    default:
        origin_x = available_rect.x();
        break;
    }

    for (const elysia::core::GlyphPlacement& placement : layout.glyphs)
    {
        if (placement.glyph_index >= textures.size())
            continue;

        float render_y = available_rect.y();
        switch (_vertical_align)
        {
        case TextVerticalAlign::Center:
            render_y = available_rect.center().y - placement.local_rect.height() * 0.5f;
            break;
        case TextVerticalAlign::Bottom:
            render_y = available_rect.bottom() - placement.local_rect.height();
            break;
        case TextVerticalAlign::Top:
        default:
            render_y = available_rect.y();
            break;
        }

        elysia::core::UiRenderCommand command = elysia::core::make_ui_texture_command(
            textures[placement.glyph_index],
            elysia::core::Rect(
                origin_x + placement.local_rect.x(),
                render_y,
                placement.local_rect.width(),
                placement.local_rect.height()
            )
        );
        apply_opacity(command);
        out_commands.push_back(command);
    }
}

void UiNumber::set_value(double value)
{
    _value = value;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

double UiNumber::value() const noexcept
{
    return _value;
}

void UiNumber::set_base_style(const UiNumberStyle& style) noexcept
{
    _style_state.set_base_style(style);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiNumber::set_style_overrides(const UiNumberStyleOverrides& overrides) noexcept
{
    _style_state.set_style_overrides(overrides);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

const UiNumberStyle& UiNumber::style() const noexcept
{
    return _style_state.effective_style();
}

const UiNumberStyleOverrides& UiNumber::style_overrides() const noexcept { return _style_state.style_overrides(); }
bool UiNumber::has_style_overrides() const noexcept
{
    return _style_state.has_style_overrides();
}

void UiNumber::clear_style_overrides() noexcept
{
    _style_state.clear_style_overrides();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiNumber::set_horizontal_align(TextHorizontalAlign align)
{
    _horizontal_align = align;
}

TextHorizontalAlign UiNumber::horizontal_align() const noexcept
{
    return _horizontal_align;
}

void UiNumber::set_vertical_align(TextVerticalAlign align)
{
    _vertical_align = align;
}

TextVerticalAlign UiNumber::vertical_align() const noexcept
{
    return _vertical_align;
}

void UiNumber::set_typography_role(UiTypographyRole role) noexcept
{
    _typography_role = role;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiTypographyRole UiNumber::typography_role() const noexcept
{
    return _typography_role;
}

void UiNumber::set_font_source_override(
    elysia::typography::FontSource source) noexcept
{
    if (_font_source_override == source)
        return;
    _font_source_override = source;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiNumber::clear_font_source_override() noexcept
{
    if (!_font_source_override)
        return;
    _font_source_override.reset();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<elysia::typography::FontSource>
UiNumber::font_source_override() const noexcept
{
    return _font_source_override;
}

void UiNumber::set_padding(int padding)
{
    _padding = std::max(0,padding);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

int UiNumber::padding() const noexcept
{
    return _padding;
}

void UiNumber::set_digit_spacing(float spacing)
{
    _digit_spacing = std::max(0.0f,spacing);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

float UiNumber::digit_spacing() const noexcept
{
    return _digit_spacing;
}

void UiNumber::set_fixed_glyph_advance(float advance)
{
    _fixed_glyph_advance = std::max(0.0f,advance);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<float> UiNumber::fixed_glyph_advance() const noexcept
{
    return _fixed_glyph_advance;
}

void UiNumber::clear_fixed_glyph_advance()
{
    _fixed_glyph_advance.reset();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiNumber::set_target_height(float height)
{
    _target_height = std::max(0.0f,height);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

std::optional<float> UiNumber::target_height() const noexcept
{
    return _target_height;
}

void UiNumber::clear_target_height()
{
    _target_height.reset();
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

void UiNumber::set_decimal_places(int decimal_places)
{
    _decimal_places = std::max(0,decimal_places);
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

int UiNumber::decimal_places() const noexcept
{
    return _decimal_places;
}

void UiNumber::set_trim_trailing_zeros(bool trim_trailing_zeros)
{
    _trim_trailing_zeros = trim_trailing_zeros;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

bool UiNumber::trims_trailing_zeros() const noexcept
{
    return _trim_trailing_zeros;
}

void UiNumber::set_keep_decimal_point(bool keep_decimal_point)
{
    _keep_decimal_point = keep_decimal_point;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

bool UiNumber::keeps_decimal_point() const noexcept
{
    return _keep_decimal_point;
}

void UiNumber::set_suffix(UiNumberSuffix suffix)
{
    _suffix = suffix;
    notify_layout_parent_of_intrinsic_layout_invalidation();
}

UiNumberSuffix UiNumber::suffix() const noexcept
{
    return _suffix;
}

elysia::core::Rect UiNumber::content_rect() const noexcept
{
    const elysia::core::Rect& number_rect = screen_rect();
    const float width = std::max(0.0f,number_rect.width());
    const float height = std::max(0.0f,number_rect.height());
    const float padding = static_cast<float>(_padding);
    const float pad_x = std::min(padding,width * 0.5f);
    const float pad_y = std::min(padding,height * 0.5f);

    elysia::core::Rect content = number_rect;
    content.set_x(number_rect.x() + pad_x);
    content.set_y(number_rect.y() + pad_y);
    content.set_width(width - pad_x * 2.0f);
    content.set_height(height - pad_y * 2.0f);
    return content;
}

std::string UiNumber::formatted_text() const
{
    double display_value = _value;
    if (std::abs(display_value) < kNegativeZeroTolerance)
        display_value = 0.0;

    std::ostringstream stream;
    stream << std::fixed << std::setprecision(_decimal_places) << display_value;
    std::string text = stream.str();

    if (_trim_trailing_zeros)
        text = trim_fractional_zeros(std::move(text),_keep_decimal_point);

    if (_suffix == UiNumberSuffix::Percent)
        text.push_back('%');

    return text;
}

std::string UiNumber::trim_fractional_zeros(std::string text,bool keep_decimal_point)
{
    if (text.find('.') == std::string::npos)
        return text;

    while (!text.empty() && text.back() == '0')
        text.pop_back();

    if (!text.empty() && text.back() == '.')
    {
        if (!keep_decimal_point)
            text.pop_back();
    }

    if (text == "-0" || text == "-0.")
        return keep_decimal_point && !text.empty() && text.back() == '.' ? "0." : "0";

    return text;
}

}
