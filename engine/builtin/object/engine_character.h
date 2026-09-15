#pragma once

#include "../../animation/animation.h"
#include "../../core/game_object.h"
#include "../../core/interface/updatable.h"
#include "../../input/contracts/raw_input_event_receiver.h"
#include "../../physics/collision/collider.h"
#include "../resources/builtin_resource_ids.h"

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace elysia::builtin
{
class EngineCharacter final : public elysia::core::GameObject,
    public elysia::input::RawInputEventReceiver,
    public elysia::core::Updatable
{
public:
    static constexpr float kMovementSpeed = 180.0f;

    EngineCharacter();
    ~EngineCharacter() override = default;

    void update(double delta_seconds) override;
    bool on_raw_input_event(
        const elysia::input::RawInputEvent& event) override;
    void submit_render_commands(
        std::vector<elysia::core::RenderCommand>& out_commands) const override;

    [[nodiscard]] std::span<const elysia::physics::Collider> colliders() const noexcept;

    [[nodiscard]] bool set_animations(
        BuiltinAnimationId idle_id,
        BuiltinAnimationId move_id);

    void set_movement_bounds(
        std::optional<elysia::core::Rect> bounds) noexcept;
    [[nodiscard]] const std::optional<elysia::core::Rect>&
        movement_bounds() const noexcept;

    void clear_movement_input() noexcept;
    void submit_debug_draw() const;

private:
    void set_moving(bool moving) noexcept;
    void clamp_to_movement_bounds() noexcept;

    std::unique_ptr<elysia::animation::Animation> _idle;
    std::unique_ptr<elysia::animation::Animation> _move;
    elysia::physics::Collider _collider;
    std::optional<elysia::core::Rect> _movement_bounds;
    elysia::core::SpriteFlip _flip = elysia::core::SpriteFlip::None;
    bool _is_moving = false;
    bool _move_left_a = false;
    bool _move_left_arrow = false;
    bool _move_right_d = false;
    bool _move_right_arrow = false;
    bool _move_up_w = false;
    bool _move_up_arrow = false;
    bool _move_down_s = false;
    bool _move_down_arrow = false;
};
}
