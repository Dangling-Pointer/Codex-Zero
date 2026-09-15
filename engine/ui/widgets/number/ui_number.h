#pragma once

#include "../../style/ui_style.h"
#include "../../style/ui_visual_styles.h"
#include "../../core/ui_element.h"
#include "../../core/ui_text_align.h"
#include "../../text/ui_typography.h"
#include "../../../typography/font_settings.h"

#include <optional>
#include <string>

namespace elysia::ui
{
enum class UiNumberSuffix
{
    None,
    Percent
};

// Numeric text widget with per-glyph layout backed by the localization texture cache.
class UiNumber : public UiElement
{
public:
    explicit UiNumber(const elysia::core::Rect& rect = elysia::core::Rect::zero(), int order = 0) noexcept;

    UiNumber(
        const elysia::core::Vector2& position,
        const elysia::core::Vector2& size,
        int order = 0
    ) noexcept;

    UiNumber(
        const elysia::core::Vector2& center,
        const elysia::core::Vector2& size,
        UiFromCenterTag,
        int order = 0
    ) noexcept;

    ~UiNumber() override = default;

    void reset() noexcept override;
    void submit_ui_render_commands(std::vector<elysia::core::UiRenderCommand>& out_commands) const override;

    // Replaces the numeric value used to generate the rendered digit string.
    void set_value(double value);
    [[nodiscard]] double value() const noexcept;

    void set_base_style(const UiNumberStyle& style) noexcept;
    void set_style_overrides(const UiNumberStyleOverrides& overrides) noexcept;
    [[nodiscard]] const UiNumberStyle& style() const noexcept;
    [[nodiscard]] const UiNumberStyleOverrides& style_overrides() const noexcept;
    [[nodiscard]] bool has_style_overrides() const noexcept;
    void clear_style_overrides() noexcept;

    void set_horizontal_align(TextHorizontalAlign align);
    [[nodiscard]] TextHorizontalAlign horizontal_align() const noexcept;

    void set_vertical_align(TextVerticalAlign align);
    [[nodiscard]] TextVerticalAlign vertical_align() const noexcept;

    void set_typography_role(
        elysia::typography::UiTypographyRole role) noexcept;
    [[nodiscard]] elysia::typography::UiTypographyRole
        typography_role() const noexcept;
    void set_font_source_override(
        elysia::typography::FontSource source) noexcept;
    void clear_font_source_override() noexcept;
    [[nodiscard]] std::optional<elysia::typography::FontSource>
        font_source_override() const noexcept;

    void set_padding(int padding);
    [[nodiscard]] int padding() const noexcept;

    void set_digit_spacing(float spacing);
    [[nodiscard]] float digit_spacing() const noexcept;

    // Optional fixed advance keeps columns stable while values change.
    void set_fixed_glyph_advance(float advance);
    [[nodiscard]] std::optional<float> fixed_glyph_advance() const noexcept;
    void clear_fixed_glyph_advance();

    // Scales glyphs uniformly without requesting a differently loaded font.
    void set_target_height(float height);
    [[nodiscard]] std::optional<float> target_height() const noexcept;
    void clear_target_height();

    void set_decimal_places(int decimal_places);
    [[nodiscard]] int decimal_places() const noexcept;

    void set_trim_trailing_zeros(bool trim_trailing_zeros);
    [[nodiscard]] bool trims_trailing_zeros() const noexcept;

    void set_keep_decimal_point(bool keep_decimal_point);
    [[nodiscard]] bool keeps_decimal_point() const noexcept;

    void set_suffix(UiNumberSuffix suffix);
    [[nodiscard]] UiNumberSuffix suffix() const noexcept;

private:
    // Returns the padded interior used to position rendered digits.
    [[nodiscard]] elysia::core::Rect content_rect() const noexcept;
    // Formats the current numeric value using suffix and decimal display settings.
    [[nodiscard]] std::string formatted_text() const;
    // Normalizes trailing fractional zeros while preserving optional decimal point output.
    [[nodiscard]] static std::string trim_fractional_zeros(
        std::string text,
        bool keep_decimal_point
    );
private:
    double _value = 0.0;
    UiStyleState<UiNumberStyle> _style_state;
    elysia::typography::UiTypographyRole _typography_role =
        elysia::typography::UiTypographyRole::Number;
    std::optional<elysia::typography::FontSource> _font_source_override;
    TextHorizontalAlign _horizontal_align = TextHorizontalAlign::Left;
    TextVerticalAlign _vertical_align = TextVerticalAlign::Top;
    int _padding = 0;
    float _digit_spacing = 0.0f;
    std::optional<float> _fixed_glyph_advance;
    std::optional<float> _target_height;
    int _decimal_places = 0;
    bool _trim_trailing_zeros = true;
    bool _keep_decimal_point = false;
    UiNumberSuffix _suffix = UiNumberSuffix::None;
};
}
