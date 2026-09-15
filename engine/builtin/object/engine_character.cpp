#include "engine_character.h"

#include "../resources/builtin_resources.h"
#include "../../core/render/colors.h"
#include "../../tools/debug_draw.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace elysia::builtin
{
namespace
{
constexpr elysia::core::Rect kDefaultWorldRect{0.0f, 0.0f, 96.0f, 96.0f};
constexpr elysia::core::Rect kLocalColliderRect{24.0f, 16.0f, 48.0f, 72.0f};

[[nodiscard]] bool is_pressed_event(
    elysia::input::RawInputEventType type) noexcept
{
    return type == elysia::input::RawInputEventType::ControlPressed;
}

[[nodiscard]] bool is_movement_event(
    elysia::input::RawInputEventType type) noexcept
{
    return type == elysia::input::RawInputEventType::ControlPressed
        || type == elysia::input::RawInputEventType::ControlReleased;
}
}

EngineCharacter::EngineCharacter()
    : GameObject(elysia::core::DepthLayer::Character)
{
    set_world_rect(kDefaultWorldRect);

    _collider.shape = elysia::physics::AabbShape{kLocalColliderRect};
    _collider.filter.category = 0;
    _collider.filter.mask = 0;
    _collider.response = elysia::physics::CollisionResponse::Ignore;

    if (!set_animations(
            BuiltinAnimationId::EngineCharacterIdle,
            BuiltinAnimationId::EngineCharacterMove))
    {
        throw std::logic_error(
            "EngineCharacter requires the built-in idle and move animations.");
    }
}

void EngineCharacter::update(double delta_seconds)
{
    const float horizontal =
        (_move_right_d || _move_right_arrow ? 1.0f : 0.0f)
        - (_move_left_a || _move_left_arrow ? 1.0f : 0.0f);
    const float vertical =
        (_move_down_s || _move_down_arrow ? 1.0f : 0.0f)
        - (_move_up_w || _move_up_arrow ? 1.0f : 0.0f);

    elysia::core::Vector2 direction{horizontal, vertical};
    set_moving(!direction.is_zero());

    if (horizontal < 0.0f)
        _flip = elysia::core::SpriteFlip::None;
    else if (horizontal > 0.0f)
        _flip = elysia::core::SpriteFlip::Horizontal;

    const double character_delta = scaled_delta(delta_seconds);
    const double safe_delta = std::isfinite(character_delta)
        && character_delta > 0.0
        ? character_delta
        : 0.0;

    if (_is_moving && safe_delta > 0.0)
    {
        direction.normalize_in_place();
        set_position(position()
            + direction * (kMovementSpeed * static_cast<float>(safe_delta)));
        clamp_to_movement_bounds();
    }

    elysia::animation::Animation* active_animation =
        _is_moving ? _move.get() : _idle.get();
    if (active_animation)
        active_animation->update(safe_delta);
}

bool EngineCharacter::on_raw_input_event(
    const elysia::input::RawInputEvent& event)
{
    if (!is_movement_event(event.type))
        return false;

    const bool pressed = is_pressed_event(event.type);
    switch (event.control)
    {
    case elysia::input::RawInputControl::KeyA:
        _move_left_a = pressed;
        return true;
    case elysia::input::RawInputControl::KeyLeft:
        _move_left_arrow = pressed;
        return true;
    case elysia::input::RawInputControl::KeyD:
        _move_right_d = pressed;
        return true;
    case elysia::input::RawInputControl::KeyRight:
        _move_right_arrow = pressed;
        return true;
    case elysia::input::RawInputControl::KeyW:
        _move_up_w = pressed;
        return true;
    case elysia::input::RawInputControl::KeyUp:
        _move_up_arrow = pressed;
        return true;
    case elysia::input::RawInputControl::KeyS:
        _move_down_s = pressed;
        return true;
    case elysia::input::RawInputControl::KeyDown:
        _move_down_arrow = pressed;
        return true;
    default:
        return false;
    }
}

void EngineCharacter::submit_render_commands(
    std::vector<elysia::core::RenderCommand>& out_commands) const
{
    const elysia::animation::Animation* active_animation =
        _is_moving ? _move.get() : _idle.get();
    if (active_animation)
    {
        (void)active_animation->append_render_commands(
            render_rect(),
            0.0,
            _flip,
            std::nullopt,
            out_commands);
    }
}

std::span<const elysia::physics::Collider>
EngineCharacter::colliders() const noexcept
{
    return std::span<const elysia::physics::Collider>(&_collider, 1);
}


bool EngineCharacter::set_animations(
    BuiltinAnimationId idle_id,
    BuiltinAnimationId move_id)
{
    std::unique_ptr<elysia::animation::Animation> idle =
        BuiltinResources::instance()->create_animation(idle_id);
    std::unique_ptr<elysia::animation::Animation> move =
        BuiltinResources::instance()->create_animation(move_id);
    if (!idle || !move)
        return false;

    idle->resume();
    move->pause();
    _idle = std::move(idle);
    _move = std::move(move);
    _is_moving = false;
    return true;
}

void EngineCharacter::set_movement_bounds(
    std::optional<elysia::core::Rect> bounds) noexcept
{
    if (bounds && bounds->is_empty())
        bounds.reset();
    _movement_bounds = std::move(bounds);
    clamp_to_movement_bounds();
}

const std::optional<elysia::core::Rect>&
EngineCharacter::movement_bounds() const noexcept
{
    return _movement_bounds;
}

void EngineCharacter::clear_movement_input() noexcept
{
    _move_left_a = false;
    _move_left_arrow = false;
    _move_right_d = false;
    _move_right_arrow = false;
    _move_up_w = false;
    _move_up_arrow = false;
    _move_down_s = false;
    _move_down_arrow = false;
    set_moving(false);
}

void EngineCharacter::submit_debug_draw() const
{
    if (!_collider.enabled)
        return;

    const auto* aabb = std::get_if<elysia::physics::AabbShape>(
        &_collider.shape);
    if (!aabb)
        return;

    elysia::tools::DebugDraw::instance()->draw_rect(
        elysia::tools::DebugDrawCategory::PhysicsCollider,
        aabb->local_rect.translated(position()),
        elysia::core::colors::green_500,
        2.0f);
}

void EngineCharacter::set_moving(bool moving) noexcept
{
    if (_is_moving == moving)
        return;

    elysia::animation::Animation* previous =
        _is_moving ? _move.get() : _idle.get();
    elysia::animation::Animation* next = moving ? _move.get() : _idle.get();
    if (previous)
        previous->pause();
    if (next)
        next->reset();
    _is_moving = moving;
}

void EngineCharacter::clamp_to_movement_bounds() noexcept
{
    if (!_movement_bounds)
        return;

    const elysia::core::Rect& bounds = *_movement_bounds;
    const elysia::core::Rect& character_rect = world_rect();
    elysia::core::Vector2 clamped_position = character_rect.position();

    if (character_rect.width() <= bounds.width())
    {
        clamped_position.x = std::clamp(
            clamped_position.x,
            bounds.left(),
            bounds.right() - character_rect.width());
    }
    else
    {
        clamped_position.x = bounds.center().x - character_rect.width() * 0.5f;
    }

    if (character_rect.height() <= bounds.height())
    {
        clamped_position.y = std::clamp(
            clamped_position.y,
            bounds.top(),
            bounds.bottom() - character_rect.height());
    }
    else
    {
        clamped_position.y = bounds.center().y - character_rect.height() * 0.5f;
    }

    set_position(clamped_position);
}
}
